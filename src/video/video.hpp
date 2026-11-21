#pragma once

#include "sh/types.hpp"

#include <array>
#include <span>
#include <vector>

namespace sh {

class Video {
public:
  void reset();
  void load_gfx(std::span<const u8> tiles, std::span<const u8> mask, std::span<const u8> sprites);

  u8 vram_read(u32 off) const;
  void vram_write(u32 off, u8 data);
  u8 c116_read(u32 off) const;
  void c116_write(u32 off, u8 data);
  u8 spriteram_read(u32 off) const;
  void spriteram_write(u32 off, u8 data);
  void latch_sprites() { copy_sprites(); }
  u8 tmap_control_read(u32 off) const;
  void tmap_control_write(u32 off, u8 data);

  /// Render one frame into RGBA8888 (kScreenW * kScreenH).
  void render(std::span<u32> out_rgba);

  std::vector<u8>& vram() { return vram_; }
  std::span<const u8> palette_r() const { return c116_r_; }
  std::span<const u8> palette_g() const { return c116_g_; }
  std::span<const u8> palette_b() const { return c116_b_; }
  u64 sprite_latch_requests() const { return sprite_latch_requests_; }

private:
  void draw_tilemaps(std::span<u32> out, std::span<u8> pri);
  void draw_layer_fixed(std::span<u32> out, std::span<u8> pri, u32 word_base, u16 pal_base,
                        u8 priority);
  void draw_layer_scroll(std::span<u32> out, std::span<u8> pri, u32 word_base, int cols, int rows,
                         u16 pal_base, int origin_x, int origin_y, u8 priority);
  void draw_sprites(std::span<u32> out, std::span<const u8> pri);
  void copy_sprites();
  u8 sprite_pen(u32 code, int x, int y) const;
  u32 palette_color(u16 index) const;
  u16 layer_pal_base(int layer) const;
  bool layer_enabled(int layer) const;

  std::vector<u8> vram_;       // 0x8000
  // C116: 3×8KiB palette RAMs + 8×16-bit regs (MAME namco_c116).
  std::vector<u8> c116_r_;
  std::vector<u8> c116_g_;
  std::vector<u8> c116_b_;
  std::array<u16, 8> c116_regs_{};
  std::vector<u8> spriteram_;  // 0x800
  std::array<u8, 0x20> tmap_ctrl_{};

  std::vector<u8> tiles_;
  std::vector<u8> mask_;
  std::vector<u8> sprites_;
  std::vector<u8> pri_;
  std::vector<u8> covered_; // sprite occlusion, reused each frame
  std::vector<u32> held_frame_;
  size_t held_nonzero_ = 0;
  bool have_held_ = false;
  bool copy_sprites_ = false;
  u64 sprite_latch_requests_ = 0;
};

} // namespace sh
