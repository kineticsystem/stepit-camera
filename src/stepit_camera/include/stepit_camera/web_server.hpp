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

#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace httplib
{
class Server;
}

namespace stepit_camera
{

/**
 * @brief An HTTP server for web pages: the camera's test page, and the
 * pictures the driver saved.
 *
 * - `GET /`: the test page, the files of `web_directory`.
 * - `GET /pictures/<name>`: a picture of `pictures_directory`, e.g.
 *   `/pictures/IMG_0042.CR2`. `Range` requests are served, so that a page can
 *   read only a part of a large file, e.g. the JPEG preview inside a RAW.
 *
 * Every response allows any origin, so that a page served by another server,
 * e.g. the final application of the rig, can read the pictures too.
 */
class WebServer
{
public:
  /// @brief Serves the given folders. The one of the pictures is created if needed; the page may be missing.
  WebServer(const std::filesystem::path& web_directory, const std::filesystem::path& pictures_directory);

  /// @brief Stops the server.
  ~WebServer();

  WebServer(const WebServer&) = delete;
  WebServer& operator=(const WebServer&) = delete;

  /// @brief Whether the test page was found, i.e. it was built.
  bool hasPage() const;

  /**
   * @brief Listen on the given address, on a thread of its own, until stop().
   * Port 0 picks a free one. Returns the port. Throws std::runtime_error if the
   * port cannot be opened, e.g. it is already in use.
   */
  int start(const std::string& host, int port);

  void stop();

private:
  std::unique_ptr<httplib::Server> server_;
  std::thread thread_;
  bool has_page_ = false;
};

}  // namespace stepit_camera
