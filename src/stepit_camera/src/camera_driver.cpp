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

#include "stepit_camera/camera_driver.hpp"

#include <algorithm>

namespace stepit_camera
{

using Clock = std::chrono::steady_clock;

CameraDriver::CameraDriver(std::unique_ptr<Camera> camera, const DriverOptions& options, Callbacks callbacks)
  : camera_(std::move(camera)), options_(options), callbacks_(std::move(callbacks)), thread_([this] { loop(); })
{
}

CameraDriver::~CameraDriver()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
  }
  wake_up_.notify_all();
  thread_.join();
}

void CameraDriver::setStreaming(bool streaming)
{
  streaming_ = streaming;
}

bool CameraDriver::isStreaming() const
{
  return streaming_;
}

bool CameraDriver::isConnected() const
{
  return connected_;
}

void CameraDriver::post(Task task)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_)
  {
    throw CameraError("The camera is not connected");
  }
  tasks_.push_back(std::move(task));
}

void CameraDriver::loop()
{
  while (running_)
  {
    if (!connected_ && !connect())
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_up_.wait_for(lock, options_.reconnect_period, [this] { return !running_; });
      continue;
    }
    try
    {
      runTasks();
      const auto now = Clock::now();
      stream(now);
      collectPictures(streaming_ ? std::max(next_frame_, now) : now + options_.poll_period);
    }
    catch (const CameraError& error)
    {
      if (error.isFatal())
      {
        disconnect(error.what());
      }
      else
      {
        warn(error.what());
      }
    }
  }

  // Leave the camera as we found it: with the mirror down, ready to shoot
  // through the viewfinder.
  if (connected_)
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      connected_ = false;
    }
    try
    {
      if (previewing_)
      {
        camera_->stopPreview();
      }
    }
    catch (const CameraError&)
    {
    }
  }
  camera_->close();
  dropTasks();
}

bool CameraDriver::connect()
{
  try
  {
    camera_->open();
  }
  catch (const CameraError&)
  {
    // No camera yet: it is off or unplugged. Not worth a warning every time.
    return false;
  }
  previewing_ = false;
  preview_failures_ = 0;
  next_frame_ = Clock::now();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = true;
  }
  try
  {
    if (callbacks_.on_connected)
    {
      callbacks_.on_connected(*camera_);
    }
  }
  catch (const CameraError& error)
  {
    if (error.isFatal())
    {
      disconnect(error.what());
      return false;
    }
    warn(error.what());
  }
  return true;
}

void CameraDriver::disconnect(const std::string& reason)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
  }
  dropTasks();
  camera_->close();
  previewing_ = false;
  if (callbacks_.on_disconnected)
  {
    callbacks_.on_disconnected(reason);
  }
}

void CameraDriver::runTasks()
{
  for (;;)
  {
    Task task;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (tasks_.empty())
      {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    // A packaged_task keeps the exceptions of its function for the caller, so
    // this never throws. If the function lost the connection, the next call
    // to the camera finds out.
    task(*camera_);
  }
}

void CameraDriver::dropTasks()
{
  std::deque<Task> dropped;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    dropped.swap(tasks_);
  }
  // Destroying a packaged_task that never ran makes its caller's future throw.
}

void CameraDriver::stream(Clock::time_point now)
{
  if (!streaming_)
  {
    if (previewing_)
    {
      previewing_ = false;
      camera_->stopPreview();
    }
    return;
  }
  if (now < next_frame_)
  {
    return;
  }
  const auto period =
      std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / options_.preview_rate));
  next_frame_ = now + period;

  std::vector<uint8_t> frame;
  try
  {
    frame = camera_->capturePreview();
    previewing_ = true;
    preview_failures_ = 0;
  }
  catch (const CameraError& error)
  {
    // The camera is busy for a moment after each shot, while it writes the
    // picture. Only many failures in a row mean that it is gone.
    if (error.isFatal() || ++preview_failures_ >= options_.max_preview_failures)
    {
      throw CameraError(error.what(), true);
    }
    warn(error.what());
    return;
  }
  if (callbacks_.on_preview)
  {
    callbacks_.on_preview(std::move(frame));
  }
}

void CameraDriver::collectPictures(Clock::time_point until)
{
  const auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(until - Clock::now());
  const auto files = camera_->waitForFiles(std::max(timeout, std::chrono::milliseconds(0)));
  for (const auto& file : files)
  {
    auto data = camera_->download(file);
    if (!options_.keep_on_camera)
    {
      try
      {
        camera_->remove(file);
      }
      catch (const CameraError& error)
      {
        if (error.isFatal())
        {
          throw;
        }
        warn(error.what());
      }
    }
    if (callbacks_.on_picture)
    {
      callbacks_.on_picture(file, std::move(data));
    }
  }
}

void CameraDriver::warn(const std::string& message) const
{
  if (callbacks_.on_warning)
  {
    callbacks_.on_warning(message);
  }
}

}  // namespace stepit_camera
