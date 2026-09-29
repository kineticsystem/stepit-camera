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

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "stepit_camera/camera.hpp"

namespace stepit_camera
{

/**
 * @brief A camera that exists only in memory, to run the driver with no
 * hardware: in the tests, and with the parameter `fake_camera`.
 *
 * Its settings and their choices are those of a Canon EOS 5D Mark II on M.
 * The live view is a moving test pattern, and trigger() stands for the
 * external device firing the shutter. Like the 5D Mark II, a shot during the
 * live view breaks it: every frame fails until stopPreview() is called.
 *
 * Unlike a real camera, it is thread safe, so that a test or a ROS service
 * can trigger it while the driver's thread uses it.
 */
class FakeCamera : public Camera
{
public:
  FakeCamera();

  void open() override;
  void close() override;
  bool isOpen() const override;
  std::string model() const override;

  std::vector<uint8_t> capturePreview() override;
  void stopPreview() override;

  std::string getSetting(const std::string& name) override;
  std::vector<std::string> getChoices(const std::string& name) override;
  void setSetting(const std::string& name, const std::string& value) override;

  std::vector<CameraFile> waitForFiles(std::chrono::milliseconds timeout) override;
  std::vector<uint8_t> download(const CameraFile& file) override;
  void remove(const CameraFile& file) override;

  /// @brief Take a JPEG picture. Like trigger(false).
  void trigger() override;

  /**
   * @brief Take a picture, as the external device does by closing the
   * contact of the remote shutter release. With `raw` true, the camera stores
   * a CR2 file next to the JPEG, as with the RAW+JPEG quality.
   */
  void trigger(bool raw);

  /**
   * @brief Plug the camera in or out. While unplugged, open() fails and every
   * other operation throws a fatal CameraError.
   */
  void setConnected(bool connected);

  /// @brief Whether the live view is on: capturePreview() was called since the last stopPreview().
  bool isPreviewing() const;

  /// @brief The files stored on the camera.
  std::vector<CameraFile> files() const;

private:
  /// @brief Throw a fatal CameraError unless the camera is open.
  void checkOpen() const;

  mutable std::mutex mutex_;
  std::condition_variable files_added_;
  bool connected_ = true;
  bool open_ = false;
  bool previewing_ = false;
  /// @brief A shot was taken during the live view: frames fail until stopPreview().
  bool preview_lost_ = false;
  int frame_ = 0;
  int next_number_ = 1;
  std::map<std::string, std::string> settings_;
  std::map<std::string, std::vector<std::string>> choices_;
  /// @brief The content of each file, by its name.
  std::map<std::string, std::vector<uint8_t>> files_;
  /// @brief The files added since the last call to waitForFiles().
  std::vector<CameraFile> new_files_;
};

/**
 * @brief Encode a test pattern as a JPEG image: color bars with a white bar
 * that moves with `frame`, so that a live view visibly runs.
 */
std::vector<uint8_t> testPattern(int width, int height, int frame);

}  // namespace stepit_camera
