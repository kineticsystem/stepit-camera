// Copyright 2026 Giovanni Remigi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stepit_camera/camera_node.hpp"

#include <functional>
#include <sstream>
#include <utility>

#include "stepit_camera/gphoto_camera.hpp"

namespace stepit_camera
{

namespace
{

std::string join(const std::vector<std::string>& texts)
{
  std::string joined;
  for (const auto& text : texts)
  {
    joined += (joined.empty() ? "" : ", ") + text;
  }
  return joined;
}

const Setting* findSetting(const std::string& parameter)
{
  for (const auto& setting : SETTINGS)
  {
    if (parameter == setting.parameter)
    {
      return &setting;
    }
  }
  return nullptr;
}

}  // namespace

std::optional<std::string> parameterText(const rclcpp::ParameterValue& value)
{
  switch (value.get_type())
  {
    case rclcpp::ParameterType::PARAMETER_NOT_SET:
      return std::string();
    case rclcpp::ParameterType::PARAMETER_STRING:
      return value.get<std::string>();
    case rclcpp::ParameterType::PARAMETER_INTEGER:
      return std::to_string(value.get<int64_t>());
    case rclcpp::ParameterType::PARAMETER_DOUBLE:
    {
      std::ostringstream text;
      text << value.get<double>();
      return text.str();
    }
    default:
      return std::nullopt;
  }
}

CameraNode::CameraNode(const rclcpp::NodeOptions& options) : rclcpp::Node("camera", options)
{
  const auto read_only = [](const std::string& description) {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.description = description;
    descriptor.read_only = true;
    return descriptor;
  };

  const bool fake_camera =
      declare_parameter("fake_camera", false, read_only("Run a fake camera instead of the one connected over USB."));
  frame_id_ = declare_parameter("frame_id", "camera", read_only("The frame of the images."));
  download_directory_ = declare_parameter("download_directory", "~/pictures", read_only("Where to save the pictures."));
  // A picture reaches the other nodes as a file: without a folder to save it
  // into, it would reach nobody.
  if (download_directory_.empty())
  {
    throw std::invalid_argument("The parameter download_directory must name a folder to save the pictures into");
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.description =
        "The subfolder of download_directory the next pictures go into, e.g. 2026-10-06/angle_01; empty for "
        "download_directory itself. It can change while the camera runs.";
    const auto folder = pictureFolder(declare_parameter("folder", "", descriptor));
    if (!folder)
    {
      throw std::invalid_argument("The parameter folder must be a relative path inside download_directory");
    }
    folder_ = *folder;
  }
  keep_on_camera_ = declare_parameter(
      "keep_on_camera", true,
      read_only("Store the pictures on the memory card, or only in the camera's memory until they are downloaded."));
  const bool stream_on_start =
      declare_parameter("stream_on_start", false, read_only("Start the live view as soon as the camera connects."));

  DriverOptions driver_options;
  driver_options.keep_on_camera = keep_on_camera_;
  driver_options.preview_rate =
      declare_parameter("preview_rate", 10.0, read_only("The frames of the live view per second, at most."));
  driver_options.reconnect_period = std::chrono::milliseconds(static_cast<int64_t>(
      1000 * declare_parameter("reconnect_period", 2.0,
                               read_only("How long to wait, in seconds, before trying to open the camera again."))));

  for (const auto& setting : SETTINGS)
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.description = std::string(setting.description) + " Empty leaves the camera as it is.";
    descriptor.dynamic_typing = true;
    const auto value = declare_parameter(setting.parameter, rclcpp::ParameterValue(std::string()), descriptor);
    const auto text = parameterText(value);
    if (!text)
    {
      throw std::invalid_argument(std::string("The parameter ") + setting.parameter + " must be a string or a number");
    }
    wanted_settings_[setting.parameter] = *text;
  }

  preview_publisher_ = create_publisher<sensor_msgs::msg::CompressedImage>("~/preview/compressed", rclcpp::QoS(1));
  picture_publisher_ = create_publisher<stepit_camera_msgs::msg::Picture>("~/picture", rclcpp::QoS(10));

