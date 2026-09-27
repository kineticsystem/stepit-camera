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

#include "stepit_camera/gphoto_camera.hpp"

#include <gphoto2/gphoto2.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <sstream>

namespace stepit_camera
{

namespace
{

/**
 * @brief Whether a libgphoto2 error means the connection is lost.
 *
 * These are the errors of the USB port (GP_ERROR_IO and GP_ERROR_IO_*, from
 * gphoto2-port-result.h) and the camera no longer being found. Anything else,
 * e.g. GP_ERROR_CAMERA_BUSY while the camera writes a picture to the card, is
 * worth retrying on the same connection.
 */
bool isFatal(int result)
{
  return result == GP_ERROR_IO || (result <= GP_ERROR_IO_SUPPORTED_SERIAL && result >= GP_ERROR_IO_LOCK) ||
         result == GP_ERROR_MODEL_NOT_FOUND;
}

/// @brief Frees a widget returned by gp_camera_get_single_config.
struct WidgetDeleter
{
  void operator()(CameraWidget* widget) const
  {
    gp_widget_free(widget);
  }
};
using WidgetPtr = std::unique_ptr<CameraWidget, WidgetDeleter>;

/// @brief Releases a file created by gp_file_new.
struct FileDeleter
{
  void operator()(::CameraFile* file) const
  {
    gp_file_unref(file);
  }
};
using FilePtr = std::unique_ptr<::CameraFile, FileDeleter>;

/// @brief The content of a file, copied out of libgphoto2.
std::vector<uint8_t> dataOf(::CameraFile* file)
{
  const char* data = nullptr;
  unsigned long int size = 0;  // NOLINT(runtime/int): the type libgphoto2 uses.
  gp_file_get_data_and_size(file, &data, &size);
  return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size);
}

}  // namespace

GPhotoCamera::~GPhotoCamera()
{
  close();
}

void GPhotoCamera::check(int result, const std::string& what) const
{
  if (result < GP_OK)
  {
    throw CameraError(what + ": " + gp_result_as_string(result), isFatal(result));
  }
}

void GPhotoCamera::open()
{
  close();
  context_ = gp_context_new();
  gp_camera_new(&camera_);

  // Without a model and a port set beforehand, libgphoto2 opens the first
  // camera it detects.
  const int result = gp_camera_init(camera_, context_);
  if (result < GP_OK)
  {
    close();
    // Not finding a camera is the normal state while it is off or unplugged:
    // the driver tries again later.
    throw CameraError(std::string("Cannot open the camera: ") + gp_result_as_string(result), true);
  }

  CameraAbilities abilities;
  gp_camera_get_abilities(camera_, &abilities);
  model_ = abilities.model;
}

void GPhotoCamera::close()
{
  if (camera_ != nullptr)
  {
    gp_camera_exit(camera_, context_);
    gp_camera_unref(camera_);
    camera_ = nullptr;
  }
  if (context_ != nullptr)
  {
    gp_context_unref(context_);
    context_ = nullptr;
  }
  model_.clear();
}

bool GPhotoCamera::isOpen() const
{
  return camera_ != nullptr;
}

std::string GPhotoCamera::model() const
{
  return model_;
}

std::vector<uint8_t> GPhotoCamera::capturePreview()
{
  ::CameraFile* raw_file = nullptr;
  check(gp_file_new(&raw_file), "Cannot allocate a frame");
  FilePtr file(raw_file);
  check(gp_camera_capture_preview(camera_, file.get(), context_), "Cannot capture a frame of the live view");
  return dataOf(file.get());
}

void GPhotoCamera::stopPreview()
{
  // Canon EOS cameras keep the live view on, and the mirror up, until the
  // viewfinder setting is switched off. Other cameras may not have it.
  CameraWidget* raw_widget = nullptr;
  if (gp_camera_get_single_config(camera_, "viewfinder", &raw_widget, context_) < GP_OK)
  {
    return;
  }
  WidgetPtr widget(raw_widget);
  int off = 0;
  gp_widget_set_value(widget.get(), &off);
  check(gp_camera_set_single_config(camera_, "viewfinder", widget.get(), context_), "Cannot stop the live view");
}

