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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <stepit_camera/camera_driver.hpp>
#include <stepit_camera/fake_camera.hpp>

#include "wait.hpp"

namespace stepit_camera::test
{

using std::chrono::milliseconds;

/// @brief Hands a fake camera to a driver while the test keeps it, so that it outlives the driver.
class SharedCamera : public Camera
{
public:
  explicit SharedCamera(std::shared_ptr<FakeCamera> camera) : camera_(std::move(camera))
  {
  }
  void open() override
  {
    camera_->open();
  }
  void close() override
  {
    camera_->close();
  }
  bool isOpen() const override
  {
    return camera_->isOpen();
  }
  std::string model() const override
  {
    return camera_->model();
  }
  std::vector<uint8_t> capturePreview() override
  {
    return camera_->capturePreview();
  }
  void stopPreview() override
  {
    camera_->stopPreview();
  }
  std::string getSetting(const std::string& name) override
  {
    return camera_->getSetting(name);
  }
  std::vector<std::string> getChoices(const std::string& name) override
  {
    return camera_->getChoices(name);
  }
  void setSetting(const std::string& name, const std::string& value) override
  {
    camera_->setSetting(name, value);
  }
  void trigger() override
  {
    camera_->trigger();
  }
  std::vector<CameraFile> waitForFiles(std::chrono::milliseconds timeout) override
  {
    return camera_->waitForFiles(timeout);
  }
  std::vector<uint8_t> download(const CameraFile& file) override
  {
    return camera_->download(file);
  }
  void remove(const CameraFile& file) override
  {
    camera_->remove(file);
  }

private:
  std::shared_ptr<FakeCamera> camera_;
};

/// @brief A driver running a fake camera, recording what it reports.
class CameraDriverTest : public ::testing::Test
{
protected:
  void start(DriverOptions options = DriverOptions())
  {
    options.reconnect_period = milliseconds(20);
    options.poll_period = milliseconds(10);
    options.preview_rate = 50.0;

    camera_ = std::make_shared<FakeCamera>();

    CameraDriver::Callbacks callbacks;
    callbacks.on_connected = [this](Camera&) { ++connections_; };
    callbacks.on_disconnected = [this](const std::string&) { ++disconnections_; };
    callbacks.on_preview = [this](std::vector<uint8_t>&&) { ++frames_; };
    callbacks.on_picture = [this](const CameraFile& file, std::vector<uint8_t>&& data) {
      std::lock_guard<std::mutex> lock(mutex_);
      pictures_.push_back(file.name);
      sizes_.push_back(data.size());
    };
    driver_ = std::make_unique<CameraDriver>(std::make_unique<SharedCamera>(camera_), options, callbacks);
    ASSERT_TRUE(waitUntil([this] { return driver_->isConnected(); }));
  }

  std::vector<std::string> pictures()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return pictures_;
  }

  std::shared_ptr<FakeCamera> camera_;
  std::unique_ptr<CameraDriver> driver_;
  std::atomic<int> connections_{ 0 };
  std::atomic<int> disconnections_{ 0 };
  std::atomic<int> frames_{ 0 };
  std::mutex mutex_;
  std::vector<std::string> pictures_;
  std::vector<size_t> sizes_;
};

TEST_F(CameraDriverTest, ItConnectsToTheCamera)
{
  start();
  EXPECT_EQ(connections_, 1);
  EXPECT_TRUE(camera_->isOpen());
}

TEST_F(CameraDriverTest, ItStreamsOnlyWhenAsked)
{
  start();
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(frames_, 0);
  EXPECT_FALSE(camera_->isPreviewing());

  driver_->setStreaming(true);
  ASSERT_TRUE(waitUntil([this] { return frames_ >= 5; }));
  EXPECT_TRUE(camera_->isPreviewing());

  driver_->setStreaming(false);
  ASSERT_TRUE(waitUntil([this] { return !camera_->isPreviewing(); }));
  const int frames = frames_;
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(frames_, frames);
}

TEST_F(CameraDriverTest, TheFramesComeAtTheRequestedRate)
{
  start();
  driver_->setStreaming(true);
  std::this_thread::sleep_for(milliseconds(500));
  driver_->setStreaming(false);

  // 50 per second: about 25 frames. Loose bounds, for a busy machine.
  EXPECT_GE(frames_, 10);
  EXPECT_LE(frames_, 30);
}

TEST_F(CameraDriverTest, EachPictureTakenIsDownloaded)
{
  start();
  camera_->trigger();
  camera_->trigger(true);

  ASSERT_TRUE(waitUntil([this] { return pictures().size() == 3; }));
  EXPECT_EQ(pictures(), (std::vector<std::string>{ "IMG_0001.JPG", "IMG_0002.CR2", "IMG_0002.JPG" }));
  EXPECT_GT(sizes_[0], 1000u);
  // Kept on the camera, by default.
  EXPECT_EQ(camera_->files().size(), 3u);
}

