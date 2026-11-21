#include "video/video.hpp"

#include <algorithm>
#include <cstdio>

namespace sh {

namespace {
constexpr u16 kColorBase = 0x0800; // namcos1 tilemap palette base

u16 read_be16(const std::vector<u8>& mem, u32 byte_off) {
  if (byte_off + 1 >= mem.size())
    return 0;
  return u16((mem[byte_off] << 8) | mem[byte_off + 1]);
}
} // namespace

void Video::reset() {
  vram_.assign(0x8000, 0);
  c116_r_.assign(0x2000, 0);
  c116_g_.assign(0x2000, 0);
  c116_b_.assign(0x2000, 0);
  c116_regs_.fill(0);
  spriteram_.assign(0x800, 0);
  tmap_ctrl_.fill(0);
  const size_t pixels = size_t(kScreenW * kScreenH);
  held_frame_.assign(pixels, 0xFF000000u);
  pri_.assign(pixels, 0);
  covered_.assign(pixels, 0);
  held_nonzero_ = 0;
  have_held_ = false;
  copy_sprites_ = false;
  sprite_latch_requests_ = 0;
}

void Video::load_gfx(std::span<const u8> tiles, std::span<const u8> mask,
                     std::span<const u8> sprites) {
  tiles_.assign(tiles.begin(), tiles.end());
  mask_.assign(mask.begin(), mask.end());
  sprites_.assign(sprites.begin(), sprites.end());
}

u8 Video::vram_read(u32 off) const { return off < vram_.size() ? vram_[off] : 0; }
void Video::vram_write(u32 off, u8 data) {
  if (off < vram_.size())
    vram_[off] = data;
}
u8 Video::c116_read(u32 off) const {
  // MAME namco_c116_device::read — A11/A12 select R/G/B/regs; A13/A14 fold into pen.
  switch (off & 0x1800u) {
  case 0x0000u:
    return c116_r_[((off & 0x6000u) >> 2) | (off & 0x7FFu)];
  case 0x0800u:
    return c116_g_[((off & 0x6000u) >> 2) | (off & 0x7FFu)];
  case 0x1000u:
    return c116_b_[((off & 0x6000u) >> 2) | (off & 0x7FFu)];
  default: {
    const int reg = int((off & 0xFu) >> 1);
    if (off & 1u)
      return u8(c116_regs_[reg] & 0xFFu);
    return u8(c116_regs_[reg] >> 8);
  }
  }
}
void Video::c116_write(u32 off, u8 data) {
  switch (off & 0x1800u) {
  case 0x0000u:
    c116_r_[((off & 0x6000u) >> 2) | (off & 0x7FFu)] = data;
    return;
  case 0x0800u:
    c116_g_[((off & 0x6000u) >> 2) | (off & 0x7FFu)] = data;
    return;
  case 0x1000u:
    c116_b_[((off & 0x6000u) >> 2) | (off & 0x7FFu)] = data;
    return;
  default: {
    const int reg = int((off & 0xFu) >> 1);
    if (off & 1u)
      c116_regs_[reg] = u16((c116_regs_[reg] & 0xFF00u) | data);
    else
      c116_regs_[reg] = u16((c116_regs_[reg] & 0x00FFu) | (u16(data) << 8));
    return;
  }
  }
}
u8 Video::spriteram_read(u32 off) const {
  return off < spriteram_.size() ? spriteram_[off] : 0;
}
void Video::spriteram_write(u32 off, u8 data) {
  if (off < spriteram_.size())
    spriteram_[off] = data;
  // Write to 0x7F2 latches the CPU staging list (bytes 4..9) into 10..15.
  if (off == 0x7F2) {
    copy_sprites_ = true;
    ++sprite_latch_requests_;
  }
}
u8 Video::tmap_control_read(u32 off) const {
  return off < tmap_ctrl_.size() ? tmap_ctrl_[off] : 0;
}
void Video::tmap_control_write(u32 off, u8 data) {
  if (off < tmap_ctrl_.size())
    tmap_ctrl_[off] = data;
}

u32 Video::palette_color(u16 index) const {
  // Pen index 0..0x1FFF indexes the three 8KiB RAMs directly (MAME set_pen_color).
  const u32 pen = index & 0x1FFFu;
  if (pen >= c116_r_.size())
    return 0xFF000000u;
  const u32 r = c116_r_[pen];
  const u32 g = c116_g_[pen];
  const u32 b = c116_b_[pen];
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

u16 Video::layer_pal_base(int layer) const {
  // control[0x18..0x1d]: palette bank 0..7 → offset ((bank<<8)+0x800)
  const u8 bank = (layer >= 0 && layer < 6 && (0x18u + u32(layer)) < tmap_ctrl_.size())
                      ? (tmap_ctrl_[0x18u + u32(layer)] & 7u)
                      : 0;
  return u16(kColorBase + (u16(bank) << 8));
}

bool Video::layer_enabled(int layer) const {
  // control[0x10..0x15]: bit3 disables layer. Unprogrammed (0) → treat as enabled.
  if (layer < 0 || layer >= 6 || (0x10u + u32(layer)) >= tmap_ctrl_.size())
    return true;
  return (tmap_ctrl_[0x10u + u32(layer)] & 0x08) == 0;
}

void Video::draw_layer_fixed(std::span<u32> out, std::span<u8> pri, u32 word_base, u16 pal_base,
                             u8 priority) {
  // Fixed 36x28 tilemap (playfield 4/5). Words are big-endian in vram bytes.
  // The C123 places this map at the visible window, so pixel (0,0) is tile (0,0).
  for (int ty = 0; ty < 28; ++ty) {
    for (int tx = 0; tx < 36; ++tx) {
      const u32 word_index = word_base + u32(ty * 36 + tx);
      const u32 byte_off = word_index * 2u;
      const u16 data = read_be16(vram_, byte_off) & 0x3FFF;
      const u32 tile = data;
      for (int py = 0; py < 8; ++py) {
        // mask: 8 bytes/tile, 1bpp MSB first
        u8 mask_row = 0xFF;
        if (!mask_.empty()) {
          const u32 moff = tile * 8u + u32(py);
          if (moff < mask_.size())
            mask_row = mask_[moff];
        }
        for (int px = 0; px < 8; ++px) {
          if (((mask_row >> (7 - px)) & 1) == 0)
            continue;
          const u32 pix_off = tile * 64u + u32(py) * 8u + u32(px);
          if (pix_off >= tiles_.size())
            continue;
          const u8 pen = tiles_[pix_off];
          const int sx = tx * 8 + px;
          const int sy = ty * 8 + py;
          if (sx < 0 || sy < 0 || sx >= kScreenW || sy >= kScreenH)
            continue;
          const size_t pix = size_t(sy * kScreenW + sx);
          out[pix] = palette_color(u16(pal_base + pen));
          pri[pix] = priority;
        }
      }
    }
  }
}

void Video::draw_layer_scroll(std::span<u32> out, std::span<u8> pri, u32 word_base, int cols,
                              int rows, u16 pal_base, int origin_x, int origin_y, u8 priority) {
  const int map_w = cols * 8;
  const int map_h = rows * 8;
  const int map_w_mask = map_w - 1;
  const int map_h_mask = map_h - 1;
  // Walk the screen in tile-aligned strips so each VRAM/mask fetch is reused
  // for up to an 8x8 block instead of once per pixel.
  for (int sy = 0; sy < kScreenH;) {
    const int my = (origin_y + sy) & map_h_mask;
    const int ty = my >> 3;
    const int py0 = my & 7;
    const int y_run = std::min(8 - py0, kScreenH - sy);
    for (int sx = 0; sx < kScreenW;) {
      const int mx = (origin_x + sx) & map_w_mask;
      const int tx = mx >> 3;
      const int px0 = mx & 7;
      const int x_run = std::min(8 - px0, kScreenW - sx);
      const u16 data =
          read_be16(vram_, (word_base + u32(ty * cols + tx)) * 2u) & 0x3FFF;
      const u32 tile = data;
      const u32 tile_pix = tile * 64u;
      const bool tile_ok = tile_pix + 64u <= tiles_.size();
      for (int ry = 0; ry < y_run; ++ry) {
        const int py = py0 + ry;
        u8 mask_row = 0xFF;
        if (!mask_.empty()) {
          const u32 moff = tile * 8u + u32(py);
          if (moff < mask_.size())
            mask_row = mask_[moff];
        }
        if (mask_row == 0)
          continue;
        const size_t row = size_t((sy + ry) * kScreenW + sx);
        for (int rx = 0; rx < x_run; ++rx) {
          const int px = px0 + rx;
          if (((mask_row >> (7 - px)) & 1) == 0)
            continue;
          if (!tile_ok)
            continue;
          const u8 pen = tiles_[tile_pix + u32(py) * 8u + u32(px)];
          const size_t pix = row + size_t(rx);
          out[pix] = palette_color(u16(pal_base + pen));
          pri[pix] = priority;
        }
      }
      sx += x_run;
    }
    sy += y_run;
  }
}

void Video::draw_tilemaps(std::span<u32> out, std::span<u8> pri) {
  if (tiles_.empty() || vram_.size() < 0x8000)
    return;
  // Half-height layout (Splatterhouse): maps at 0x0000/0x1000/0x2000/0x3000 words,
  // fixed at 0x3808 / 0x3C08 words (= byte 0x7010 / 0x7810).
  // Layers paint back to front in the priority stored in control[0x10..].
  for (int priority = 0; priority < 8; ++priority) {
    for (int layer = 0; layer < 6; ++layer) {
      if (!layer_enabled(layer))
        continue;
      if (int(tmap_ctrl_[0x10u + u32(layer)] & 7u) != priority)
        continue;
      if (layer < 4) {
        const int cols = 64;
        const int rows = (layer == 3) ? 32 : 64;
        int origin_x = 0;
        int origin_y = 0;
        if (spriteram_[0x7f6] & 1u) {
          // C123 flipped offsets, followed by the cabinet's ROT180 presentation.
          static constexpr int kXAdjust[4] = {48, 46, 45, 44};
          const u32 reg = u32(layer) * 4u;
          const int scroll_x = (int(tmap_ctrl_[reg]) << 8) | int(tmap_ctrl_[reg + 1]);
          const int scroll_y = (int(tmap_ctrl_[reg + 2]) << 8) | int(tmap_ctrl_[reg + 3]);
          origin_x = -288 - kXAdjust[layer] - scroll_x;
          origin_y = -248 - scroll_y;
        }
        draw_layer_scroll(out, pri, u32(layer) * 0x1000u, cols, rows, layer_pal_base(layer),
                          origin_x, origin_y, u8(priority));
      } else {
        const u32 base = (layer == 4) ? 0x3808u : 0x3C08u;
        draw_layer_fixed(out, pri, base, layer_pal_base(layer), u8(priority));
      }
    }
  }
}

void Video::copy_sprites() {
  if (!copy_sprites_)
    return;
  for (u32 i = 0; i < 0x800; i += 16)
    for (u32 j = 10; j < 16; ++j)
      spriteram_[i + j] = spriteram_[i + j - 6];
  copy_sprites_ = false;
}

u8 Video::sprite_pen(u32 code, int x, int y) const {
  // MAME spritelayout: 32x32, 4bpp, planes STEP4(0,1).
  // X: STEP16(0,4) + STEP16(4*16*16,4); Y: STEP16(0,4*16) + STEP16(4*16*16*2,4*16).
  // Hot path: callers clip to the 32x32 tile, so skip the empty/range checks.
  const u32 tile_bytes = 512u;
  const u32 base = code * tile_bytes;
  if (base + tile_bytes > sprites_.size())
    return 0xF;
  const int xl = x & 15;
  const int yl = y & 15;
  const u32 xblock = (x >= 16) ? (4u * 16u * 16u) : 0u;
  const u32 yblock = (y >= 16) ? (4u * 16u * 16u * 2u) : 0u;
  // Two pixels share a byte: the left one is the high nibble. Reading the
  // low nibble first and then skipping odd columns ahead by 8 bits repeats
  // column 15, which is the line down Rick and the gap at a fist or foot.
  const u32 bit_index = yblock + u32(yl) * (4u * 16u) + xblock + u32(xl ^ 1) * 4u;
  return u8((sprites_[base + (bit_index / 8u)] >> (bit_index % 8u)) & 0x0F);
}

void Video::draw_sprites(std::span<u32> out, std::span<const u8> pri) {
  if (sprites_.empty() || spriteram_.size() < 0x800)
    return;
  // Higher RAM slots are drawn first and own their visible pixels.
  if (covered_.size() != size_t(kScreenW * kScreenH))
    covered_.assign(size_t(kScreenW * kScreenH), 0);
  else
    std::fill(covered_.begin(), covered_.end(), u8(0));
  static constexpr int kSpriteSize[4] = {16, 8, 32, 4};
  const int sprite_xoffs =
      int(spriteram_[0x07F5]) + (int(spriteram_[0x07F4] & 1) << 8);
  const int sprite_yoffs = int(spriteram_[0x07F7]);
  const bool flip_hw = (spriteram_[0x7f6] & 1u) != 0;
  // Last 0x20 bytes are control regs, not a sprite.
  for (int base = 0x800 - 0x20; base >= 0; base -= 0x10) {
    const u8* source = &spriteram_[size_t(base)];
    const u8 attr1 = source[10];
    const u8 attr2 = source[14];
    // Skip empty / cleared-to-$FF slots before any geometry work.
    if ((attr1 == 0 && source[11] == 0 && source[13] == 0 && source[15] == 0) ||
        (attr1 == 0xFF && source[11] == 0xFF && attr2 == 0xFF))
      continue;
    u32 color = source[12];
    const bool flipx = (attr1 & 0x20) != 0;
    const bool flipy = (attr2 & 0x01) != 0;
    const int sizex = kSpriteSize[(attr1 & 0xC0) >> 6];
    const int sizey = kSpriteSize[(attr2 & 0x06) >> 1];
    if (sizex <= 0 || sizey <= 0)
      continue;
    const int tx = int(attr1 & 0x18) & ~(sizex - 1);
    const int ty = int(attr2 & 0x18) & ~(sizey - 1);
    int sx = int(source[13]) + (int(color & 1) << 8);
    int sy = -int(source[15]) - sizey;
    u32 sprite = (u32(source[11]) & 0xFFu) + (u32(attr1 & 7u) << 8);
    color >>= 1;
    // Pen 0x7F is the shadow/highlight pass, not a solid color.
    const bool shadow = color == 0x7F;
    sx += sprite_xoffs;
    sy -= sprite_yoffs;
    sy += 1; // buffered one scanline
    // Hardware bitmap is 384x264. The visible window starts at (73, 16),
    // and our buffer is only that 288x224 view.
    int draw_x = (sx & 0x1FF) - 73;
    int draw_y = (((sy + 16) & 0xFF) - 16) - 16;
    if (flip_hw) {
      // Flipped sprite hardware and ROT180 cancel their pixel orientation.
      draw_x = (sx - 150) & 0x1ff;
      // MAME applies the one-scanline delay after the hardware flip. The
      // 224-line visible window and ROT180 leave this two-line adjustment.
      draw_y = (sy + 238) & 0xff;
      if (draw_x >= 384)
        draw_x -= 512;
      if (draw_y >= 240)
        draw_y -= 256;
    }
    // Clip the sprite rectangle to the visible buffer once.
    const int x0 = std::max(0, -draw_x);
    const int y0 = std::max(0, -draw_y);
    const int x1 = std::min(sizex, kScreenW - draw_x);
    const int y1 = std::min(sizey, kScreenH - draw_y);
    if (x0 >= x1 || y0 >= y1)
      continue;
    const u32 spri = (attr2 & 0xE0) >> 5;
    const u32 pmask = (0xFFu << (spri + 1u)) & 0xFFu;
    const u16 color_base = u16((color * 16u) & 0x7F0u);
    for (int y = y0; y < y1; ++y) {
      const int src_y = ty + (flipy ? (sizey - 1 - y) : y);
      const size_t row = size_t((draw_y + y) * kScreenW + draw_x);
      for (int x = x0; x < x1; ++x) {
        const size_t pix = row + size_t(x);
        if (covered_[pix])
          continue;
        // sprite_pri_cb: mask (0xFF << (pri+1)). Hidden where a tile left that bit set.
        if (((1u << pri[pix]) & pmask) != 0)
          continue;
        const int src_x = tx + (flipx ? (sizex - 1 - x) : x);
        const u8 pen = sprite_pen(sprite, src_x, src_y);
        if (pen == 0x0F)
          continue;
        covered_[pix] = 1;
        if (shadow) {
          const u32 px = out[pix];
          const u32 r = ((px >> 16) & 0xFF) >> 1;
          const u32 g = ((px >> 8) & 0xFF) >> 1;
          const u32 b = (px & 0xFF) >> 1;
          out[pix] = 0xFF000000u | (r << 16) | (g << 8) | b;
          continue;
        }
        // Sprite colors sit in C116 page 0 (pens 0..0x7FF).
        out[pix] = palette_color(u16(color_base + pen));
      }
    }
  }
}

void Video::render(std::span<u32> out_rgba) {
  if (out_rgba.size() < size_t(kScreenW * kScreenH))
    return;

  // Avoid scanning all of VRAM/spriteram every frame. A small sample catches
  // the boot wipe; gameplay always has dense tile content so the full count
  // is unnecessary and dominated the frame when many sprites were active.
  size_t sample_nz = 0;
  for (size_t i = 0; i < 0x200; i += 2)
    sample_nz += size_t((vram_[i] | vram_[i + 1]) != 0);
  for (size_t i = 0; i < 0x80; ++i)
    sample_nz += size_t(spriteram_[i] != 0);
  const bool looks_empty = sample_nz < 4;

  // Hold prior frame only across a near-total VRAM wipe (boot clears), not
  // across normal scene changes that merely drop tile counts.
  if (have_held_ && looks_empty && held_nonzero_ > 64) {
    std::copy_n(held_frame_.begin(), size_t(kScreenW * kScreenH), out_rgba.begin());
    return;
  }
  if (looks_empty) {
    std::fill(out_rgba.begin(), out_rgba.begin() + kScreenW * kScreenH, 0xFF000000u);
    return;
  }

  std::fill(out_rgba.begin(), out_rgba.begin() + kScreenW * kScreenH, 0xFF000000u);
  if (pri_.size() != size_t(kScreenW * kScreenH))
    pri_.assign(size_t(kScreenW * kScreenH), 0);
  else
    std::fill(pri_.begin(), pri_.end(), u8(0));
  draw_tilemaps(out_rgba, pri_);
  draw_sprites(out_rgba, pri_);

  if (held_frame_.size() != size_t(kScreenW * kScreenH))
    held_frame_.assign(size_t(kScreenW * kScreenH), 0xFF000000u);
  std::copy_n(out_rgba.begin(), size_t(kScreenW * kScreenH), held_frame_.begin());
  held_nonzero_ = sample_nz + 64; // mark as contentful for the wipe detector
  have_held_ = true;
}

} // namespace sh
