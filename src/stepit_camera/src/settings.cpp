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

#include "stepit_camera/settings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace stepit_camera
{

namespace
{

std::string lowercase(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
  return text;
}

/// @brief The number a whole text stands for, if it is one, e.g. "5.6" or "+1".
std::optional<double> toNumber(const std::string& text)
{
  if (text.empty())
  {
    return std::nullopt;
  }
  char* end = nullptr;
  const double number = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size())
  {
    return std::nullopt;
  }
  return number;
}

}  // namespace

std::optional<std::string> matchChoice(const std::string& value, const std::vector<std::string>& choices)
{
  for (const auto& choice : choices)
  {
    if (lowercase(choice) == lowercase(value))
    {
      return choice;
    }
  }
  const auto number = toNumber(value);
  if (number)
  {
    for (const auto& choice : choices)
    {
      const auto choice_number = toNumber(choice);
      if (choice_number && std::abs(*choice_number - *number) < 1e-3)
      {
        return choice;
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> captureTargetChoice(bool memory_card, const std::vector<std::string>& choices)
{
  // A Canon EOS offers "Internal RAM" and "Memory card".
  const std::string wanted = memory_card ? "card" : "ram";
  for (const auto& choice : choices)
  {
    if (lowercase(choice).find(wanted) != std::string::npos)
    {
      return choice;
    }
  }
  return std::nullopt;
}

std::filesystem::path expandHome(const std::string& path)
{
  if (path == "~" || path.rfind("~/", 0) == 0)
  {
    const char* home = std::getenv("HOME");
    if (home != nullptr)
    {
      return path == "~" ? std::filesystem::path(home) : std::filesystem::path(home) / path.substr(2);
    }
  }
  return path;
}

std::filesystem::path savePicture(const std::filesystem::path& folder, const std::string& name,
                                  const std::vector<uint8_t>& data)
{
  std::filesystem::create_directories(folder);

  const std::filesystem::path original(name);
  auto path = folder / original.filename();
  for (int suffix = 1; std::filesystem::exists(path); ++suffix)
  {
    path = folder / (original.stem().string() + "_" + std::to_string(suffix) + original.extension().string());
  }

  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!file)
  {
    throw std::runtime_error("Cannot write " + path.string());
  }
  return path;
}

}  // namespace stepit_camera
