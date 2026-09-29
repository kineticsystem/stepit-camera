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

#include "stepit_camera/fake_camera.hpp"

// libjpeg's header needs the declarations of stdio.h, and does not include it.
#include <cstdio>

#include <jpeglib.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace stepit_camera
{

namespace
{

/// @brief The folder where a Canon EOS stores its pictures.
constexpr const char* FOLDER = "/store_00010001/DCIM/100CANON";

/// @brief The size of the live view of a 5D Mark II, and of the fake pictures.
constexpr int PREVIEW_WIDTH = 1024;
constexpr int PREVIEW_HEIGHT = 680;
constexpr int PICTURE_WIDTH = 1872;
constexpr int PICTURE_HEIGHT = 1248;

}  // namespace

std::vector<uint8_t> testPattern(int width, int height, int frame)
{
  // Seven color bars, as on a television test card.
  static const uint8_t BARS[7][3] = { { 192, 192, 192 }, { 192, 192, 0 }, { 0, 192, 192 }, { 0, 192, 0 },
                                      { 192, 0, 192 },   { 192, 0, 0 },   { 0, 0, 192 } };
  const int bar_x = (frame * 8) % width;

  std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      const uint8_t* color = BARS[x * 7 / width];
      const bool moving_bar = x >= bar_x && x < bar_x + 16;
      uint8_t* pixel = &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3];
      for (int c = 0; c < 3; ++c)
      {
        pixel[c] = moving_bar ? 255 : color[c];
      }
    }
  }

  jpeg_compress_struct compressor;
  jpeg_error_mgr errors;
  compressor.err = jpeg_std_error(&errors);
  jpeg_create_compress(&compressor);

  unsigned char* buffer = nullptr;
  unsigned long size = 0;  // NOLINT(runtime/int): the type libjpeg uses.
  jpeg_mem_dest(&compressor, &buffer, &size);

  compressor.image_width = static_cast<JDIMENSION>(width);
  compressor.image_height = static_cast<JDIMENSION>(height);
  compressor.input_components = 3;
  compressor.in_color_space = JCS_RGB;
  jpeg_set_defaults(&compressor);
  jpeg_set_quality(&compressor, 80, TRUE);
  jpeg_start_compress(&compressor, TRUE);
  while (compressor.next_scanline < compressor.image_height)
  {
    JSAMPROW row = &pixels[static_cast<size_t>(compressor.next_scanline) * static_cast<size_t>(width) * 3];
    jpeg_write_scanlines(&compressor, &row, 1);
  }
  jpeg_finish_compress(&compressor);
  jpeg_destroy_compress(&compressor);

  std::vector<uint8_t> jpeg(buffer, buffer + size);
  std::free(buffer);
  return jpeg;
}

FakeCamera::FakeCamera()
{
  choices_ = {
    { "iso", { "Auto", "100",  "125",  "160",  "200",  "250",  "320",  "400",  "500",  "640",
               "800",  "1000", "1250", "1600", "2000", "2500", "3200", "4000", "5000", "6400" } },
    { "shutterspeed",
      { "bulb",   "30",     "25",     "20",     "15",     "13",    "10",     "8",      "6",      "5",
        "4",      "3.2",    "2.5",    "2",      "1.6",    "1.3",   "1",      "0.8",    "0.6",    "0.5",
        "0.4",    "0.3",    "1/4",    "1/5",    "1/6",    "1/8",   "1/10",   "1/13",   "1/15",   "1/20",
        "1/25",   "1/30",   "1/40",   "1/50",   "1/60",   "1/80",  "1/100",  "1/125",  "1/160",  "1/200",
        "1/250",  "1/320",  "1/400",  "1/500",  "1/640",  "1/800", "1/1000", "1/1250", "1/1600", "1/2000",
        "1/2500", "1/3200", "1/4000", "1/5000", "1/6400", "1/8000" } },
    { "aperture", { "4", "4.5", "5", "5.6", "6.3", "7.1", "8", "9", "10", "11", "13", "14", "16", "18", "20", "22" } },
    { "exposurecompensation",
      { "-2", "-1.6", "-1.3", "-1", "-0.6", "-0.3", "0", "0.3", "0.6", "1", "1.3", "1.6", "2" } },
    { "whitebalance",
      { "Auto", "Daylight", "Shadow", "Cloudy", "Tungsten", "Fluorescent", "Flash", "Manual", "Color Temperature" } },
    { "capturetarget", { "Internal RAM", "Memory card" } },
  };
  settings_ = {
    { "iso", "400" },           { "shutterspeed", "1/125" },
    { "aperture", "8" },        { "exposurecompensation", "0" },
    { "whitebalance", "Auto" }, { "capturetarget", "Internal RAM" },
  };
}

void FakeCamera::checkOpen() const
{
  if (!connected_ || !open_)
  {
    throw CameraError("The camera is not connected", true);
  }
}

