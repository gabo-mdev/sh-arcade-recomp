#include "video/video.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {

void put_tile(sh::Video& video, unsigned word, sh::u8 code) {
  video.vram_write(word * 2 + 1, code);
}

bool check(bool condition, const char* message) {
  if (!condition)
    std::fprintf(stderr, "%s\n", message);
  return condition;
}

} // namespace

int main() {
  sh::Video video;
  video.reset();
  std::vector<sh::u8> tiles(128, 0);
  std::vector<sh::u8> mask(16, 0);
  for (unsigned i = 64; i < 128; ++i)
    tiles[i] = 1;
  for (unsigned i = 8; i < 16; ++i)
    mask[i] = 0xff;
  video.load_gfx(tiles, mask, {});
  for (int layer = 1; layer < 6; ++layer)
    video.tmap_control_write(0x10 + layer, 0x08);

  put_tile(video, 0, 1);
  // Ensure render() does not take its near-empty VRAM shortcut.
  for (unsigned word = 1; word < 9; ++word)
    put_tile(video, word, 2);
  std::array<sh::u32, sh::kScreenW * sh::kScreenH> frame{};
  video.render(frame);
  if (!check(frame[0] == 0xff000000u, "black palette entry was replaced with gray"))
    return 1;

  video.c116_write(0x2001, 0x7f); // layer 0 pen 1, red component
  video.render(frame);
  if (!check(frame[0] == 0xff7f0000u, "programmed palette entry was not rendered"))
    return 1;

  std::vector<sh::u8> sprites(1024, 0);
  for (unsigned i = 512; i < 1024; ++i)
    sprites[i] = 0x11;
  video.load_gfx(tiles, mask, sprites);
  video.c116_write(0x1101, 0x7f); // sprite color 0x10, pen 1, blue component
  video.tmap_control_write(0x10, 3); // tile priority 3
  video.spriteram_write(4, 0);      // 16x16, sprite bank 0
  video.spriteram_write(5, 1);      // sprite code 1
  video.spriteram_write(6, 0x20);   // color 0x10
  video.spriteram_write(7, 73);     // visible x=0
  video.spriteram_write(8, 0x20);   // sprite priority 1
  video.spriteram_write(9, 225);    // visible y=0
  video.spriteram_write(0x7f2, 1);  // request sprite-list latch
  video.render(frame);
  if (!check(frame[0] == 0xff7f0000u, "priority-1 sprite covered priority-3 tile"))
    return 1;

  video.spriteram_write(8, 0xe0); // sprite priority 7, above tile priority 3
  video.spriteram_write(0x7f2, 1);
  video.latch_sprites();
  video.render(frame);
  if (!check(frame[0] == 0xff00007fu, "priority-7 sprite did not cover tile"))
    return 1;
  video.spriteram_write(7, 100); // staging changes without a latch request
  video.latch_sprites();
  video.render(frame);
  if (!check(frame[0] == 0xff00007fu, "sprite staging changed the active list without strobe"))
    return 1;

  video.spriteram_write(7, 73);
  video.c116_write(0x0911, 0x7f); // sprite color 0x11, pen 1, green component
  video.spriteram_write(16 + 4, 0);
  video.spriteram_write(16 + 5, 1);
  video.spriteram_write(16 + 6, 0x22);
  video.spriteram_write(16 + 7, 73);
  video.spriteram_write(16 + 8, 0xe0);
  video.spriteram_write(16 + 9, 225);
  video.spriteram_write(0x7f2, 1);
  video.latch_sprites();
  video.render(frame);
  if (!check(frame[0] == 0xff007f00u, "rear sprite overwrote front sprite"))
    return 1;

  sh::Video zero_tile;
  zero_tile.reset();
  std::vector<sh::u8> zero_tiles(192, 0);
  std::vector<sh::u8> zero_mask(24, 0);
  for (unsigned i = 0; i < 8; ++i)
    zero_mask[i] = 0xff;
  zero_tile.load_gfx(zero_tiles, zero_mask, {});
  for (int layer = 1; layer < 6; ++layer)
    zero_tile.tmap_control_write(0x10 + layer, 0x08);
  for (unsigned word = 1; word < 9; ++word)
    put_tile(zero_tile, word, 2);
  zero_tile.c116_write(0x2800, 0x60); // layer 0 pen 0, green component
  zero_tile.render(frame);
  if (!check(frame[0] == 0xff006000u, "opaque mask of tile 0 lost pen 0"))
    return 1;

  sh::Video flipped;
  flipped.reset();
  flipped.load_gfx(tiles, mask, sprites);
  for (int layer = 1; layer < 6; ++layer)
    flipped.tmap_control_write(0x10 + layer, 0x08);
  flipped.tmap_control_write(0x01, 0xb0);
  flipped.tmap_control_write(0x03, 0xf0);
  flipped.spriteram_write(0x7f6, 1); // Splatterhouse cabinet flip
  flipped.spriteram_write(0x7f5, 150);
  flipped.spriteram_write(0x7f7, 238);
  put_tile(flipped, 3 * 64, 1);      // visible at (0,0) after flip + ROT180
  for (unsigned word = 1; word < 9; ++word)
    put_tile(flipped, word, 2);
  flipped.c116_write(0x2001, 0x7f);
  flipped.render(frame);
  if (!check(frame[0] == 0xff7f0000u, "flipped C123 scroll did not reach visible origin"))
    return 1;

  flipped.c116_write(0x1101, 0x7f);
  flipped.spriteram_write(4, 0);
  flipped.spriteram_write(5, 1);
  flipped.spriteram_write(6, 0x20);
  flipped.spriteram_write(7, 0);    // visible x=0 when cabinet is flipped
  flipped.spriteram_write(8, 0xe0);
  flipped.spriteram_write(9, 241);
  flipped.spriteram_write(0x7f2, 1);
  flipped.latch_sprites();
  flipped.render(frame);
  if (!check(frame[0] == 0xff00007fu, "flipped sprite did not reach visible origin"))
    return 1;
  return 0;
}