std::string GPhotoCamera::getSetting(const std::string& name)
{
  CameraWidget* raw_widget = nullptr;
  check(gp_camera_get_single_config(camera_, name.c_str(), &raw_widget, context_), "Cannot read " + name);
  WidgetPtr widget(raw_widget);

  CameraWidgetType type;
  gp_widget_get_type(widget.get(), &type);
  switch (type)
  {
    case GP_WIDGET_TEXT:
    case GP_WIDGET_RADIO:
    case GP_WIDGET_MENU:
    {
      const char* value = nullptr;
      gp_widget_get_value(widget.get(), &value);
      return value != nullptr ? value : "";
    }
    case GP_WIDGET_TOGGLE:
    case GP_WIDGET_DATE:
    {
      int value = 0;
      gp_widget_get_value(widget.get(), &value);
      return std::to_string(value);
    }
    case GP_WIDGET_RANGE:
    {
      float value = 0;
      gp_widget_get_value(widget.get(), &value);
      std::ostringstream stream;
      stream << value;
      return stream.str();
    }
    default:
      throw CameraError(name + " is not a setting");
  }
}

std::vector<std::string> GPhotoCamera::getChoices(const std::string& name)
{
  CameraWidget* raw_widget = nullptr;
  check(gp_camera_get_single_config(camera_, name.c_str(), &raw_widget, context_), "Cannot read " + name);
  WidgetPtr widget(raw_widget);

  std::vector<std::string> choices;
  const int count = gp_widget_count_choices(widget.get());
  for (int i = 0; i < count; ++i)
  {
    const char* choice = nullptr;
    if (gp_widget_get_choice(widget.get(), i, &choice) == GP_OK && choice != nullptr)
    {
      choices.emplace_back(choice);
    }
  }
  return choices;
}

void GPhotoCamera::setSetting(const std::string& name, const std::string& value)
{
  CameraWidget* raw_widget = nullptr;
  check(gp_camera_get_single_config(camera_, name.c_str(), &raw_widget, context_), "Cannot read " + name);
  WidgetPtr widget(raw_widget);

  CameraWidgetType type;
  gp_widget_get_type(widget.get(), &type);
  int result = GP_OK;
  switch (type)
  {
    case GP_WIDGET_TEXT:
    case GP_WIDGET_RADIO:
    case GP_WIDGET_MENU:
      result = gp_widget_set_value(widget.get(), value.c_str());
      break;
    case GP_WIDGET_TOGGLE:
    case GP_WIDGET_DATE:
    {
      const int number = std::stoi(value);
      result = gp_widget_set_value(widget.get(), &number);
      break;
    }
    case GP_WIDGET_RANGE:
    {
      const float number = std::stof(value);
      result = gp_widget_set_value(widget.get(), &number);
      break;
    }
    default:
      throw CameraError(name + " is not a setting");
  }
  check(result, "Cannot set " + name + " to " + value);
  check(gp_camera_set_single_config(camera_, name.c_str(), widget.get(), context_),
        "Cannot set " + name + " to " + value);
}

std::vector<CameraFile> GPhotoCamera::waitForFiles(std::chrono::milliseconds timeout)
{
  using Clock = std::chrono::steady_clock;
  const auto deadline = Clock::now() + timeout;

  // A Canon EOS reports a stream of events, most of them about properties
  // that changed. Keep waiting through them until a file is added or the time
  // is up, then collect whatever else is already queued: RAW+JPEG adds two
  // files for one shot.
  std::vector<CameraFile> files;
  for (;;)
  {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
    const int wait = files.empty() ? static_cast<int>(std::max<int64_t>(remaining.count(), 0)) : 0;

    CameraEventType type = GP_EVENT_UNKNOWN;
    void* data = nullptr;
    check(gp_camera_wait_for_event(camera_, wait, &type, &data, context_), "Cannot read the events of the camera");

    if (type == GP_EVENT_FILE_ADDED && data != nullptr)
    {
      const auto* path = static_cast<CameraFilePath*>(data);
      files.push_back(CameraFile{ path->folder, path->name });
    }
    std::free(data);

    if (type == GP_EVENT_TIMEOUT || (files.empty() && Clock::now() >= deadline))
    {
      return files;
    }
  }
}

std::vector<uint8_t> GPhotoCamera::download(const CameraFile& file)
{
  ::CameraFile* raw_file = nullptr;
  check(gp_file_new(&raw_file), "Cannot allocate a file");
  FilePtr camera_file(raw_file);
  check(gp_camera_file_get(camera_, file.folder.c_str(), file.name.c_str(), GP_FILE_TYPE_NORMAL, camera_file.get(),
                           context_),
        "Cannot download " + file.folder + "/" + file.name);
  return dataOf(camera_file.get());
}

void GPhotoCamera::remove(const CameraFile& file)
{
  check(gp_camera_file_delete(camera_, file.folder.c_str(), file.name.c_str(), context_),
        "Cannot delete " + file.folder + "/" + file.name);
}

}  // namespace stepit_camera
