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

#include <gtest/gtest.h>

#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <stepit_camera/camera_node.hpp>
#include <stepit_camera_msgs/msg/picture.hpp>
#include <stepit_camera_msgs/srv/get_settings.hpp>

#include "wait.hpp"

namespace stepit_camera::test
{

namespace fs = std::filesystem;
using std_srvs::srv::Trigger;
using stepit_camera_msgs::msg::Picture;
using stepit_camera_msgs::srv::GetSettings;

/**
 * @brief The camera node running a fake camera, and a client node talking to
 * it, both spun on a thread of their own. Each test runs them in a namespace
 * of its own, so that they do not hear another test's node.
 */
class CameraNodeTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    rclcpp::init(0, nullptr);
  }

  static void TearDownTestSuite()
  {
    rclcpp::shutdown();
  }

  void SetUp() override
  {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    namespace_ = std::string("/test_") + std::to_string(::getpid()) + "_" + info->name();
    folder_ = fs::temp_directory_path() / ("stepit_camera" + namespace_);
    fs::remove_all(folder_);
  }

  void TearDown() override
  {
    if (executor_)
    {
      executor_->cancel();
      spinner_.join();
    }
    fs::remove_all(folder_);
  }

  void start(std::vector<rclcpp::Parameter> parameters = {})
  {
    parameters.emplace_back("fake_camera", true);
    parameters.emplace_back("download_directory", folder_.string());
    parameters.emplace_back("preview_rate", 50.0);
    parameters.emplace_back("reconnect_period", 0.02);

    rclcpp::NodeOptions options;
    options.arguments({ "--ros-args", "-r", "__ns:=" + namespace_ });
    options.parameter_overrides(parameters);
    camera_ = std::make_shared<CameraNode>(options);

    client_ = std::make_shared<rclcpp::Node>("client", namespace_);
    preview_subscription_ = client_->create_subscription<sensor_msgs::msg::CompressedImage>(
        "camera/preview/compressed", rclcpp::QoS(1), [this](sensor_msgs::msg::CompressedImage::ConstSharedPtr frame) {
          std::lock_guard<std::mutex> lock(mutex_);
          last_frame_ = frame;
          ++frames_;
        });
    picture_subscription_ = client_->create_subscription<Picture>("camera/picture", rclcpp::QoS(10),
                                                                  [this](Picture::ConstSharedPtr picture) {
                                                                    std::lock_guard<std::mutex> lock(mutex_);
                                                                    pictures_.push_back(picture);
                                                                  });

    executor_ = std::make_unique<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(camera_);
    executor_->add_node(client_);
    spinner_ = std::thread([this] { executor_->spin(); });
  }

  /// @brief Call a service of the camera and return its response.
  template <typename Service>
  typename Service::Response::SharedPtr call(const std::string& name)
  {
    auto client = client_->create_client<Service>("camera/" + name);
    EXPECT_TRUE(client->wait_for_service(std::chrono::seconds(5)));
    auto future = client->async_send_request(std::make_shared<typename Service::Request>());
    EXPECT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    return future.get();
  }

  /// @brief The current value of a setting on the camera.
  std::string setting(const std::string& name)
  {
    const auto response = call<GetSettings>("get_settings");
    EXPECT_TRUE(response->success) << response->message;
    for (const auto& setting : response->settings)
    {
      if (setting.name == name)
      {
        return setting.value;
      }
    }
    ADD_FAILURE() << "No setting " << name;
    return "";
  }

  std::vector<Picture::ConstSharedPtr> pictures()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return pictures_;
  }

  std::string namespace_;
  fs::path folder_;
  std::shared_ptr<CameraNode> camera_;
  rclcpp::Node::SharedPtr client_;
  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr preview_subscription_;
  rclcpp::Subscription<Picture>::SharedPtr picture_subscription_;
  std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
  std::thread spinner_;

  std::mutex mutex_;
  std::atomic<int> frames_{ 0 };
  sensor_msgs::msg::CompressedImage::ConstSharedPtr last_frame_;
  std::vector<Picture::ConstSharedPtr> pictures_;
};

TEST_F(CameraNodeTest, TheLiveViewStartsAndStopsOnRequest)
{
  start();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(frames_, 0);

  const auto started = call<Trigger>("start_streaming");
  EXPECT_TRUE(started->success);
  ASSERT_TRUE(waitUntil([this] { return frames_ >= 3; }));
  {
    std::lock_guard<std::mutex> lock(mutex_);
    EXPECT_EQ(last_frame_->format, "jpeg");
    EXPECT_EQ(last_frame_->header.frame_id, "camera");
    EXPECT_GT(last_frame_->data.size(), 1000u);
  }

  const auto stopped = call<Trigger>("stop_streaming");
  EXPECT_TRUE(stopped->success);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  const int frames = frames_;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(frames_, frames);
}

