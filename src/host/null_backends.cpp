#include "host/backends.hpp"

namespace sh {

namespace {

struct NullPresent : IPresentBackend {
  bool init(const char*, int) override { return true; }
  void present(std::span<const u32>, int, int) override {}
  bool poll_events(bool& quit) override {
    quit = false;
    return true;
  }
  void set_fullscreen(bool) override {}
  void shutdown() override {}
};

struct NullAudio : IAudioBackend {
  bool init(int rate, int ch) override {
    rate_ = rate;
    ch_ = ch;
    return true;
  }
  void queue(std::span<const float>) override {}
  int sample_rate() const override { return rate_; }
  void set_ym_mute(bool) override {}
  void set_ost_gain(float) override {}
  void shutdown() override {}
  int rate_ = 48000;
  int ch_ = 2;
};

struct NullInput : IInputBackend {
  void poll(InputState& out) override {
    out = InputState{};
    // Leave service DIP off by default (bit0 high). Probe can clear it.
  }
};

} // namespace

std::unique_ptr<IPresentBackend> make_null_present() { return std::make_unique<NullPresent>(); }
std::unique_ptr<IAudioBackend> make_null_audio() { return std::make_unique<NullAudio>(); }
std::unique_ptr<IInputBackend> make_null_input() { return std::make_unique<NullInput>(); }

} // namespace sh
