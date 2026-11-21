#pragma once

#include "sh/types.hpp"

#include <array>
#include <functional>
#include <vector>

namespace sh {

class KeyCus;
class Video;
class AudioSystem;

/// 23-bit Namco System 1 virtual address space behind CUS117.
class VirtMap {
public:
  using KeyHandler = std::function<u8(u16 off)>;
  using KeyWrite = std::function<void(u16 off, u8 data)>;

  void reset();
  void attach_rom(std::vector<u8> mainrom);
  void attach_video(Video* video) { video_ = video; }
  void attach_audio(AudioSystem* audio) { audio_ = audio; }
  void attach_keycus(KeyCus* key) { key_ = key; }

  u8 read(u32 addr);
  void write(u32 addr, u8 data);

  std::vector<u8>& triram() { return triram_; }
  std::vector<u8>& workram() { return workram_; }
  std::vector<u8>& scratch() { return scratch_; }
  const std::vector<u8>& mainrom() const { return mainrom_; }

private:
  std::vector<u8> mainrom_;   // 4 MiB
  std::vector<u8> workram_;   // 0x300000-0x307FFF → 32 KiB
  std::vector<u8> triram_;    // 2 KiB shared
  std::vector<u8> scratch_;   // 2 KiB
  Video* video_ = nullptr;
  AudioSystem* audio_ = nullptr;
  KeyCus* key_ = nullptr;
};

} // namespace sh