TEST_F(CameraNodeTest, TheLiveViewCanStartWithTheNode)
{
  start({ rclcpp::Parameter("stream_on_start", true) });
  EXPECT_TRUE(waitUntil([this] { return frames_ >= 3; }));
}

TEST_F(CameraNodeTest, APictureIsSavedAndPublishedWithItsPath)
{
  start();

  const auto shot = call<Trigger>("take_picture");
  EXPECT_TRUE(shot->success);

  ASSERT_TRUE(waitUntil([this] { return pictures().size() == 1; }));
  const auto picture = pictures()[0];
  EXPECT_EQ(picture->name, "IMG_0001.JPG");
  EXPECT_EQ(picture->header.frame_id, "camera");
  EXPECT_EQ(fs::path(picture->path), folder_ / "IMG_0001.JPG");

  // The file is saved before the message says where it is.
  std::ifstream file(picture->path, std::ios::binary);
  const std::vector<uint8_t> saved{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
  ASSERT_GT(saved.size(), 1000u);
  EXPECT_EQ(saved[0], 0xff);
  EXPECT_EQ(saved[1], 0xd8);
}

TEST_F(CameraNodeTest, TheNodeNeedsAFolderForThePictures)
{
  rclcpp::NodeOptions options;
  options.arguments({ "--ros-args", "-r", "__ns:=" + namespace_ });
  options.parameter_overrides({ rclcpp::Parameter("fake_camera", true), rclcpp::Parameter("download_directory", "") });

  EXPECT_THROW(CameraNode{ options }, std::invalid_argument);
}

TEST_F(CameraNodeTest, TheExposureIsSetThroughParameters)
{
  start();
  ASSERT_TRUE(waitUntil([this] { return call<GetSettings>("get_settings")->success; }));

  EXPECT_TRUE(camera_->set_parameter(rclcpp::Parameter("iso", "800")).successful);
  EXPECT_TRUE(camera_->set_parameter(rclcpp::Parameter("shutter_speed", "1/60")).successful);
  // As `ros2 param set` sends them: numbers, not strings.
  EXPECT_TRUE(camera_->set_parameter(rclcpp::Parameter("aperture", 5.6)).successful);
  EXPECT_TRUE(camera_->set_parameter(rclcpp::Parameter("exposure_compensation", -1)).successful);
  EXPECT_TRUE(camera_->set_parameter(rclcpp::Parameter("white_balance", "daylight")).successful);

  EXPECT_EQ(setting("iso"), "800");
  EXPECT_EQ(setting("shutter_speed"), "1/60");
  EXPECT_EQ(setting("aperture"), "5.6");
  EXPECT_EQ(setting("exposure_compensation"), "-1");
  EXPECT_EQ(setting("white_balance"), "Daylight");
}

TEST_F(CameraNodeTest, AValueTheCameraDoesNotAcceptIsRejected)
{
  start();
  ASSERT_TRUE(waitUntil([this] { return call<GetSettings>("get_settings")->success; }));

  const auto result = camera_->set_parameter(rclcpp::Parameter("iso", 123));

  EXPECT_FALSE(result.successful);
  EXPECT_NE(result.reason.find("only one of: Auto, 100"), std::string::npos) << result.reason;
  EXPECT_EQ(camera_->get_parameter("iso").as_string(), "");
  EXPECT_EQ(setting("iso"), "400");
}

TEST_F(CameraNodeTest, TheExposureIsAppliedWhenTheCameraConnects)
{
  start({ rclcpp::Parameter("iso", "1600"), rclcpp::Parameter("aperture", "11") });
  ASSERT_TRUE(waitUntil([this] { return call<GetSettings>("get_settings")->success; }));

  EXPECT_EQ(setting("iso"), "1600");
  EXPECT_EQ(setting("aperture"), "11");
  // Left as it was.
  EXPECT_EQ(setting("shutter_speed"), "1/125");
}

TEST_F(CameraNodeTest, TheSettingsListTheirChoices)
{
  start();
  ASSERT_TRUE(waitUntil([this] { return call<GetSettings>("get_settings")->success; }));

  const auto response = call<GetSettings>("get_settings");
  ASSERT_EQ(response->settings.size(), 5u);
  EXPECT_EQ(response->settings[0].name, "iso");
  EXPECT_EQ(response->settings[0].choices.front(), "Auto");
  EXPECT_EQ(response->settings[1].name, "shutter_speed");
  EXPECT_EQ(response->settings[2].name, "aperture");
  EXPECT_EQ(response->settings[3].name, "exposure_compensation");
  EXPECT_EQ(response->settings[4].name, "white_balance");
}

}  // namespace stepit_camera::test
