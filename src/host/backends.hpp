#pragma once

#include "sh/types.hpp"

#include <span>
#include <string_view>

namespace sh {

inline constexpr int kBindCount = 8;
inline constexpr int kImageFilterCount = 8;
inline constexpr int kPadTriggerLeft = 100;
inline constexpr int kPadTriggerRight = 101;
inline constexpr int kDefaultKey[kBindCount] = {82, 81, 80, 79, 29, 27, 30, 34};
inline constexpr int kDefaultPad[kBindCount] = {11, 12, 13, 14, 2, 0, 6, 4};

/// Present path — swap later for widescreen / filters without touching the core.
struct IPresentBackend {
  virtual ~IPresentBackend() = default;
  virtual bool init(const char* title, int scale) = 0;
  virtual void present(std::span<const u32> rgba8888, int w, int h) = 0;
  virtual bool poll_events(bool& quit) = 0; // returns true if still running
  virtual bool menu_open() const { return false; }
  virtual bool consume_reset() { return false; }
  virtual void set_vsync(bool /*on*/) {}
  virtual void set_fullscreen(bool on) = 0;
  virtual void shutdown() = 0;
};

/// Audio path — OST replacement hooks mute YM2151 and inject streams here later.
struct IAudioBackend {
  virtual ~IAudioBackend() = default;
  virtual bool init(int sample_rate, int channels) = 0;
  virtual void queue(std::span<const float> interleaved) = 0;
  virtual int sample_rate() const = 0;
  virtual void set_ym_mute(bool mute) = 0;       // Phase 4 OST
  virtual void set_ost_gain(float gain) = 0;     // Phase 4 OST
  virtual void set_output_gain(float) {}
  virtual void set_mono(bool) {}
  virtual void set_paused(bool) {}
  virtual bool load_ost_track(std::string_view /*path*/) { return false; }
  virtual void shutdown() = 0;
};

/// Host settings shared by the options menu and the input backend.
/// Switches here are live: the next emulated frame reads them.
struct HostOptions {
  bool stage_select = false; // DIP SW3 On: Start opens the stage menu
  bool skip_checksum = true; // Uncap POST until the title; do not skip the sum
  bool invincible = false;    // Keep the player's hearts full during play
  bool untouchable = false;   // Ignore contact: no recoil and no lost hearts
  bool uncap_fps = false;     // Drop the 60 Hz cap and vsync
  bool debug_menu = false;    // F1 / right-stick click opens the debug panel
  bool mono_audio = false;    // Downmix stereo output to mono (PCB JP4-style)
  bool fullscreen = false;
  bool stretch = false;       // Fill the window instead of integer scaling
  bool vsync = true;
  int scale = 3;              // Window size in multiples of 288x224
  int volume = 100;           // Output gain, percent
  int gamepad = 0;            // Index into the SDL gamepad list
  int image_filter = 0;       // See kImageFilterCount
  int language = 0;           // 0 English (default), 1 Spanish
  // One keyboard scancode and one pad control per action.
  // Order: up, down, left, right, attack, jump, start, coin.
  // Pad values are SDL gamepad buttons, or 100 / 101 for the triggers. -1 is unbound.
  int key_bind[kBindCount] = {82, 81, 80, 79, 29, 27, 30, 34};
  int pad_bind[kBindCount] = {11, 12, 13, 14, 2, 0, 6, 4};
  bool dirty = false;         // Menu edited; not written to the options file
};

struct InputState {
  u8 control0 = 0xFF; // active low
  u8 control1 = 0xFF;
  u8 dipsw = 0xFF;    // bit0 = service, bit2 = stage select; set bit = Off
  // MCU P1 / COIN: bits 0-2 are outs; 3-7 active-low inputs (coin/service).
  // Default all high so coins/service read as released.
  u8 coin = 0xFF;
  bool coin_edge = false;
};

struct IInputBackend {
  virtual ~IInputBackend() = default;
  virtual void poll(InputState& out) = 0;
};

std::unique_ptr<IPresentBackend> make_null_present();
std::unique_ptr<IAudioBackend> make_null_audio();
std::unique_ptr<IInputBackend> make_null_input();

#if defined(SH_HOST_SDL)
std::unique_ptr<IPresentBackend> make_sdl_present(HostOptions* options);
std::unique_ptr<IAudioBackend> make_sdl_audio();
std::unique_ptr<IInputBackend> make_sdl_input(HostOptions* options);
#endif

} // namespace sh
