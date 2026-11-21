#include "machine/romset.hpp"

#include <fstream>

namespace sh {
namespace fs = std::filesystem;

namespace {

std::vector<u8> read_all(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    throw Error("Cannot open " + p.string());
  in.seekg(0, std::ios::end);
  const auto n = static_cast<size_t>(in.tellg());
  in.seekg(0);
  std::vector<u8> buf(n);
  in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(n));
  return buf;
}

} // namespace

RomSet RomSet::load(const fs::path& rom_dir) {
  RomSet rs;
  const auto set_txt = rom_dir / "set.txt";
  if (fs::exists(set_txt)) {
    std::ifstream in(set_txt);
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("set=", 0) == 0)
        rs.set_name = line.substr(4);
      else if (line.rfind("desc=", 0) == 0)
        rs.description = line.substr(5);
    }
  }
  rs.mainrom = read_all(rom_dir / "mainrom.bin");
  rs.audiocpu = read_all(rom_dir / "audiocpu.bin");
  rs.mcu = read_all(rom_dir / "mcu.bin");
  rs.voice = read_all(rom_dir / "voice.bin");
  rs.tiles = read_all(rom_dir / "c123tmap.bin");
  rs.mask = read_all(rom_dir / "c123mask.bin");
  rs.sprites = read_all(rom_dir / "sprites.bin");
  if (rs.mainrom.size() != 0x400000)
    throw Error("mainrom.bin size mismatch");
  if (rs.mcu.size() < 0x1000)
    throw Error("mcu.bin too small");
  return rs;
}

} // namespace sh
