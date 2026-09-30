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

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "stepit_camera/camera_driver.hpp"
#include "stepit_camera/fake_camera.hpp"
#include "stepit_camera/settings.hpp"
#include "stepit_camera_msgs/msg/picture.hpp"
#include "stepit_camera_msgs/srv/get_settings.hpp"

namespace stepit_camera
{

/**
 * @brief The ROS2 interface of the camera.
 *
 * - `~/preview/compressed` (sensor_msgs/CompressedImage): the live view, as
 *   JPEG frames, while streaming.
 * - `~/picture` (stepit_camera_msgs/Picture): each picture the camera takes,
 *   downloaded as soon as it is reported, and saved into `download_directory`.
 * - `~/saved_picture` (stepit_camera_msgs/Picture): each picture once saved,
 *   without its content, for whoever can read the file instead, e.g. a web
 *   page through the web server.
 * - `~/start_streaming`, `~/stop_streaming` (std_srvs/Trigger): the live view.
 * - `~/get_settings` (stepit_camera_msgs/GetSettings): the current settings
 *   and the values they accept.
 * - `~/take_picture` (std_srvs/Trigger): release the shutter over USB, for a
 *   test shot. The picture comes on `~/picture`, like any other.
 *
 * The exposure is set through the parameters `iso`, `shutter_speed`,
 * `aperture`, `exposure_compensation` and `white_balance`. A value the camera does not accept
 * is rejected, with the values it does accept in the reason.
 */
class CameraNode : public rclcpp::Node
{
public:
  explicit CameraNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

private:
  /// @brief Configure the camera once it is connected: where to store pictures, and the exposure.
  void onConnected(Camera& camera);

  void onPreview(std::vector<uint8_t>&& frame);
  void onPicture(const CameraFile& file, std::vector<uint8_t>&& data);

  rcl_interfaces::msg::SetParametersResult onSetParameters(const std::vector<rclcpp::Parameter>& parameters);

  /// @brief Set one setting, e.g. the ISO, on the camera. Throws CameraError if the camera does not accept the value.
  void applySetting(Camera& camera, const Setting& setting, const std::string& value);

  void startStreaming(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                      std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  void stopStreaming(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                     std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  void getSettings(const std::shared_ptr<stepit_camera_msgs::srv::GetSettings::Request> request,
                   std::shared_ptr<stepit_camera_msgs::srv::GetSettings::Response> response);
  void takePicture(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                   std::shared_ptr<std_srvs::srv::Trigger::Response> response);

  std::string frame_id_;
  std::string download_directory_;
  bool keep_on_camera_ = true;

  /// @brief The value of each setting to apply on connection, by parameter name. Empty leaves the camera as it is.
  std::map<std::string, std::string> wanted_settings_;
  std::mutex wanted_settings_mutex_;

  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr preview_publisher_;
  rclcpp::Publisher<stepit_camera_msgs::msg::Picture>::SharedPtr picture_publisher_;
  rclcpp::Publisher<stepit_camera_msgs::msg::Picture>::SharedPtr saved_picture_publisher_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_streaming_service_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr stop_streaming_service_;
  rclcpp::Service<stepit_camera_msgs::srv::GetSettings>::SharedPtr get_settings_service_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr take_picture_service_;
  OnSetParametersCallbackHandle::SharedPtr parameters_callback_;

  /// @brief Last, so that its thread stops before anything it calls back is destroyed.
  std::unique_ptr<CameraDriver> driver_;
};

/**
 * @brief The text of a setting's parameter, e.g. "5.6" for the number 5.6, or
 * nothing if the parameter is neither a string nor a number.
 *
 * `ros2 param set` sends 400 as an integer and 5.6 as a double, so a setting
 * accepts them as well as strings, e.g. "1/125".
 */
std::optional<std::string> parameterText(const rclcpp::ParameterValue& value);

}  // namespace stepit_camera
