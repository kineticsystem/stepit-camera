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

#include <gtest/gtest.h>
#include <httplib.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <stepit_camera/web_server.hpp>

namespace stepit_camera::test
{

namespace fs = std::filesystem;

/// @brief A web server on a free port, over a page and a pictures folder of its own.
class WebServerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    root_ = fs::temp_directory_path() / ("stepit_web_" + std::to_string(::getpid()) + "_" + info->name());
    fs::remove_all(root_);
    fs::create_directories(root_ / "dist");
    write(root_ / "dist" / "index.html", "<html>test page</html>");
  }

  void TearDown() override
  {
    server_.reset();
    fs::remove_all(root_);
  }

  void start()
  {
    server_ = std::make_unique<WebServer>(root_ / "dist", root_ / "pictures");
    client_ = std::make_unique<httplib::Client>("127.0.0.1", server_->start("127.0.0.1", 0));
  }

  static void write(const fs::path& path, const std::string& content)
  {
    std::ofstream(path, std::ios::binary) << content;
  }

  fs::path root_;
  std::unique_ptr<WebServer> server_;
  std::unique_ptr<httplib::Client> client_;
};

TEST_F(WebServerTest, ServesThePage)
{
  start();
  EXPECT_TRUE(server_->hasPage());

  const auto response = client_->Get("/");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 200);
  EXPECT_EQ(response->body, "<html>test page</html>");
  EXPECT_EQ(response->get_header_value("Content-Type"), "text/html");
}

TEST_F(WebServerTest, ServesAPictureSavedAfterItStarted)
{
  start();
  write(root_ / "pictures" / "IMG_0001.CR2", "raw content");

  const auto response = client_->Get("/pictures/IMG_0001.CR2");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 200);
  EXPECT_EQ(response->body, "raw content");
  EXPECT_EQ(response->get_header_value("Content-Type"), "image/x-canon-cr2");
  EXPECT_EQ(response->get_header_value("Access-Control-Allow-Origin"), "*");
}

TEST_F(WebServerTest, ServesAPartOfAPicture)
{
  start();
  write(root_ / "pictures" / "IMG_0001.CR2", "0123456789");

  const auto response = client_->Get("/pictures/IMG_0001.CR2", { httplib::make_range_header({ { 2, 5 } }) });
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 206);
  EXPECT_EQ(response->body, "2345");
  EXPECT_EQ(response->get_header_value("Content-Range"), "bytes 2-5/10");
  EXPECT_EQ(response->get_header_value("Access-Control-Expose-Headers"), "Content-Range");
}

TEST_F(WebServerTest, ServesNothingOutsideItsFolders)
{
  write(root_ / "secret.txt", "secret");
  start();

  for (const auto* path : { "/pictures/../secret.txt", "/../secret.txt", "/pictures/%2E%2E/secret.txt" })
  {
    const auto response = client_->Get(path);
    ASSERT_TRUE(response) << path;
    EXPECT_EQ(response->status, 404) << path;
    EXPECT_EQ(response->body.find("secret"), std::string::npos) << path;
  }
  const auto missing = client_->Get("/pictures/IMG_9999.JPG");
  ASSERT_TRUE(missing);
  EXPECT_EQ(missing->status, 404);
}

TEST_F(WebServerTest, SaysWhenThePageIsNotBuilt)
{
  fs::remove_all(root_ / "dist");
  start();
  EXPECT_FALSE(server_->hasPage());

  const auto response = client_->Get("/");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 404);
  EXPECT_NE(response->body.find("not built"), std::string::npos);
}

TEST_F(WebServerTest, FailsOnAPortInUse)
{
  start();
  WebServer other(root_ / "dist", root_ / "pictures");
  EXPECT_THROW(other.start("127.0.0.1", client_->port()), std::runtime_error);
}

}  // namespace stepit_camera::test
