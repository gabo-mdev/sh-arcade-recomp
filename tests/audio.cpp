#include "audio/audio.hpp"
#include "machine/machine.hpp"
#include "machine/romset.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
struct MeasureAudio final : sh::IAudioBackend {
  explicit MeasureAudio(sh::Machine& machine) : machine(machine) {}
  bool init(int, int) override { return true; }
  void queue(std::span<const float> samples) override {
    if (machine.frame_index() >= 2100 && machine.frame_index() < 2150) {
      for (float s : samples)
        coin_peak = std::max(coin_peak, std::fabs(s));
    }
    if (machine.frame_index() < 2200)
      return;
    ++frames;
    sample_frames += samples.size() / 2;
    for (float s : samples)
      peak = std::max(peak, std::fabs(s));
  }
  int sample_rate() const override { return 48000; }
  void set_ym_mute(bool) override {}
  void set_ost_gain(float) override {}
  void shutdown() override {}
  sh::Machine& machine;
  int frames = 0;
  size_t sample_frames = 0;
  float peak = 0;
  float coin_peak = 0;
};
}

int main() {
  sh::AudioSystem timed;
  timed.reset();
  float timed_wave[480 * 2]{};
  timed.begin_frame(timed_wave);
  for (int i = 0; i < 16; ++i)
    timed.cus30_write(sh::u32(i), sh::u8((2 * i << 4) | (2 * i + 1)));
  timed.cus30_write(0x102, 0xFF);
  timed.cus30_write(0x103, 0xFF);
  timed.set_sample_time(120);
  timed.cus30_write(0x100, 15);
  timed.set_sample_time(240);
  timed.cus30_write(0x100, 0);
  timed.end_frame();
  float before = 0, during = 0, after = 0;
  for (int i = 0; i < 480; ++i) {
    const float magnitude = std::fabs(timed_wave[i * 2]);
    if (i < 120) before = std::max(before, magnitude);
    else if (i < 240) during = std::max(during, magnitude);
    else after = std::max(after, magnitude);
  }
  if (before != 0 || during < 0.02f || after != 0) {
    std::fprintf(stderr, "subframe CUS30 voice window before=%.3f during=%.3f after=%.3f\n", before, during, after);
    return 1;
  }

  sh::NamcoWsg wsg;
  for (int i = 0; i < 16; ++i)
    wsg.write(sh::u32(i), sh::u8((2 * i << 4) | (2 * i + 1)));
  wsg.write(0x100, 15); // left volume
  wsg.write(0x102, 0xFF);
  wsg.write(0x103, 0xFF);
  float wave[480 * 2]{};
  wsg.render(wave, 48000);
  float wave_peak = 0, right_peak = 0;
  for (int i = 0; i < 480; ++i) {
    wave_peak = std::max(wave_peak, std::fabs(wave[i * 2]));
    right_peak = std::max(right_peak, std::fabs(wave[i * 2 + 1]));
  }
  if (wave_peak < 0.02f || right_peak != 0) {
    std::fprintf(stderr, "CUS30 left-only waveform was not rendered\n");
    return 1;
  }
  wsg.reset();
  wsg.write(0x100, 15);
  wsg.write(0x103, 0xFF);
  wsg.write(0x13C, 0x80); // channel 7 right register enables channel 0 noise
  float noise[4800 * 2]{};
  wsg.render(noise, 48000);
  bool positive = false, negative = false;
  for (int i = 0; i < 4800; ++i) {
    positive |= noise[i * 2] > 0;
    negative |= noise[i * 2] < 0;
  }
  if (!positive || !negative) {
    std::fprintf(stderr, "CUS30 noise mode did not alternate polarity\n");
    return 1;
  }

  sh::MachineConfig cfg;
  cfg.headless = true;
  cfg.coin_frame = 2100;
  cfg.start_frame = 2200;
  sh::Machine machine(cfg);
  machine.load(sh::RomSet::load("rom"));
  machine.reset();
  machine.set_input(sh::make_null_input());
  machine.set_present(sh::make_null_present());
  auto backend = std::make_unique<MeasureAudio>(machine);
  auto* result = backend.get();
  machine.set_audio_backend(std::move(backend));
  while (machine.frame_index() < 2299)
    machine.run_frame();
  const sh::u64 dac_before[2] = {machine.audio().dac_writes(0),
                                 machine.audio().dac_writes(1)};
  const sh::u64 audio_cycles_before = machine.audiocpu().cycles_total;
  machine.run_frame();
  const sh::u64 audio_cycles = machine.audiocpu().cycles_total - audio_cycles_before;
  const sh::u64 dac_frame[2] = {machine.audio().dac_writes(0) - dac_before[0],
                                machine.audio().dac_writes(1) - dac_before[1]};
  if (machine.audiocpu().pc < 0xD020 ||
      machine.virtmap().read(0x2FE000) != 0x7C ||
      result->frames != 100 || result->sample_frames != 100 * 792 ||
      result->peak < 0.02f || result->coin_peak < 0.02f ||
      audio_cycles < 25000 || audio_cycles > 25600 ||
      dac_frame[0] < 90 || dac_frame[0] > 105 ||
      dac_frame[1] < 90 || dac_frame[1] > 105) {
    std::fprintf(stderr,
                 "game audio: pc=%04X wave=%02X frames=%d samples=%zu peak=%.3f coin=%.3f audio_cycles=%llu dac=%llu,%llu\n",
                 machine.audiocpu().pc,
                 machine.virtmap().read(0x2FE000), result->frames,
                 result->sample_frames, result->peak, result->coin_peak,
                 static_cast<unsigned long long>(audio_cycles),
                 static_cast<unsigned long long>(dac_frame[0]),
                 static_cast<unsigned long long>(dac_frame[1]));
    return 1;
  }
  // At the first real enemy hit, the MCU must accept the sample command and
  // stream non-centre samples on DAC 0. This failed when BPL tested V.
  sh::MachineConfig hit_cfg = cfg;
  hit_cfg.right_frame = 3350;
  hit_cfg.right_hold_frames = 500;
  hit_cfg.attack_frame = 3440;
  sh::Machine hit(hit_cfg);
  hit.load(sh::RomSet::load("rom"));
  hit.reset();
  hit.set_input(sh::make_null_input());
  hit.set_present(sh::make_null_present());
  hit.set_audio_backend(sh::make_null_audio());
  while (hit.frame_index() < 3379)
    hit.run_frame();
  const sh::u64 before_ambient = hit.audio().dac_noncenter_writes(1);
  while (hit.frame_index() < 3392)
    hit.run_frame();
  if (hit.audio().dac_noncenter_writes(1) - before_ambient < 500 ||
      (hit.virtmap().triram()[0x2E] & 0x80) == 0) {
    std::fprintf(stderr, "ambient MCU DAC 1 voice did not start\n");
    return 1;
  }
  while (hit.frame_index() < 3440)
    hit.run_frame();
  const sh::u64 before_hit = hit.audio().dac_noncenter_writes(0);
  while (hit.frame_index() < 3455)
    hit.run_frame();
  if (hit.audio().dac_noncenter_writes(0) - before_hit < 50 ||
      (hit.virtmap().triram()[0x2C] & 0x80) == 0) {
    std::fprintf(stderr, "enemy hit did not start MCU DAC 0 voice\n");
    return 1;
  }
  return 0;
}
