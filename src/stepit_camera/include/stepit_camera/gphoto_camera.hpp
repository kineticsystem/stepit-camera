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
#include <cstdint>
#include <string>
#include <vector>

#include "stepit_camera/camera.hpp"

// Forward declarations of the libgphoto2 types, so that including this header
// does not pull the whole C API in.
struct _Camera;
struct _GPContext;

namespace stepit_camera
{

/**
 * @brief A camera connected over USB, driven through libgphoto2.
 *
 * It opens the first camera libgphoto2 finds: the driver drives one camera.
 * See http://www.gphoto.org/proj/libgphoto2/support.php for the
 * supported models: the Canon EOS 5D Mark II supports capture, live view and
 * configuration.
 */
class GPhotoCamera : public Camera
{
public:
  GPhotoCamera() = default;
  ~GPhotoCamera() override;

  GPhotoCamera(const GPhotoCamera&) = delete;
  GPhotoCamera& operator=(const GPhotoCamera&) = delete;

  void open() override;
  void close() override;
  bool isOpen() const override;
  std::string model() const override;

  std::vector<uint8_t> capturePreview() override;
  void stopPreview() override;

  std::string getSetting(const std::string& name) override;
  std::vector<std::string> getChoices(const std::string& name) override;
  void setSetting(const std::string& name, const std::string& value) override;

  void trigger() override;
  std::vector<CameraFile> waitForFiles(std::chrono::milliseconds timeout) override;
  std::vector<uint8_t> download(const CameraFile& file) override;
  void remove(const CameraFile& file) override;

private:
  /// @brief Throw a CameraError if a libgphoto2 call failed.
  void check(int result, const std::string& what) const;

  _Camera* camera_ = nullptr;
  _GPContext* context_ = nullptr;
  std::string model_;
};

}  // namespace stepit_camera
