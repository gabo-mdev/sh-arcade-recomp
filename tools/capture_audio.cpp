#include "machine/machine.hpp"
#include "machine/romset.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {
class CaptureAudio final : public sh::IAudioBackend {
public:
  explicit CaptureAudio(sh::Machine& machine, int from) : machine_(machine), from_(from) {}
  bool init(int, int) override { return true; }
  void queue(std::span<const float> samples) override {
    if (machine_.frame_index() < sh::u64(from_))
      return;
    for (float sample : samples) {
      const float s = std::clamp(sample, -1.f, 1.f);
      pcm_.push_back(std::int16_t(std::lround(s * 32767.f)));
      peak_ = std::max(peak_, std::fabs(s));
      energy_ += double(s) * double(s);
    }
  }
  int sample_rate() const override { return 48000; }
  void set_ym_mute(bool) override {}
  void set_ost_gain(float) override {}
  void shutdown() override {}
  const std::vector<std::int16_t>& pcm() const { return pcm_; }
  float peak() const { return peak_; }
  double rms() const { return pcm_.empty() ? 0 : std::sqrt(energy_ / pcm_.size()); }
private:
  sh::Machine& machine_;
  int from_;
  std::vector<std::int16_t> pcm_;
  float peak_ = 0;
  double energy_ = 0;
};

void put16(std::FILE* file, std::uint16_t v) {
  std::fputc(v & 0xff, file);
  std::fputc(v >> 8, file);
}
void put32(std::FILE* file, std::uint32_t v) {
  put16(file, std::uint16_t(v));
  put16(file, std::uint16_t(v >> 16));
}
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: sh_capture_audio output.wav [from=2200] [to=2700] [right=-1] [attack=-1]\n");
    return 2;
  }
  const int from = argc > 2 ? std::atoi(argv[2]) : 2200;
  const int to = argc > 3 ? std::atoi(argv[3]) : 2700;
  const int right_at = argc > 4 ? std::atoi(argv[4]) : -1;
  const int attack_at = argc > 5 ? std::atoi(argv[5]) : -1;
  if (from < 0 || to <= from)
    return 2;
  sh::MachineConfig cfg;
  cfg.headless = true;
  cfg.coin_frame = 2100;
  cfg.start_frame = 2200;
  cfg.right_frame = right_at;
  cfg.right_hold_frames = 500;
  cfg.attack_frame = attack_at;
  sh::Machine machine(cfg);
  machine.load(sh::RomSet::load("rom"));
  machine.reset();
  machine.set_input(sh::make_null_input());
  machine.set_present(sh::make_null_present());
  auto capture = std::make_unique<CaptureAudio>(machine, from);
  auto* result = capture.get();
  machine.set_audio_backend(std::move(capture));
  while (machine.frame_index() < sh::u64(to))
    machine.run_frame();
  const auto& pcm = result->pcm();
  const std::uint32_t bytes = std::uint32_t(pcm.size() * 2);
  std::FILE* file = std::fopen(argv[1], "wb");
  if (!file)
    return 1;
  std::fwrite("RIFF", 1, 4, file); put32(file, bytes + 36);
  std::fwrite("WAVEfmt ", 1, 8, file); put32(file, 16);
  put16(file, 1); put16(file, 2); put32(file, 48000);
  put32(file, 48000 * 4); put16(file, 4); put16(file, 16);
  std::fwrite("data", 1, 4, file); put32(file, bytes);
  std::fwrite(pcm.data(), sizeof(std::int16_t), pcm.size(), file);
  std::fclose(file);
  std::printf("samples=%zu peak=%.4f rms=%.4f\n", pcm.size() / 2,
              result->peak(), result->rms());
}