  using std::placeholders::_1;
  using std::placeholders::_2;
  start_streaming_service_ =
      create_service<std_srvs::srv::Trigger>("~/start_streaming", std::bind(&CameraNode::startStreaming, this, _1, _2));
  stop_streaming_service_ =
      create_service<std_srvs::srv::Trigger>("~/stop_streaming", std::bind(&CameraNode::stopStreaming, this, _1, _2));
  get_settings_service_ = create_service<stepit_camera_msgs::srv::GetSettings>(
      "~/get_settings", std::bind(&CameraNode::getSettings, this, _1, _2));
  take_picture_service_ =
      create_service<std_srvs::srv::Trigger>("~/take_picture", std::bind(&CameraNode::takePicture, this, _1, _2));

  std::unique_ptr<Camera> camera;
  if (fake_camera)
  {
    camera = std::make_unique<FakeCamera>();
    RCLCPP_INFO(get_logger(), "Running a fake camera");
  }
  else
  {
    camera = std::make_unique<GPhotoCamera>();
  }

  CameraDriver::Callbacks callbacks;
  callbacks.on_connected = [this](Camera& connected) { onConnected(connected); };
  callbacks.on_disconnected = [this](const std::string& reason) {
    RCLCPP_WARN(get_logger(), "Camera disconnected: %s", reason.c_str());
  };
  callbacks.on_preview = [this](std::vector<uint8_t>&& frame) { onPreview(std::move(frame)); };
  callbacks.on_picture = [this](const CameraFile& file, std::vector<uint8_t>&& data) {
    onPicture(file, std::move(data));
  };
  callbacks.on_warning = [this](const std::string& message) { RCLCPP_WARN(get_logger(), "%s", message.c_str()); };

  driver_ = std::make_unique<CameraDriver>(std::move(camera), driver_options, std::move(callbacks));
  driver_->setStreaming(stream_on_start);

  parameters_callback_ = add_on_set_parameters_callback(std::bind(&CameraNode::onSetParameters, this, _1));

  if (!fake_camera)
  {
    RCLCPP_INFO(get_logger(), "Waiting for a camera");
  }
}

void CameraNode::onConnected(Camera& camera)
{
  RCLCPP_INFO(get_logger(), "Camera connected: %s", camera.model().c_str());

  // Where the camera stores a picture. Pictures in its memory only are lost if
  // they cannot be downloaded, but they do not fill the card up.
  try
  {
    const auto target = captureTargetChoice(keep_on_camera_, camera.getChoices(CAPTURE_TARGET));
    if (target)
    {
      camera.setSetting(CAPTURE_TARGET, *target);
    }
  }
  catch (const CameraError& error)
  {
    if (error.isFatal())
    {
      throw;
    }
    RCLCPP_WARN(get_logger(), "%s", error.what());
  }

  std::map<std::string, std::string> wanted;
  {
    std::lock_guard<std::mutex> lock(wanted_settings_mutex_);
    wanted = wanted_settings_;
  }
  std::string current;
  for (const auto& setting : SETTINGS)
  {
    try
    {
      const auto& value = wanted[setting.parameter];
      if (!value.empty())
      {
        applySetting(camera, setting, value);
      }
      current += std::string(current.empty() ? "" : ", ") + setting.parameter + "=" + camera.getSetting(setting.config);
    }
    catch (const CameraError& error)
    {
      if (error.isFatal())
      {
        throw;
      }
      RCLCPP_WARN(get_logger(), "%s", error.what());
    }
  }
  RCLCPP_INFO(get_logger(), "Camera settings: %s", current.c_str());
}

void CameraNode::applySetting(Camera& camera, const Setting& setting, const std::string& value)
{
  const auto choices = camera.getChoices(setting.config);
  std::string choice = value;
  if (!choices.empty())
  {
    const auto match = matchChoice(value, choices);
    if (!match)
    {
      throw CameraError(std::string("The camera does not accept ") + setting.parameter + " " + value +
                        ", only one of: " + join(choices));
    }
    choice = *match;
  }
  if (camera.getSetting(setting.config) != choice)
  {
    camera.setSetting(setting.config, choice);
  }
  RCLCPP_INFO(get_logger(), "Set %s to %s", setting.parameter, choice.c_str());
}

