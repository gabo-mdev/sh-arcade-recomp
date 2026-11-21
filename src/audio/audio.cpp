#include "audio/audio.hpp"
#include "ymfm_opm.h"

#include <algorithm>

namespace sh {

namespace {
constexpr unsigned kYmClock = 3'579'545; // MAME 0.256 splatter -listxml
}

struct Ym2151::Impl : ymfm::ymfm_interface {
  Impl() : chip(*this) { chip.reset(); }
  ymfm::ym2151 chip;
  unsigned long long sample_fraction = 0;
  int last_left = 0;
  int last_right = 0;
};

Ym2151::Ym2151() : impl_(std::make_unique<Impl>()) {}
Ym2151::~Ym2151() = default;

void Ym2151::reset() {
  impl_->chip.reset();
  impl_->sample_fraction = 0;
  impl_->last_left = impl_->last_right = 0;
}

u8 Ym2151::read(u16) const {
  return impl_->chip.read_status();
}

void Ym2151::write(u16 addr, u8 data) {
  impl_->chip.write(addr, data);
}

void Ym2151::render(std::span<float> interleaved_stereo, int sample_rate) {
  if (sample_rate <= 0)
    return;
  const auto chip_rate = impl_->chip.sample_rate(kYmClock);
  for (size_t i = 0; i + 1 < interleaved_stereo.size(); i += 2) {
    impl_->sample_fraction += chip_rate;
    while (impl_->sample_fraction >= unsigned(sample_rate)) {
      ymfm::ym2151::output_data output;
      impl_->chip.generate(&output);
      impl_->last_left = output.data[0];
      impl_->last_right = output.data[1];
      impl_->sample_fraction -= unsigned(sample_rate);
    }
    if (!mute_) {
      interleaved_stereo[i] += float(impl_->last_left) / 32768.f * 0.5f;
      interleaved_stereo[i + 1] += float(impl_->last_right) / 32768.f * 0.5f;
    }
  }
}

void NamcoWsg::reset() {
  ram_.fill(0);
  phase_.fill(0);
  noise_phase_.fill(0);
  noise_seed_.fill(1);
  noise_state_.fill(0);
  noise_hold_.fill(0);
  sample_fraction_ = 0;
}
u8 NamcoWsg::read(u32 off) const {
  off &= 0x3FF;
  if (off >= 0x100 && off < 0x140 && (off & 7) == 5)
    return u8((phase_[(off - 0x100) / 8] >> 19) & 31);
  return ram_[off];
}
void NamcoWsg::write(u32 off, u8 data) { ram_[off & 0x3FF] = data; }

void NamcoWsg::render(std::span<float> interleaved_stereo, int sample_rate) {
  if (sample_rate <= 0)
    return;
  // CUS30 runs packed 4-bit waveforms at 192 kHz (12 kHz clock x16), then we
  // average into the host rate. Body Eater V0+V2 (~6.1 kHz, $81F5C/$82782)
  // form a metallic beat on wave 1; duck non-noise voices above 5 kHz.
  constexpr u32 chip_rate = 192'000;
  for (size_t i = 0; i + 1 < interleaved_stereo.size(); i += 2) {
    sample_fraction_ += chip_rate;
    float left = 0.f;
    float right = 0.f;
    int count = 0;
    while (sample_fraction_ >= u32(sample_rate)) {
      sample_fraction_ -= u32(sample_rate);
      ++count;
      for (unsigned ch = 0; ch < 8; ++ch) {
        const unsigned reg = 0x100 + ch * 8;
        const u32 frequency = (u32(ram_[reg + 1] & 15) << 16) |
                              (u32(ram_[reg + 2]) << 8) | ram_[reg + 3];
        int value;
        const unsigned prev_reg = 0x100 + ((ch + 7) & 7) * 8;
        const bool noise = (ram_[prev_reg + 4] & 0x80) != 0;
        if (noise) {
          value = noise_state_[ch] ? 7 : -7;
          if (noise_hold_[ch]) {
            --noise_hold_[ch];
          } else {
            noise_hold_[ch] = 8; // 1 << (CUS30 fractional bits 19 - 16)
            noise_phase_[ch] += (frequency & 0xFF) << 4;
            unsigned steps = noise_phase_[ch] >> 12;
            noise_phase_[ch] &= 0xFFF;
            while (steps--) {
              if ((noise_seed_[ch] + 1) & 2)
                noise_state_[ch] ^= 1;
              if (noise_seed_[ch] & 1)
                noise_seed_[ch] ^= 0x28000;
              noise_seed_[ch] >>= 1;
            }
          }
        } else {
          const unsigned wave = ram_[reg + 1] >> 4;
          const unsigned pos = wave * 32 + ((phase_[ch] >> 19) & 31);
          const u8 packed = ram_[(pos >> 1) & 0xFF];
          value = int((pos & 1) ? (packed & 15) : (packed >> 4)) - 8;
          phase_[ch] += frequency;
        }
        const int lv = noise ? ((ram_[reg] & 15) >> 1) : int(ram_[reg] & 15);
        const int rv =
            noise ? ((ram_[reg + 4] & 15) >> 1) : int(ram_[reg + 4] & 15);
        float duck = 1.f;
        if (!noise && frequency != 0) {
          const float hz = float(frequency) * (192000.f / 16777216.f);
          if (hz > 5500.f)
            duck = 0.06f;
          else if (hz > 5000.f)
            duck = 0.06f + 0.94f * ((5500.f - hz) / 500.f);
        }
        left += float(value * lv) * duck;
        right += float(value * rv) * duck;
      }
    }
    if (count) {
      interleaved_stereo[i] += left / float(count * 1024);
      interleaved_stereo[i + 1] += right / float(count * 1024);
    }
  }
}

AudioSystem::AudioSystem()
    : ym_(std::make_unique<Ym2151>()), wsg_(std::make_unique<NamcoWsg>()) {}
AudioSystem::~AudioSystem() = default;

void AudioSystem::reset() {
  frame_samples_ = {};
  sample_time_ = rendered_frames_ = 0;
  ym_->reset();
  wsg_->reset();
  dac_[0] = dac_[1] = 128;
  dac_writes_[0] = dac_writes_[1] = 0;
  dac_noncenter_writes_[0] = dac_noncenter_writes_[1] = 0;
  dac_gain_ = 0;
  if (backend_)
    backend_->set_ym_mute(false);
}

void AudioSystem::load_voice(std::span<const u8> voice) {
  voice_.assign(voice.begin(), voice.end());
}

u8 AudioSystem::cus30_read(u32 off) const { return wsg_->read(off); }
void AudioSystem::cus30_write(u32 off, u8 data) {
  render_until(sample_time_);
  wsg_->write(off, data);
}

u8 AudioSystem::ym_read(u16 addr) { return ym_->read(addr); }
void AudioSystem::ym_write(u16 addr, u8 data) {
  render_until(sample_time_);
  ym_->write(addr, data);
  // Phase 4: observe key-on / track cues here for OST replacement.
  if (backend_)
    backend_->set_ym_mute(false);
}

void AudioSystem::dac_write(int channel, u8 data) {
  if (channel >= 0 && channel < 2) {
    render_until(sample_time_);
    dac_[channel] = data;
    ++dac_writes_[channel];
    if (data != 128)
      ++dac_noncenter_writes_[channel];
  }
}

void AudioSystem::set_dac_gain(u8 data) {
  render_until(sample_time_);
  dac_gain_ = data;
}

void AudioSystem::begin_frame(std::span<float> interleaved_stereo) {
  frame_samples_ = interleaved_stereo;
  sample_time_ = rendered_frames_ = 0;
  std::fill(frame_samples_.begin(), frame_samples_.end(), 0.f);
}

void AudioSystem::set_sample_time(size_t sample_frame) {
  sample_time_ = std::min(sample_frame, frame_samples_.size() / 2);
}

void AudioSystem::end_frame() {
  render_until(frame_samples_.size() / 2);
  frame_samples_ = {};
  sample_time_ = rendered_frames_ = 0;
}

void AudioSystem::render_until(size_t sample_frame) {
  const size_t end = std::min(sample_frame, frame_samples_.size() / 2);
  if (end <= rendered_frames_)
    return;
  auto interleaved_stereo = frame_samples_.subspan(rendered_frames_ * 2,
                                                   (end - rendered_frames_) * 2);
  const int rate = backend_ ? backend_->sample_rate() : 48000;
  ym_->render(interleaved_stereo, rate);
  wsg_->render(interleaved_stereo, rate);
  // Mix DACs
  const float left_gain = float((((dac_gain_ >> 2) & 1) << 1) |
                                (dac_gain_ & 1)) * 0.25f + 0.25f;
  const float right_gain = float((((dac_gain_ >> 4) & 1) << 1) |
                                 ((dac_gain_ >> 3) & 1)) * 0.25f + 0.25f;
  const float l = (float(dac_[0]) - 128.f) / 128.f * left_gain;
  const float r = (float(dac_[1]) - 128.f) / 128.f * right_gain;
  for (size_t i = 0; i + 1 < interleaved_stereo.size(); i += 2) {
    interleaved_stereo[i] += l;
    interleaved_stereo[i + 1] += r;
  }
  rendered_frames_ = end;
}

} // namespace sh
