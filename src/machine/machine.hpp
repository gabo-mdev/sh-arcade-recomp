#pragma once

#include "audio/audio.hpp"
#include "bus/c117.hpp"
#include "bus/virtmap.hpp"
#include "cpu/hd63701.hpp"
#include "cpu/m6809.hpp"
#include "host/backends.hpp"
#include "machine/keycus.hpp"
#include "machine/romset.hpp"
#include "video/video.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace sh {

struct MachineConfig {
  bool headless = false;
  bool trace_main = false;
  bool trace_audio = false;
  bool probe = false;
  std::string options_path; // empty: do not load or save the options file
  u64 max_main_steps = 0;
  int scale = 3;
  std::string probe_out = "build/probe";
  int coin_frame = -1;  // 0-based input frame; negative disables injection
  int start_frame = -1;
  int input_hold_frames = 4;
  int right_frame = -1;
  int right_hold_frames = 60;
  int attack_frame = -1;
  int attack_hold_frames = 4;
};

class Machine {
public:
  explicit Machine(MachineConfig cfg = {});

  void load(const RomSet& roms);
  void reset(bool preserve_nvram = false);

  /// Run until quit, or for `frames` if > 0.
  int run(int frames = 0);

  void run_slice();
  void run_frame();

  M6809& maincpu() { return main_; }
  M6809& subcpu() { return sub_; }
  M6809& audiocpu() { return audio_cpu_; }
  AudioSystem& audio() { return audio_; }
  HD63701& mcu() { return mcu_; }
  Video& video() { return video_; }
  VirtMap& virtmap() { return map_; }
  C117& c117() { return c117_; }
  InputState& input() { return input_; }
  HostOptions& options() { return options_; }

  void set_present(std::unique_ptr<IPresentBackend> b) { present_ = std::move(b); }
  void set_audio_backend(std::unique_ptr<IAudioBackend> b);
  void set_input(std::unique_ptr<IInputBackend> b) { input_be_ = std::move(b); }

  u64 frame_index() const { return frame_; }

private:
  void wire_buses();
  void run_audio_cpu(int cycles);
  void service_vblank();
  void service_audio_irq();
  void persist_options();
  const std::vector<u8>& audio_voice() const;

  MachineConfig cfg_;
  VirtMap map_;
  C117 c117_{map_};
  KeyCus key_;
  Video video_;
  AudioSystem audio_;
  M6809 main_;
  M6809 sub_;
  M6809 audio_cpu_;
  HD63701 mcu_;
  std::vector<u8> sound_rom_;
  std::vector<u8> mcu_rom_;
  std::vector<u8> voice_ref_;
  std::vector<u8> sound_work_;
  std::array<u8, 0x800> nvram_{};
  u8 sound_bank_ = 0;
  int audio_cycle_debt_ = 0;
  u8 mcu_bank_ = 0;
  u8 mcu_patch_data_ = 0;
  u32 mcu_voice_base_ = 0;
  bool sub_held_ = true;
  bool audio_held_ = true;
  bool mcu_held_ = true;
  bool checksum_logged_ = false;
  bool service_bits_cleared_ = false;
  u64 service_exit_frame_ = 0;
  bool vram_test_cleared_ = false;
  bool vram_test_retried_ = false;
  bool post_ram_skipped_ = false;
  bool c116_header_seeded_ = false;
  bool game_entered_ = false;
  bool e00a_hle_done_ = false;
  bool initial_health_seeded_ = false;
  bool hud_bit_latched_ = false;
  bool vsync_applied_ = true;

  void mcu_bankswitch(u8 data);
  void mcu_patch_write(u8 data);
  HostOptions options_;
  InputState input_;
  bool coin1_prev_ = false;
  u64 frame_ = 0;

  std::unique_ptr<IPresentBackend> present_;
  std::unique_ptr<IAudioBackend> audio_be_;
  std::unique_ptr<IInputBackend> input_be_;
  std::vector<u32> framebuffer_;
  std::vector<float> audiobuf_;
};

} // namespace sh
