#pragma once

#include "sh/types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace sh {

struct RomSet {
  std::string set_name;
  std::string description;
  std::vector<u8> mainrom;
  std::vector<u8> audiocpu;
  std::vector<u8> mcu;
  std::vector<u8> voice;
  std::vector<u8> tiles;
  std::vector<u8> mask;
  std::vector<u8> sprites;

  static RomSet load(const std::filesystem::path& rom_dir);
};

} // namespace sh