TEST_F(CameraDriverTest, PicturesAreDownloadedWhileStreaming)
{
  start();
  driver_->setStreaming(true);
  ASSERT_TRUE(waitUntil([this] { return frames_ >= 2; }));

  camera_->trigger();

  ASSERT_TRUE(waitUntil([this] { return pictures().size() == 1; }));
  const int frames = frames_;
  ASSERT_TRUE(waitUntil([this, frames] { return frames_ > frames + 2; }));
}

TEST_F(CameraDriverTest, ATestShotIsDownloadedLikeAnyPicture)
{
  start();
  driver_->setStreaming(true);
  ASSERT_TRUE(waitUntil([this] { return frames_ >= 2; }));

  driver_->run([](Camera& camera) { camera.trigger(); });

  ASSERT_TRUE(waitUntil([this] { return pictures().size() == 1; }));
  EXPECT_EQ(pictures()[0], "IMG_0001.JPG");
  // The shot broke the live view: the driver starts it again.
  const int frames = frames_;
  ASSERT_TRUE(waitUntil([this, frames] { return frames_ > frames + 2; }));
  EXPECT_EQ(disconnections_, 0);
}

TEST_F(CameraDriverTest, DownloadedPicturesCanBeDeletedFromTheCamera)
{
  DriverOptions options;
  options.keep_on_camera = false;
  start(options);

  camera_->trigger();

  ASSERT_TRUE(waitUntil([this] { return pictures().size() == 1; }));
  EXPECT_TRUE(waitUntil([this] { return camera_->files().empty(); }));
}

TEST_F(CameraDriverTest, ATaskRunsOnTheCamera)
{
  start();

  driver_->run([](Camera& camera) { camera.setSetting("iso", "1600"); });
  const auto iso = driver_->run([](Camera& camera) { return camera.getSetting("iso"); });

  EXPECT_EQ(iso, "1600");
}

TEST_F(CameraDriverTest, TheErrorOfATaskReachesItsCaller)
{
  start();
  EXPECT_THROW(driver_->run([](Camera& camera) { camera.setSetting("iso", "123"); }), CameraError);
  // The connection is not affected.
  EXPECT_TRUE(driver_->isConnected());
  EXPECT_NO_THROW(driver_->run([](Camera& camera) { return camera.getSetting("iso"); }));
}

TEST_F(CameraDriverTest, ItReconnectsWhenTheCameraComesBack)
{
  start();
  driver_->setStreaming(true);
  ASSERT_TRUE(waitUntil([this] { return frames_ >= 1; }));

  camera_->setConnected(false);
  ASSERT_TRUE(waitUntil([this] { return !driver_->isConnected(); }));
  EXPECT_EQ(disconnections_, 1);
  EXPECT_THROW(driver_->run([](Camera& camera) { return camera.getSetting("iso"); }), CameraError);

  camera_->setConnected(true);
  ASSERT_TRUE(waitUntil([this] { return driver_->isConnected(); }));
  EXPECT_EQ(connections_, 2);

  // It still streams, and still downloads.
  const int frames = frames_;
  ASSERT_TRUE(waitUntil([this, frames] { return frames_ > frames; }));
  camera_->trigger();
  ASSERT_TRUE(waitUntil([this] { return pictures().size() == 1; }));
}

TEST_F(CameraDriverTest, StoppingSwitchesTheLiveViewOffAndClosesTheCamera)
{
  start();
  driver_->setStreaming(true);
  ASSERT_TRUE(waitUntil([this] { return camera_->isPreviewing(); }));

  driver_.reset();

  EXPECT_FALSE(camera_->isPreviewing());
  EXPECT_FALSE(camera_->isOpen());
}

TEST_F(CameraDriverTest, ItWaitsForACameraToBePluggedIn)
{
  camera_ = std::make_shared<FakeCamera>();
  camera_->setConnected(false);
  DriverOptions options;
  options.reconnect_period = milliseconds(20);
  CameraDriver::Callbacks callbacks;
  callbacks.on_connected = [this](Camera&) { ++connections_; };
  driver_ = std::make_unique<CameraDriver>(std::make_unique<SharedCamera>(camera_), options, callbacks);

  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_FALSE(driver_->isConnected());
  EXPECT_EQ(connections_, 0);

  camera_->setConnected(true);
  ASSERT_TRUE(waitUntil([this] { return driver_->isConnected(); }));
  EXPECT_EQ(connections_, 1);
}

}  // namespace stepit_camera::test
