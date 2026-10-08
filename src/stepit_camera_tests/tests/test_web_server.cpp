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
#include <regex>
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

// A picture the camera saved into a subfolder, the parameter folder of the
// camera node, is at its relative_path under /pictures.
TEST_F(WebServerTest, ServesAPictureInASubfolder)
{
  start();
  const auto folder = root_ / "pictures" / "2026-10-06_15-20-04" / "angle_01";
  fs::create_directories(folder);
  write(folder / "IMG_0001.CR2", "raw content");

  const auto response = client_->Get("/pictures/2026-10-06_15-20-04/angle_01/IMG_0001.CR2");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 200);
  EXPECT_EQ(response->body, "raw content");
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

// A folder of the pictures, its path ending with a slash, is listed as JSON:
// its folders and its files, with their sizes and when they changed.
TEST_F(WebServerTest, ListsAFolderOfThePictures)
{
  start();
  const auto folder = root_ / "pictures" / "series_01";
  fs::create_directories(folder / "previews");
  write(folder / "IMG_0002.CR2", "raw content");
  write(folder / "IMG_0001.JPG", "jpeg");

  const auto response = client_->Get("/pictures/series_01/");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 200);
  EXPECT_EQ(response->get_header_value("Content-Type"), "application/json");
  EXPECT_EQ(response->get_header_value("Access-Control-Allow-Origin"), "*");
  const auto& body = response->body;
  EXPECT_NE(body.find(R"("folder":"series_01")"), std::string::npos) << body;
  EXPECT_NE(body.find(R"("folders":["previews"])"), std::string::npos) << body;
  // Sorted by path, each with its size.
  const auto jpeg = body.find(R"({"path":"IMG_0001.JPG","size":4,"modified":")");
  const auto raw = body.find(R"({"path":"IMG_0002.CR2","size":11,"modified":")");
  ASSERT_NE(jpeg, std::string::npos) << body;
  ASSERT_NE(raw, std::string::npos) << body;
  EXPECT_LT(jpeg, raw);
  EXPECT_TRUE(std::regex_search(body, std::regex(R"("modified":"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z")"))) << body;
}

TEST_F(WebServerTest, ListsThePicturesFolderItself)
{
  start();
  fs::create_directories(root_ / "pictures" / "series_01");
  write(root_ / "pictures" / "IMG_0001.CR2", "raw content");

  const auto response = client_->Get("/pictures/");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 200);
  EXPECT_NE(response->body.find(R"("folder":"","folders":["series_01"],"files":[{"path":"IMG_0001.CR2")"),
            std::string::npos)
      << response->body;
}

// With ?recursive=1, everything below the folder, in one answer.
TEST_F(WebServerTest, ListsEverythingBelowAFolder)
{
  start();
  fs::create_directories(root_ / "pictures" / "series_01" / "part_2");
  write(root_ / "pictures" / "series_01" / "IMG_0001.CR2", "1");
  write(root_ / "pictures" / "series_01" / "part_2" / "IMG_0002.CR2", "22");

  const auto response = client_->Get("/pictures/?recursive=1");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 200);
  const auto& body = response->body;
  EXPECT_NE(body.find(R"("folders":["series_01","series_01/part_2"])"), std::string::npos) << body;
  EXPECT_NE(body.find(R"({"path":"series_01/IMG_0001.CR2","size":1,)"), std::string::npos) << body;
  EXPECT_NE(body.find(R"({"path":"series_01/part_2/IMG_0002.CR2","size":2,)"), std::string::npos) << body;
}

// A file still being written under a temporary name, e.g. .IMG_0001.CR2.part,
// is hidden: it is not listed, nor is a hidden folder or what it holds.
TEST_F(WebServerTest, LeavesOutHiddenFilesAndFolders)
{
  start();
  fs::create_directories(root_ / "pictures" / ".trash");
  write(root_ / "pictures" / ".trash" / "IMG_0009.CR2", "old");
  write(root_ / "pictures" / ".IMG_0001.CR2.part", "half");
  write(root_ / "pictures" / "IMG_0002.CR2", "whole");

  for (const auto* path : { "/pictures/", "/pictures/?recursive=1" })
  {
    const auto response = client_->Get(path);
    ASSERT_TRUE(response) << path;
    EXPECT_EQ(response->body.find("IMG_0009"), std::string::npos) << response->body;
    EXPECT_EQ(response->body.find("IMG_0001"), std::string::npos) << response->body;
    EXPECT_NE(response->body.find("IMG_0002"), std::string::npos) << response->body;
  }
  const auto hidden = client_->Get("/pictures/.trash/");
  ASSERT_TRUE(hidden);
  EXPECT_EQ(hidden->status, 404);
}

TEST_F(WebServerTest, ListsNothingOutsideThePictures)
{
  fs::create_directories(root_ / "outside");
  write(root_ / "outside" / "secret.txt", "secret");
  start();
  write(root_ / "pictures" / "IMG_0001.CR2", "raw content");
  fs::create_symlink(root_ / "outside", root_ / "pictures" / "link");

  for (const auto* path : { "/pictures/../", "/pictures/%2E%2E/", "/pictures/../outside/", "/pictures/link/",
                            "/pictures/missing/", "/pictures/IMG_0001.CR2/" })
  {
    const auto response = client_->Get(path);
    ASSERT_TRUE(response) << path;
    EXPECT_EQ(response->status, 404) << path;
    EXPECT_EQ(response->body.find("secret"), std::string::npos) << path;
  }
}

// The API describes itself, in OpenAPI, with every route the server has, and
// a Swagger UI page shows that description.
TEST_F(WebServerTest, DescribesItsApi)
{
  fs::remove_all(root_ / "dist");  // Even without the test page.
  start();

  const auto description = client_->Get("/openapi.json");
  ASSERT_TRUE(description);
  EXPECT_EQ(description->status, 200);
  EXPECT_EQ(description->get_header_value("Content-Type"), "application/json");
  EXPECT_NE(description->body.find(R"("openapi": "3.0.3")"), std::string::npos);
  for (const auto* path : { "/", "/pictures/{path}", "/pictures/", "/pictures/{folder}/", "/docs", "/openapi.json" })
  {
    EXPECT_NE(description->body.find("\"" + std::string(path) + "\": {"), std::string::npos) << path;
  }

  const auto page = client_->Get("/docs");
  ASSERT_TRUE(page);
  EXPECT_EQ(page->status, 200);
  EXPECT_EQ(page->get_header_value("Content-Type"), "text/html");
  EXPECT_NE(page->body.find("swagger-ui"), std::string::npos);
  EXPECT_NE(page->body.find("/openapi.json"), std::string::npos);
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
