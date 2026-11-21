#include "machine/romset.hpp"
#include "video/video.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::vector<sh::u8> read_exact(const fs::path& path, size_t size) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw sh::Error("Cannot open " + path.string());
  std::vector<sh::u8> bytes(size);
  in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  if (in.gcount() != static_cast<std::streamsize>(size) || in.peek() != EOF)
    throw sh::Error("Wrong size: " + path.string());
  return bytes;
}

int main(int argc, char** argv) {
  if (argc != 5 && argc != 6) {
    std::fprintf(stderr, "usage: sh_render_state <rom_dir> <mame_dump_dir> <frame> <out.ppm> [no-sprites]\n");
    return 2;
  }
  if (argc == 6 && std::string(argv[5]) != "no-sprites") {
    std::fprintf(stderr, "unknown option: %s\n", argv[5]);
    return 2;
  }
  try {
    const auto roms = sh::RomSet::load(argv[1]);
    const fs::path dir = argv[2];
    const int frame = std::stoi(argv[3]);
    char prefix[32];
    std::snprintf(prefix, sizeof(prefix), "frame_%05d_", frame);
    auto read = [&](const char* name, size_t size) {
      return read_exact(dir / (std::string(prefix) + name + ".bin"), size);
    };
    const auto vram = read("vram", 0x8000);
    const auto palette = read("palette", 0x8000);
    const auto sprites = read("sprites", 0x800);
    const auto control = read("control", 0x20);
    sh::Video video;
    video.reset();
    video.load_gfx(roms.tiles, roms.mask, roms.sprites);
    for (sh::u32 i = 0; i < vram.size(); ++i)
      video.vram_write(i, vram[i]);
    for (sh::u32 i = 0; i < palette.size(); ++i)
      video.c116_write(i, palette[i]);
    const bool no_sprites = argc == 6;
    for (sh::u32 i = 0; i < sprites.size(); ++i)
      video.spriteram_write(i, no_sprites && i < 0x7e0 ? 0 : sprites[i]);
    for (sh::u32 i = 0; i < control.size(); ++i)
      video.tmap_control_write(i, control[i]);
    std::vector<sh::u32> pixels(sh::kScreenW * sh::kScreenH);
    video.render(pixels);
    std::ofstream out(argv[4], std::ios::binary);
    if (!out)
      throw sh::Error("Cannot write output");
    out << "P6\n" << sh::kScreenW << ' ' << sh::kScreenH << "\n255\n";
    for (auto px : pixels) {
      const char rgb[3] = {char(px >> 16), char(px >> 8), char(px)};
      out.write(rgb, 3);
    }
    if (!out)
      throw sh::Error("Cannot finish output");
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
