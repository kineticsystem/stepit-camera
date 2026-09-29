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

#include <chrono>
#include <thread>

#include <stepit_camera/fake_camera.hpp>

namespace stepit_camera::test
{

using std::chrono::milliseconds;

bool isJpeg(const std::vector<uint8_t>& data)
{
  return data.size() > 4 && data[0] == 0xff && data[1] == 0xd8 && data[data.size() - 2] == 0xff &&
         data[data.size() - 1] == 0xd9;
}

TEST(FakeCamera, ItMustBeOpenedFirst)
{
  FakeCamera camera;
  EXPECT_FALSE(camera.isOpen());
  EXPECT_THROW(camera.capturePreview(), CameraError);

  camera.open();
  EXPECT_TRUE(camera.isOpen());
  EXPECT_NO_THROW(camera.capturePreview());
}

TEST(FakeCamera, TheLiveViewIsAMovingPicture)
{
  FakeCamera camera;
  camera.open();

  const auto first = camera.capturePreview();
  const auto second = camera.capturePreview();

  EXPECT_TRUE(isJpeg(first));
  EXPECT_NE(first, second);
  EXPECT_TRUE(camera.isPreviewing());
  camera.stopPreview();
  EXPECT_FALSE(camera.isPreviewing());
}

TEST(FakeCamera, ASettingOnlyTakesOneOfItsChoices)
{
  FakeCamera camera;
  camera.open();

  camera.setSetting("iso", "800");
  EXPECT_EQ(camera.getSetting("iso"), "800");

  try
  {
    camera.setSetting("iso", "123");
    FAIL() << "An ISO of 123 was accepted";
  }
  catch (const CameraError& error)
  {
    EXPECT_FALSE(error.isFatal());
  }
  EXPECT_EQ(camera.getSetting("iso"), "800");
  EXPECT_THROW(camera.getSetting("focus"), CameraError);
}

TEST(FakeCamera, ATriggerStoresAPictureAndReportsIt)
{
  FakeCamera camera;
  camera.open();

  EXPECT_TRUE(camera.waitForFiles(milliseconds(0)).empty());
  camera.trigger();

  const auto files = camera.waitForFiles(milliseconds(0));
  ASSERT_EQ(files.size(), 1u);
  EXPECT_EQ(files[0].name, "IMG_0001.JPG");
  EXPECT_TRUE(isJpeg(camera.download(files[0])));
  // Reported once only.
  EXPECT_TRUE(camera.waitForFiles(milliseconds(0)).empty());
}

TEST(FakeCamera, RawAndJpegAreTwoFiles)
{
  FakeCamera camera;
  camera.open();
  camera.trigger(true);

  const auto files = camera.waitForFiles(milliseconds(0));
  ASSERT_EQ(files.size(), 2u);
  EXPECT_EQ(files[0].name, "IMG_0001.CR2");
  EXPECT_EQ(files[1].name, "IMG_0001.JPG");
  EXPECT_NE(camera.download(files[0]), camera.download(files[1]));
}

TEST(FakeCamera, WaitingEndsAsSoonAsAPictureIsTaken)
{
  FakeCamera camera;
  camera.open();

  std::thread trigger([&camera] {
    std::this_thread::sleep_for(milliseconds(50));
    camera.trigger();
  });
  const auto start = std::chrono::steady_clock::now();
  const auto files = camera.waitForFiles(std::chrono::seconds(10));
  trigger.join();

  EXPECT_EQ(files.size(), 1u);
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
}

TEST(FakeCamera, ADeletedFileIsGone)
{
  FakeCamera camera;
  camera.open();
  camera.trigger();
  const auto file = camera.waitForFiles(milliseconds(0)).at(0);

  camera.remove(file);

  EXPECT_TRUE(camera.files().empty());
  EXPECT_THROW(camera.download(file), CameraError);
}

TEST(FakeCamera, UnpluggedItFailsFatally)
{
  FakeCamera camera;
  camera.open();
  camera.setConnected(false);

  try
  {
    camera.capturePreview();
    FAIL() << "An unplugged camera captured a frame";
  }
  catch (const CameraError& error)
  {
    EXPECT_TRUE(error.isFatal());
  }
  EXPECT_THROW(camera.open(), CameraError);

  camera.setConnected(true);
  EXPECT_NO_THROW(camera.open());
}

TEST(FakeCamera, AShotBreaksTheLiveViewUntilItIsSwitchedOff)
{
  FakeCamera camera;
  camera.open();
  camera.capturePreview();

  camera.trigger();

  EXPECT_THROW(camera.capturePreview(), CameraError);
  camera.stopPreview();
  EXPECT_FALSE(camera.capturePreview().empty());
}

}  // namespace stepit_camera::test
