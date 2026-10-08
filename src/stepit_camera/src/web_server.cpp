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

#include "stepit_camera/openapi.hpp"

#include <httplib.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace stepit_camera
{
namespace
{
namespace fs = std::filesystem;

// Swagger UI, from jsdelivr, at a fixed version, showing /openapi.json.
constexpr const char* kSwaggerPage = R"html(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>StepIt Camera web server</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.17.14/swagger-ui.css">
</head>
<body>
<div id="swagger-ui"></div>
<script src="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.17.14/swagger-ui-bundle.js"></script>
<script>SwaggerUIBundle({ url: "/openapi.json", dom_id: "#swagger-ui" });</script>
</body>
</html>
)html";

/// @brief A text as a JSON string, quotes included.
std::string jsonString(const std::string& text)
{
  std::string out = "\"";
  for (const char c : text)
  {
    if (c == '"' || c == '\\')
    {
      out += '\\';
      out += c;
    }
    else if (static_cast<unsigned char>(c) < 0x20)
    {
      char escaped[8];
      std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(c));
      out += escaped;
    }
    else
    {
      out += c;
    }
  }
  return out + "\"";
}

/// @brief When a file last changed, in ISO 8601 and UTC, e.g. 2026-10-06T15:20:04Z; empty if unknown.
std::string modified(const fs::path& path)
{
  struct stat status
  {
  };
  if (::stat(path.c_str(), &status) != 0)
  {
    return "";
  }
  std::tm utc{};
  gmtime_r(&status.st_mtime, &utc);
  char text[32];
  std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return text;
}

/// @brief Whether a file or a folder is hidden: its name starts with a dot, e.g. a file still being written under
/// a temporary name.
bool hidden(const fs::path& path)
{
  const auto name = path.filename().string();
  return !name.empty() && name[0] == '.';
}

/// @brief The folder `relative` names under `root`, if it is one and it is under `root`, even through links.
std::optional<fs::path> folderUnder(const fs::path& root, const std::string& relative)
{
  const fs::path path(relative);
  if (path.is_absolute())
  {
    return std::nullopt;
  }
  for (const auto& part : path)
  {
    if (part == ".." || hidden(part))
    {
      return std::nullopt;
    }
  }
  std::error_code error;
  const auto folder = fs::canonical(root / path, error);
  const auto base = fs::canonical(root, error);
  if (error || !fs::is_directory(folder, error))
  {
    return std::nullopt;
  }
  if (std::mismatch(base.begin(), base.end(), folder.begin(), folder.end()).first != base.end())
  {
    return std::nullopt;
  }
  return folder;
}

/// @brief The folders and files in `folder`, or below it if `recursive`, as JSON, their paths relative to it.
std::string listing(const fs::path& folder, const std::string& relative, bool recursive)
{
  struct File
  {
    std::string path;
    std::uintmax_t size;
    std::string modified;
  };
  std::vector<std::string> folders;
  std::vector<File> files;
  std::error_code error;
  const auto add = [&](const fs::directory_entry& entry) {
    const auto path = entry.path().lexically_relative(folder).generic_string();
    if (entry.is_directory(error))
    {
      folders.push_back(path);
    }
    else if (entry.is_regular_file(error))
    {
      files.push_back({ path, entry.file_size(error), modified(entry.path()) });
    }
  };
  if (recursive)
  {
    for (auto it = fs::recursive_directory_iterator(folder, error); it != fs::recursive_directory_iterator();
         it.increment(error))
    {
      if (hidden(it->path()))
      {
        it.disable_recursion_pending();
        continue;
      }
      add(*it);
    }
  }
  else
  {
    for (const auto& entry : fs::directory_iterator(folder, error))
    {
      if (!hidden(entry.path()))
      {
        add(entry);
      }
    }
  }
  std::sort(folders.begin(), folders.end());
  std::sort(files.begin(), files.end(), [](const File& a, const File& b) { return a.path < b.path; });

  std::ostringstream json;
  json << "{\"folder\":" << jsonString(relative) << ",\"folders\":[";
  for (size_t i = 0; i < folders.size(); ++i)
  {
    json << (i == 0 ? "" : ",") << jsonString(folders[i]);
  }
  json << "],\"files\":[";
  for (size_t i = 0; i < files.size(); ++i)
  {
    json << (i == 0 ? "" : ",") << "{\"path\":" << jsonString(files[i].path) << ",\"size\":" << files[i].size
         << ",\"modified\":" << jsonString(files[i].modified) << "}";
  }
  json << "]}";
  return json.str();
}
}  // namespace

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

  // A folder of the pictures, its path ending with a slash, e.g. /pictures/ or
  // /pictures/2026-10-06/: what it holds, as JSON. The mount point above
  // serves files only, and passes a folder on to this handler.
  server_->Get(R"(/pictures/((?:.+/)?))", [pictures_directory](const httplib::Request& request,
                                                               httplib::Response& response) {
    auto relative = request.matches[1].str();
    if (!relative.empty())
    {
      relative.pop_back();  // The slash at the end.
    }
    const auto folder = folderUnder(pictures_directory, relative);
    if (!folder)
    {
      response.status = 404;
      response.set_content("No such folder of pictures: " + relative + "\n", "text/plain");
      return;
    }
    const auto recursive = request.get_param_value("recursive");
    response.set_content(listing(*folder, relative, recursive == "1" || recursive == "true"), "application/json");
  });

  // The description of this API, and a Swagger UI page to read and try it,
  // which the browser loads from a CDN: nothing more to install here.
  server_->Get("/openapi.json", [](const httplib::Request&, httplib::Response& response) {
    response.set_content(kOpenApi, "application/json");
  });
  server_->Get("/docs", [](const httplib::Request&, httplib::Response& response) {
    response.set_content(kSwaggerPage, "text/html");
  });

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
