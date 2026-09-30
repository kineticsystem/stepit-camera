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

#include "stepit_camera/web_server.hpp"

#include <httplib.h>
#include <sys/socket.h>

#include <stdexcept>

namespace stepit_camera
{

WebServer::WebServer(const std::filesystem::path& web_directory, const std::filesystem::path& pictures_directory)
  : server_(std::make_unique<httplib::Server>())
{
  // The camera driver creates the folder with its first picture, which may
  // come after the server started.
  std::filesystem::create_directories(pictures_directory);
  if (!server_->set_mount_point("/pictures", pictures_directory.string()))
  {
    throw std::runtime_error("Cannot serve the pictures of " + pictures_directory.string());
  }

  has_page_ = server_->set_mount_point("/", web_directory.string());
  if (!has_page_)
  {
    const auto missing = "The test page is not built: " + web_directory.string() + " is missing.\n";
    server_->Get("/", [missing](const httplib::Request&, httplib::Response& response) {
      response.status = 404;
      response.set_content(missing, "text/plain");
    });
  }

  // The extensions are case sensitive, and a Canon writes them in capitals.
  server_->set_file_extension_and_mimetype_mapping("CR2", "image/x-canon-cr2");
  server_->set_file_extension_and_mimetype_mapping("cr2", "image/x-canon-cr2");
  server_->set_file_extension_and_mimetype_mapping("JPG", "image/jpeg");
  // Content-Range tells a page the size of a file it read a part of.
  server_->set_default_headers(
      { { "Access-Control-Allow-Origin", "*" }, { "Access-Control-Expose-Headers", "Content-Range" } });

  // SO_REUSEADDR only, so that the server can restart at once on its port. The
  // default adds SO_REUSEPORT, which lets a second server take the same port,
  // e.g. one left over from a previous launch, and share the requests with it.
  server_->set_socket_options([](socket_t socket) {
    int yes = 1;
    ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  });
}

WebServer::~WebServer()
{
  stop();
}

bool WebServer::hasPage() const
{
  return has_page_;
}

int WebServer::start(const std::string& host, int port)
{
  const int bound = port == 0 ? server_->bind_to_any_port(host) : (server_->bind_to_port(host, port) ? port : -1);
  if (bound < 0)
  {
    throw std::runtime_error("Cannot listen on " + host + ":" + std::to_string(port) +
                             ", is another server using the port?");
  }
  thread_ = std::thread([this] { server_->listen_after_bind(); });
  server_->wait_until_ready();
  return bound;
}

void WebServer::stop()
{
  server_->stop();
  if (thread_.joinable())
  {
    thread_.join();
  }
}

}  // namespace stepit_camera
