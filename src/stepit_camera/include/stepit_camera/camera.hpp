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
#include <stdexcept>
#include <string>
#include <vector>

namespace stepit_camera
{

/**
 * @brief A failure reported by the camera.
 *
 * A fatal error means the connection is gone, e.g. the cable was unplugged or
 * the camera switched itself off: the camera must be closed and opened again.
 * Any other error, e.g. a value the camera does not accept, or a camera busy
 * writing a picture to the card, leaves the connection working.
 */
class CameraError : public std::runtime_error
{
public:
  explicit CameraError(const std::string& message, bool fatal = false) : std::runtime_error(message), fatal_(fatal)
  {
  }

  bool isFatal() const
  {
    return fatal_;
  }

private:
  bool fatal_;
};

/// @brief A file stored on the camera, e.g. a picture just taken.
struct CameraFile
{
  std::string folder;
  std::string name;

  bool operator==(const CameraFile& other) const
  {
    return folder == other.folder && name == other.name;
  }
};

/**
 * @brief The operations the driver needs from a camera.
 *
 * GPhotoCamera talks to a real camera through libgphoto2; FakeCamera stands in
 * for it in the tests and when no camera is connected.
 *
 * A camera is not thread safe: only one thread may use it, the one of the
 * CameraDriver. Every operation but open() and isOpen() requires the camera to
 * be open. They throw CameraError on failure.
 */
class Camera
{
public:
  virtual ~Camera() = default;

  /// @brief Connect to the camera. Throws CameraError when there is none.
  virtual void open() = 0;

  /// @brief Disconnect from the camera. Safe to call when it is not open.
  virtual void close() = 0;

  virtual bool isOpen() const = 0;

  /// @brief The model of the camera, e.g. "Canon EOS 5D Mark II".
  virtual std::string model() const = 0;

  /**
   * @brief One frame of the live view, as a JPEG image. The first call
   * switches the live view on.
   */
  virtual std::vector<uint8_t> capturePreview() = 0;

  /// @brief Switch the live view off, so that the camera goes back to the viewfinder.
  virtual void stopPreview() = 0;

  /**
   * @brief The current value of a setting, e.g. getSetting("iso") returns
   * "400". The names are those of libgphoto2, as `gphoto2 --list-config`
   * shows them.
   */
  virtual std::string getSetting(const std::string& name) = 0;

  /// @brief The values a setting accepts right now, e.g. "100", "200", ... for the ISO.
  virtual std::vector<std::string> getChoices(const std::string& name) = 0;

  /// @brief Change a setting. The value must be one of getChoices(name).
  virtual void setSetting(const std::string& name, const std::string& value) = 0;

  /**
   * @brief Release the shutter, as the remote shutter release does. Returns at
   * once: the pictures are reported by waitForFiles() when the camera has
   * stored them.
   */
  virtual void trigger() = 0;

  /**
   * @brief Wait for the camera to report new files, i.e. that a picture was
   * taken, for at most the given time.
   *
   * Returns as soon as there is at least one file, or when the time is up,
   * with no file. A single shot can produce more than one file, e.g. with
   * RAW+JPEG.
   */
  virtual std::vector<CameraFile> waitForFiles(std::chrono::milliseconds timeout) = 0;

  /// @brief The content of a file on the camera.
  virtual std::vector<uint8_t> download(const CameraFile& file) = 0;

  /// @brief Delete a file from the camera.
  virtual void remove(const CameraFile& file) = 0;
};

}  // namespace stepit_camera
