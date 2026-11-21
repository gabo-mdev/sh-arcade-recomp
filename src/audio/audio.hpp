#pragma once

#include "host/backends.hpp"
#include "sh/types.hpp"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace sh {

class Ym2151 {
public:
  Ym2151();
  ~Ym2151();
  void reset();
  u8 read(u16 addr) const;
  void write(u16 addr, u8 data);
  void render(std::span<float> interleaved_stereo, int sample_rate);
  void set_mute(bool m) { mute_ = m; }

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool mute_ = false;
};

class NamcoWsg {
public:
  void reset();
  u8 read(u32 off) const;
  void write(u32 off, u8 data);
  void render(std::span<float> interleaved_stereo, int sample_rate);

private:
  std::array<u8, 0x400> ram_{};
  std::array<u32, 8> phase_{};
  std::array<u32, 8> noise_phase_{};
  std::array<u32, 8> noise_seed_{};
  std::array<u8, 8> noise_state_{};
  std::array<u8, 8> noise_hold_{};
  u32 sample_fraction_ = 0;
};

/// Aggregates YM2151 + CUS30 WSG + MCU DAC paths.
class AudioSystem {
public:
  AudioSystem();
  ~AudioSystem();

  void reset();
  void load_voice(std::span<const u8> voice);

  u8 cus30_read(u32 off) const;
  void cus30_write(u32 off, u8 data);

  u8 ym_read(u16 addr);
  void ym_write(u16 addr, u8 data);

  void dac_write(int channel, u8 data);
  void set_dac_gain(u8 data);
  u64 dac_writes(int channel) const { return dac_writes_[channel]; }
  u64 dac_noncenter_writes(int channel) const { return dac_noncenter_writes_[channel]; }

  void begin_frame(std::span<float> interleaved_stereo);
  void set_sample_time(size_t sample_frame);
  void end_frame();

  void set_backend(IAudioBackend* backend) { backend_ = backend; }
  IAudioBackend* backend() const { return backend_; }

  Ym2151& ym() { return *ym_; }

private:
  std::unique_ptr<Ym2151> ym_;
  std::unique_ptr<NamcoWsg> wsg_;
  std::vector<u8> voice_;
  u8 dac_[2]{};
  u64 dac_writes_[2]{};
  u64 dac_noncenter_writes_[2]{};
  u8 dac_gain_ = 0;
  IAudioBackend* backend_ = nullptr;
  std::span<float> frame_samples_;
  size_t sample_time_ = 0;
  size_t rendered_frames_ = 0;
  void render_until(size_t sample_frame);
};

} // namespace sh
