#include "bus/virtmap.hpp"
#include "audio/audio.hpp"
#include "machine/keycus.hpp"
#include "video/video.hpp"

#include <algorithm>
#include <cstdio>

namespace sh {

void VirtMap::reset() {
  if (workram_.size() != 0x8000)
    workram_.assign(0x8000, u8(0));
  else
    std::fill(workram_.begin(), workram_.end(), u8(0));
  if (triram_.size() != 0x800)
    triram_.assign(0x800, u8(0));
  else
    std::fill(triram_.begin(), triram_.end(), u8(0));
  if (scratch_.size() != 0x800)
    scratch_.assign(0x800, u8(0));
  else
    std::fill(scratch_.begin(), scratch_.end(), u8(0));
}

void VirtMap::attach_rom(std::vector<u8> mainrom) {
  if (mainrom.size() != 0x400000)
    throw Error("mainrom.bin must be 4 MiB");
  mainrom_ = std::move(mainrom);
}

u8 VirtMap::read(u32 addr) {
  addr &= 0x7FFFFF;
  if (addr >= 0x400000) {
    const u32 off = addr - 0x400000;
    return mainrom_[off];
  }
  if (addr >= 0x300000 && addr <= 0x307FFF)
    return workram_[addr - 0x300000];
  if (addr >= 0x2FF000 && addr <= 0x2FFFFF)
    return triram_[(addr - 0x2FF000) & 0x7FF];
  if (addr >= 0x2FC000 && addr <= 0x2FC7FF)
    return scratch_[addr - 0x2FC000];
  if (addr >= 0x2F8000 && addr <= 0x2F9FFF && key_)
    return key_->read(u16(addr - 0x2F8000));
  if (addr >= 0x2F0000 && addr <= 0x2F7FFF && video_)
    return video_->vram_read(addr - 0x2F0000);
  if (addr >= 0x2E0000 && addr <= 0x2E7FFF && video_)
    return video_->c116_read(addr - 0x2E0000);
  if (addr >= 0x2FC800 && addr <= 0x2FCFFF && video_)
    return video_->spriteram_read(addr - 0x2FC800);
  if (addr >= 0x2FD000 && addr <= 0x2FDFFF && video_)
    return video_->tmap_control_read(addr & 0x1F);
  if (addr >= 0x2FE000 && addr <= 0x2FEFFF && audio_)
    return audio_->cus30_read(addr & 0x3FF);
  return 0;
}

void VirtMap::write(u32 addr, u8 data) {
  addr &= 0x7FFFFF;
  if (addr >= 0x400000)
    return; // ROM
  if (addr >= 0x300000 && addr <= 0x307FFF) {
    workram_[addr - 0x300000] = data;
    return;
  }
  if (addr >= 0x2FF000 && addr <= 0x2FFFFF) {
    triram_[(addr - 0x2FF000) & 0x7FF] = data;
    return;
  }
  if (addr >= 0x2FC000 && addr <= 0x2FC7FF) {
    scratch_[addr - 0x2FC000] = data;
    return;
  }
  if (addr >= 0x2F8000 && addr <= 0x2F9FFF && key_) {
    key_->write(u16(addr - 0x2F8000), data);
    return;
  }
  if (addr >= 0x2F0000 && addr <= 0x2F7FFF && video_) {
    video_->vram_write(addr - 0x2F0000, data);
    return;
  }
  if (addr >= 0x2E0000 && addr <= 0x2E7FFF && video_) {
    video_->c116_write(addr - 0x2E0000, data);
    return;
  }
  if (addr >= 0x2FC800 && addr <= 0x2FCFFF && video_) {
    video_->spriteram_write(addr - 0x2FC800, data);
    return;
  }
  if (addr >= 0x2FD000 && addr <= 0x2FDFFF && video_) {
    video_->tmap_control_write(addr & 0x1F, data);
    return;
  }
  if (addr >= 0x2FE000 && addr <= 0x2FEFFF && audio_) {
    audio_->cus30_write(addr & 0x3FF, data);
    return;
  }
  // 0x2C0000 3DCS — ignore
}

} // namespace sh
