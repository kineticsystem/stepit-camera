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

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <stepit_camera/settings.hpp>

namespace stepit_camera::test
{

namespace fs = std::filesystem;

const std::vector<std::string> APERTURES = { "4", "4.5", "5.6", "8", "11" };
const std::vector<std::string> SHUTTER_SPEEDS = { "bulb", "30", "0.3", "1/4", "1/125", "1/8000" };
const std::vector<std::string> ISOS = { "Auto", "100", "400" };
const std::vector<std::string> COMPENSATIONS = { "-1", "-0.6", "-0.3", "0", "0.3", "0.6", "1" };

TEST(MatchChoice, AValueWrittenAsTheCameraWritesItIsItsChoice)
{
  EXPECT_EQ(matchChoice("1/125", SHUTTER_SPEEDS), "1/125");
  EXPECT_EQ(matchChoice("5.6", APERTURES), "5.6");
}

TEST(MatchChoice, ANumberMatchesTheChoiceWithTheSameValue)
{
  // As `ros2 param set` sends them: 8 as an integer, 5.6 as a double.
  EXPECT_EQ(matchChoice("8.0", APERTURES), "8");
  EXPECT_EQ(matchChoice("5.60", APERTURES), "5.6");
  EXPECT_EQ(matchChoice("+1", COMPENSATIONS), "1");
  EXPECT_EQ(matchChoice("-0.3", COMPENSATIONS), "-0.3");
}

TEST(MatchChoice, TheLetterCaseDoesNotMatter)
{
  EXPECT_EQ(matchChoice("auto", ISOS), "Auto");
  EXPECT_EQ(matchChoice("BULB", SHUTTER_SPEEDS), "bulb");
}

TEST(MatchChoice, AFractionIsNotConfusedWithANumber)
{
  // 1/4 is not the number 1, nor 0.25 the choice 1/4: shutter speeds are
  // compared as text unless both sides are numbers.
  EXPECT_FALSE(matchChoice("1", SHUTTER_SPEEDS));
  EXPECT_FALSE(matchChoice("0.25", SHUTTER_SPEEDS));
}

TEST(MatchChoice, AValueTheCameraDoesNotOfferHasNoChoice)
{
  EXPECT_FALSE(matchChoice("7", APERTURES));
  EXPECT_FALSE(matchChoice("", APERTURES));
  EXPECT_FALSE(matchChoice("f/8", APERTURES));
}

TEST(CaptureTarget, TheMemoryCardOrTheCamerasMemory)
{
  const std::vector<std::string> choices = { "Internal RAM", "Memory card" };
  EXPECT_EQ(captureTargetChoice(true, choices), "Memory card");
  EXPECT_EQ(captureTargetChoice(false, choices), "Internal RAM");
  EXPECT_FALSE(captureTargetChoice(true, { "Internal RAM" }));
}

TEST(ExpandHome, ALeadingTildeIsTheHomeFolder)
{
  const fs::path home = std::getenv("HOME");
  EXPECT_EQ(expandHome("~/pictures"), home / "pictures");
  EXPECT_EQ(expandHome("~"), home);
  EXPECT_EQ(expandHome("/tmp/pictures"), fs::path("/tmp/pictures"));
  EXPECT_EQ(expandHome("pictures/~"), fs::path("pictures/~"));
}

class SavePicture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    folder_ = fs::temp_directory_path() / ("stepit_camera_test_" + std::to_string(::getpid()));
    fs::remove_all(folder_);
  }

  void TearDown() override
  {
    fs::remove_all(folder_);
  }

  static std::vector<uint8_t> read(const fs::path& path)
  {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  }

  fs::path folder_;
};

TEST_F(SavePicture, ThePictureIsSavedUnderItsNameInANewFolder)
{
  const std::vector<uint8_t> data = { 0xff, 0xd8, 0x00, 0xff, 0xd9 };

  const auto path = savePicture(folder_ / "session", "IMG_0001.JPG", data);

  EXPECT_EQ(path, folder_ / "session" / "IMG_0001.JPG");
  EXPECT_EQ(read(path), data);
}

TEST_F(SavePicture, APictureNeverOverwritesAnother)
{
  const auto first = savePicture(folder_, "IMG_0001.JPG", { 1 });
  const auto second = savePicture(folder_, "IMG_0001.JPG", { 2 });
  const auto third = savePicture(folder_, "IMG_0001.JPG", { 3 });

  EXPECT_EQ(first.filename(), "IMG_0001.JPG");
  EXPECT_EQ(second.filename(), "IMG_0001_1.JPG");
  EXPECT_EQ(third.filename(), "IMG_0001_2.JPG");
  EXPECT_EQ(read(first), std::vector<uint8_t>{ 1 });
  EXPECT_EQ(read(third), std::vector<uint8_t>{ 3 });
}

TEST_F(SavePicture, TheFolderOfTheCameraIsNotPartOfTheName)
{
  const auto path = savePicture(folder_, "../IMG_0002.CR2", { 1 });
  EXPECT_EQ(path, folder_ / "IMG_0002.CR2");
}

}  // namespace stepit_camera::test
