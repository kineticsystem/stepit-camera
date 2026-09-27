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

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace stepit_camera
{

/// @brief A camera setting exposed as a ROS parameter.
struct Setting
{
  /// @brief The name of the ROS parameter, e.g. shutter_speed.
  const char* parameter;
  /// @brief The name of the setting in libgphoto2, e.g. shutterspeed.
  const char* config;
  /// @brief What it does, shown by `ros2 param describe`.
  const char* description;
};

/// @brief The settings of the exposure, in the order they are applied.
constexpr std::array<Setting, 4> SETTINGS = { {
    { "iso", "iso", "The ISO, e.g. 400 or Auto." },
    { "shutter_speed", "shutterspeed", "The shutter speed, e.g. 1/125 or 2. Needs the mode dial on M or Tv." },
    { "aperture", "aperture", "The aperture, e.g. 8 or 5.6. Needs the mode dial on M or Av." },
    { "exposure_compensation", "exposurecompensation",
      "The exposure compensation in stops, e.g. -1 or 0.3. Has no effect on M, unless the ISO is Auto." },
} };

/// @brief The libgphoto2 setting that tells the camera where to store a picture.
constexpr const char* CAPTURE_TARGET = "capturetarget";

/**
 * @brief The choice of a setting that a value stands for, if any.
 *
 * The value may be written the way the camera writes it, e.g. "1/125", or as
 * a number equal to a numeric choice, e.g. "5.60" for "5.6", so that a value
 * typed on the command line as a number finds its choice. Letter case is
 * ignored, e.g. "auto" for "Auto".
 */
std::optional<std::string> matchChoice(const std::string& value, const std::vector<std::string>& choices);

/**
 * @brief The choice of the setting `capturetarget` that stores the pictures on
 * the memory card, or in the camera's memory (RAM) until they are downloaded.
 */
std::optional<std::string> captureTargetChoice(bool memory_card, const std::vector<std::string>& choices);

/// @brief Expand a leading ~ into the home folder.
std::filesystem::path expandHome(const std::string& path);

/**
 * @brief Save a picture into a folder, creating the folder if needed, and
 * return the path of the file.
 *
 * The camera numbers its files from IMG_0001 again after IMG_9999, or with a
 * new card, so a name can come back. A file never overwrites another one: it
 * is given a suffix instead, e.g. IMG_0001_1.JPG. Throws
 * std::filesystem::filesystem_error or std::runtime_error on failure.
 */
std::filesystem::path savePicture(const std::filesystem::path& folder, const std::string& name,
                                  const std::vector<uint8_t>& data);

}  // namespace stepit_camera