void FakeCamera::open()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_)
  {
    throw CameraError("Cannot open the camera: Could not detect any camera", true);
  }
  open_ = true;
}

void FakeCamera::close()
{
  std::lock_guard<std::mutex> lock(mutex_);
  open_ = false;
  previewing_ = false;
}

bool FakeCamera::isOpen() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return open_;
}

std::string FakeCamera::model() const
{
  return "Fake Canon EOS 5D Mark II";
}

std::vector<uint8_t> FakeCamera::capturePreview()
{
  int frame = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    checkOpen();
    if (preview_lost_)
    {
      throw CameraError("Cannot capture a frame of the live view: Unspecified error");
    }
    previewing_ = true;
    frame = frame_++;
  }
  return testPattern(PREVIEW_WIDTH, PREVIEW_HEIGHT, frame);
}

void FakeCamera::stopPreview()
{
  std::lock_guard<std::mutex> lock(mutex_);
  checkOpen();
  previewing_ = false;
  preview_lost_ = false;
}

std::string FakeCamera::getSetting(const std::string& name)
{
  std::lock_guard<std::mutex> lock(mutex_);
  checkOpen();
  const auto setting = settings_.find(name);
  if (setting == settings_.end())
  {
    throw CameraError("Cannot read " + name + ": Not Found");
  }
  return setting->second;
}

std::vector<std::string> FakeCamera::getChoices(const std::string& name)
{
  std::lock_guard<std::mutex> lock(mutex_);
  checkOpen();
  const auto choices = choices_.find(name);
  if (choices == choices_.end())
  {
    throw CameraError("Cannot read " + name + ": Not Found");
  }
  return choices->second;
}

void FakeCamera::setSetting(const std::string& name, const std::string& value)
{
  std::lock_guard<std::mutex> lock(mutex_);
  checkOpen();
  const auto choices = choices_.find(name);
  if (choices == choices_.end())
  {
    throw CameraError("Cannot read " + name + ": Not Found");
  }
  if (std::find(choices->second.begin(), choices->second.end(), value) == choices->second.end())
  {
    throw CameraError("Cannot set " + name + " to " + value + ": Bad parameters");
  }
  settings_[name] = value;
}

std::vector<CameraFile> FakeCamera::waitForFiles(std::chrono::milliseconds timeout)
{
  std::unique_lock<std::mutex> lock(mutex_);
  checkOpen();
  files_added_.wait_for(lock, timeout, [this] { return !new_files_.empty() || !connected_; });
  checkOpen();
  std::vector<CameraFile> files;
  files.swap(new_files_);
  return files;
}

std::vector<uint8_t> FakeCamera::download(const CameraFile& file)
{
  std::lock_guard<std::mutex> lock(mutex_);
  checkOpen();
  const auto content = files_.find(file.name);
  if (file.folder != FOLDER || content == files_.end())
  {
    throw CameraError("Cannot download " + file.folder + "/" + file.name + ": File not found");
  }
  return content->second;
}

void FakeCamera::remove(const CameraFile& file)
{
  std::lock_guard<std::mutex> lock(mutex_);
  checkOpen();
  if (file.folder != FOLDER || files_.erase(file.name) == 0)
  {
    throw CameraError("Cannot delete " + file.folder + "/" + file.name + ": File not found");
  }
}

void FakeCamera::trigger()
{
  trigger(false);
}

void FakeCamera::trigger(bool raw)
{
  int number = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    number = next_number_++;
  }
  // Encoding takes a while, so do it without holding the lock.
  const auto jpeg = testPattern(PICTURE_WIDTH, PICTURE_HEIGHT, number * 16);

  std::ostringstream stem;
  stem << "IMG_" << std::setw(4) << std::setfill('0') << number;

  std::lock_guard<std::mutex> lock(mutex_);
  if (raw)
  {
    // Not a real CR2: its content only has to differ from the JPEG's.
    files_[stem.str() + ".CR2"] = std::vector<uint8_t>(jpeg.rbegin(), jpeg.rend());
    new_files_.push_back(CameraFile{ FOLDER, stem.str() + ".CR2" });
  }
  files_[stem.str() + ".JPG"] = jpeg;
  new_files_.push_back(CameraFile{ FOLDER, stem.str() + ".JPG" });
  preview_lost_ = previewing_;
  files_added_.notify_all();
}

void FakeCamera::setConnected(bool connected)
{
  std::lock_guard<std::mutex> lock(mutex_);
  connected_ = connected;
  if (!connected)
  {
    open_ = false;
    previewing_ = false;
  }
  files_added_.notify_all();
}

bool FakeCamera::isPreviewing() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return previewing_;
}

std::vector<CameraFile> FakeCamera::files() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<CameraFile> files;
  for (const auto& file : files_)
  {
    files.push_back(CameraFile{ FOLDER, file.first });
  }
  return files;
}

}  // namespace stepit_camera
