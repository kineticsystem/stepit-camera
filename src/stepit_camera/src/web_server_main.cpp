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

#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "stepit_camera/settings.hpp"
#include "stepit_camera/web_server.hpp"

int main(int argc, char* argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("web_server");

  const auto read_only = [](const std::string& description) {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.description = description;
    descriptor.read_only = true;
    return descriptor;
  };
  const auto host = node->declare_parameter("host", "0.0.0.0", read_only("The address to listen on."));
  const auto port = node->declare_parameter("port", 8090, read_only("The HTTP port."));
  const auto web_directory =
      node->declare_parameter("web_directory", "~/ws/web/dist", read_only("The test page, as built by build.sh."));
  // The same parameter as the camera node's, so that one file sets both.
  const auto download_directory =
      node->declare_parameter("download_directory", "~/pictures", read_only("The pictures the camera node saved."));

  int exit_code = 0;
  try
  {
    stepit_camera::WebServer server(stepit_camera::expandHome(web_directory),
                                    stepit_camera::expandHome(download_directory));
    const int bound = server.start(host, static_cast<int>(port));
    if (!server.hasPage())
    {
      RCLCPP_WARN(node->get_logger(), "The test page is not built: %s is missing. Only the pictures are served.",
                  web_directory.c_str());
    }
    RCLCPP_INFO(node->get_logger(), "Serving the test page on http://localhost:%d, the pictures under /pictures", bound);
    rclcpp::spin(node);
  }
  catch (const std::exception& error)
  {
    RCLCPP_FATAL(node->get_logger(), "%s", error.what());
    exit_code = 1;
  }
  rclcpp::shutdown();
  return exit_code;
}