rcl_interfaces::msg::SetParametersResult CameraNode::onSetParameters(const std::vector<rclcpp::Parameter>& parameters)
{
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto& parameter : parameters)
  {
    if (parameter.get_name() == "folder")
    {
      const auto folder = parameter.get_type() == rclcpp::ParameterType::PARAMETER_STRING ?
                              pictureFolder(parameter.as_string()) :
                              std::nullopt;
      if (!folder)
      {
        result.successful = false;
        result.reason = "folder must be a relative path inside download_directory, without ..";
        return result;
      }
      std::lock_guard<std::mutex> lock(folder_mutex_);
      folder_ = *folder;
      continue;
    }
    const auto* setting = findSetting(parameter.get_name());
    if (setting == nullptr)
    {
      continue;
    }
    const auto value = parameterText(parameter.get_parameter_value());
    if (!value)
    {
      result.successful = false;
      result.reason = parameter.get_name() + " must be a string or a number";
      return result;
    }
    // While the camera is disconnected, the value is only remembered, and
    // applied when it connects.
    if (!value->empty() && driver_->isConnected())
    {
      try
      {
        driver_->run([this, setting, value](Camera& camera) { applySetting(camera, *setting, *value); });
      }
      catch (const CameraError& error)
      {
        result.successful = false;
        result.reason = error.what();
        return result;
      }
    }
    std::lock_guard<std::mutex> lock(wanted_settings_mutex_);
    wanted_settings_[parameter.get_name()] = *value;
  }
  return result;
}

void CameraNode::onPreview(std::vector<uint8_t>&& frame)
{
  sensor_msgs::msg::CompressedImage message;
  message.header.stamp = now();
  message.header.frame_id = frame_id_;
  message.format = "jpeg";
  message.data = std::move(frame);
  preview_publisher_->publish(message);
}

void CameraNode::onPicture(const CameraFile& file, std::vector<uint8_t>&& data)
{
  stepit_camera_msgs::msg::Picture message;
  message.header.stamp = now();
  message.header.frame_id = frame_id_;
  message.name = file.name;
  message.folder = file.folder;

  // The message says where the file is, never what is in it: a RAW file is
  // tens of megabytes, too heavy for DDS, which the other nodes share.
  std::filesystem::path folder;
  {
    std::lock_guard<std::mutex> lock(folder_mutex_);
    folder = folder_;
  }
  try
  {
    const auto root = expandHome(download_directory_);
    const auto path = savePicture(root / folder, file.name, data);
    message.path = path.string();
    message.relative_path = path.lexically_relative(root).generic_string();
  }
  catch (const std::exception& error)
  {
    const char* copy = keep_on_camera_ ? "it is still on the memory card" : "it is lost";
    RCLCPP_ERROR(get_logger(), "Cannot save %s, %s: %s", file.name.c_str(), copy, error.what());
    return;
  }
  RCLCPP_INFO(get_logger(), "Downloaded %s (%.1f MB) to %s", file.name.c_str(), static_cast<double>(data.size()) / 1e6,
              message.path.c_str());
  picture_publisher_->publish(message);
}

void CameraNode::startStreaming(const std::shared_ptr<std_srvs::srv::Trigger::Request>,
                                std::shared_ptr<std_srvs::srv::Trigger::Response> response)
{
  driver_->setStreaming(true);
  response->success = true;
  response->message =
      driver_->isConnected() ? "Streaming" : "The camera is not connected: streaming starts when it connects";
}

void CameraNode::stopStreaming(const std::shared_ptr<std_srvs::srv::Trigger::Request>,
                               std::shared_ptr<std_srvs::srv::Trigger::Response> response)
{
  driver_->setStreaming(false);
  response->success = true;
  response->message = "Not streaming";
}

void CameraNode::getSettings(const std::shared_ptr<stepit_camera_msgs::srv::GetSettings::Request>,
                             std::shared_ptr<stepit_camera_msgs::srv::GetSettings::Response> response)
{
  try
  {
    response->settings = driver_->run([](Camera& camera) {
      std::vector<stepit_camera_msgs::msg::Setting> settings;
      for (const auto& setting : SETTINGS)
      {
        stepit_camera_msgs::msg::Setting message;
        message.name = setting.parameter;
        message.value = camera.getSetting(setting.config);
        message.choices = camera.getChoices(setting.config);
        settings.push_back(message);
      }
      return settings;
    });
    response->success = true;
  }
  catch (const CameraError& error)
  {
    response->success = false;
    response->message = error.what();
  }
}

void CameraNode::takePicture(const std::shared_ptr<std_srvs::srv::Trigger::Request>,
                             std::shared_ptr<std_srvs::srv::Trigger::Response> response)
{
  try
  {
    driver_->run([](Camera& camera) { camera.trigger(); });
    response->success = true;
    response->message = "Shutter released";
  }
  catch (const CameraError& error)
  {
    response->success = false;
    response->message = error.what();
  }
}

}  // namespace stepit_camera
