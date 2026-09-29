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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "stepit_camera/camera.hpp"

namespace stepit_camera
{

struct DriverOptions
{
  /// @brief The frames of the live view per second, at most. The camera may deliver fewer.
  double preview_rate = 10.0;
  /// @brief How long to wait before trying to open the camera again.
  std::chrono::milliseconds reconnect_period{ 2000 };
  /// @brief How long to wait for a picture at a time, when not streaming. It bounds how long a task waits to run.
  std::chrono::milliseconds poll_period{ 100 };
  /// @brief After how many failed frames in a row the connection is considered lost.
  int max_preview_failures = 5;
  /// @brief Leave the pictures on the camera once downloaded, or delete them.
  bool keep_on_camera = true;
};

/**
 * @brief Runs a camera on a thread of its own.
 *
 * libgphoto2 is not thread safe, and a camera answers one request at a time,
 * so a single thread does everything with it, in a loop:
 *
 * 1. open the camera if it is not, and try again later if there is none;
 * 2. run the tasks other threads queued with run(), e.g. change a setting;
 * 3. when streaming, capture a frame of the live view;
 * 4. wait for the camera to report new pictures, until the next frame is due,
 *    and download them.
 *
 * The camera is triggered by an external device, not by the driver: the
 * driver only notices the pictures it takes. A test shot, run() with
 * Camera::trigger(), is the one exception, and its pictures come the same way.
 * The results are handed over through callbacks, which run on the driver's
 * thread.
 */
class CameraDriver
{
public:
  struct Callbacks
  {
    /// @brief The camera was opened. Receives the camera, to configure it.
    std::function<void(Camera&)> on_connected;
    /// @brief The connection was lost, for the given reason.
    std::function<void(const std::string&)> on_disconnected;
    /// @brief A frame of the live view, as a JPEG image.
    std::function<void(std::vector<uint8_t>&&)> on_preview;
    /// @brief A picture the camera took, downloaded.
    std::function<void(const CameraFile&, std::vector<uint8_t>&&)> on_picture;
    /// @brief Something failed, but the connection still works.
    std::function<void(const std::string&)> on_warning;
  };

  CameraDriver(std::unique_ptr<Camera> camera, const DriverOptions& options, Callbacks callbacks);

  /// @brief Stops the thread, switching the live view off and closing the camera.
  ~CameraDriver();

  CameraDriver(const CameraDriver&) = delete;
  CameraDriver& operator=(const CameraDriver&) = delete;

  /// @brief Start or stop the live view. It is remembered while the camera is disconnected.
  void setStreaming(bool streaming);
  bool isStreaming() const;

  bool isConnected() const;

  /**
   * @brief Run a function with the camera on the driver's thread, and return
   * its result.
   *
   * Throws CameraError when the camera is not connected, when it is lost
   * before the function runs, or when the function takes longer than the
   * timeout. Exceptions thrown by the function reach the caller.
   */
  template <typename Function>
  auto run(Function function,
           std::chrono::milliseconds timeout = std::chrono::seconds(5)) -> decltype(function(std::declval<Camera&>()))
  {
    using Result = decltype(function(std::declval<Camera&>()));
    if (std::this_thread::get_id() == thread_.get_id())
    {
      // Already on the driver's thread, e.g. from a callback: queuing the
      // function would wait for this very thread.
      return function(*camera_);
    }
    auto task = std::make_shared<std::packaged_task<Result(Camera&)>>(std::move(function));
    auto future = task->get_future();
    post([task](Camera& camera) { (*task)(camera); });
    if (future.wait_for(timeout) != std::future_status::ready)
    {
      throw CameraError("The camera did not answer in time");
    }
    try
    {
      return future.get();
    }
    catch (const std::future_error&)
    {
      // The task was dropped without running, see dropTasks().
      throw CameraError("The camera is not connected");
    }
  }

private:
  using Task = std::function<void(Camera&)>;

  /// @brief Queue a task, or throw CameraError if the camera is not connected.
  void post(Task task);

  void loop();

  /// @brief Open the camera. Returns false if it cannot be opened.
  bool connect();

  /// @brief Close the camera after the connection was lost.
  void disconnect(const std::string& reason);

  /// @brief Run the queued tasks.
  void runTasks();

  /// @brief Drop the queued tasks, so that their callers learn the camera is gone.
  void dropTasks();

  /// @brief Capture and publish a frame of the live view if one is due.
  void stream(std::chrono::steady_clock::time_point now);

  /// @brief Wait for new pictures until the given time, and download them.
  void collectPictures(std::chrono::steady_clock::time_point until);

  void warn(const std::string& message) const;

  std::unique_ptr<Camera> camera_;
  const DriverOptions options_;
  const Callbacks callbacks_;

  std::atomic<bool> running_{ true };
  std::atomic<bool> connected_{ false };
  std::atomic<bool> streaming_{ false };

  /// @brief Whether the live view of the camera is on. Only used by the driver's thread.
  bool previewing_ = false;
  int preview_failures_ = 0;
  std::chrono::steady_clock::time_point next_frame_;

  std::mutex mutex_;
  /// @brief Wakes the thread up while it waits to reconnect, to stop it.
  std::condition_variable wake_up_;
  std::deque<Task> tasks_;

  /// @brief Last, so that it starts once everything else is initialized.
  std::thread thread_;
};

}  // namespace stepit_camera
