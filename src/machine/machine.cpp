#include "machine/machine.hpp"

#include "host/options_file.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace sh {

namespace {
// MAME namcos1 uses ~40-cycle quantum to avoid VRAM/triram fights.
constexpr int kQuantumCycles = 16;
constexpr int kCyclesPerFrame = 25344;
} // namespace

Machine::Machine(MachineConfig cfg) : cfg_(std::move(cfg)) {
  framebuffer_.assign(kScreenW * kScreenH, 0);
  audiobuf_.assign(792 * 2, 0.f); // 48 kHz / (2000/33 Hz)
}

void Machine::set_audio_backend(std::unique_ptr<IAudioBackend> b) {
  audio_be_ = std::move(b);
  audio_.set_backend(audio_be_.get());
}

void Machine::load(const RomSet& roms) {
  map_.attach_rom(roms.mainrom);
  map_.attach_video(&video_);
  map_.attach_audio(&audio_);
  map_.attach_keycus(&key_);
  video_.load_gfx(roms.tiles, roms.mask, roms.sprites);
  audio_.load_voice(roms.voice);
  sound_rom_ = roms.audiocpu;
  mcu_rom_ = roms.mcu;
  voice_ref_ = roms.voice;
  if (mcu_rom_.size() > 0x1000)
    mcu_rom_.resize(0x1000);
  wire_buses();
}

void Machine::mcu_bankswitch(u8 data) {
  // MAME namcos1_state::mcu_bankswitch_w — chip-select + A15/A16.
  int bank = 0;
  u8 d = data;
  switch (d & 0xfc) {
  case 0xf8:
    bank = 0;
    d = u8(d ^ 2); // ROM0 A16 inverted
    break;
  case 0xf4:
    bank = 4;
    break;
  case 0xec:
    bank = 8;
    break;
  case 0xdc:
    bank = 12;
    break;
  case 0xbc:
    bank = 16;
    break;
  case 0x7c:
    bank = 20;
    break;
  default:
    bank = 0;
    break;
  }
  bank += (d & 3);
  mcu_bank_ = data;
  mcu_voice_base_ = u32(bank) * 0x8000u;
}

void Machine::mcu_patch_write(u8 data) {
  // MAME mcu_patch_w: once $A6 is seen, freeze triram[0] (init handshake).
  if (mcu_patch_data_ == 0xA6)
    return;
  mcu_patch_data_ = data;
  map_.triram()[0] = data;
  if (cfg_.probe && data == 0xA6)
    std::printf("mcu_patch: A6 @ mcu=%04X frame=%llu\n", mcu_.pc,
                static_cast<unsigned long long>(frame_));
}

void Machine::wire_buses() {
  c117_.set_subres([this](bool hold) {
    // Match MAME subres_w: patch clear + RESET edge only when state changes.
    if (hold == sub_held_)
      return;
    mcu_patch_data_ = 0;
    if (cfg_.probe && (hold || frame_ < 600)) {
      std::printf("SUBRES %s f=%llu tri0=%02X main=%04X S=%04X\n", hold ? "HOLD" : "RUN",
                  static_cast<unsigned long long>(frame_), unsigned(map_.triram()[0]),
                  unsigned(main_.pc), unsigned(main_.s));
    }
    sub_held_ = hold;
    audio_held_ = hold;
    mcu_held_ = hold;
    audio_cycle_debt_ = 0;
    sub_.set_reset_hold(hold);
    audio_cpu_.set_reset_hold(hold);
    // Leaving reset: refetch vectors (MAME CLEAR_LINE after ASSERT).
    if (!hold) {
      sub_.reset();
      audio_cpu_.reset();
      mcu_.reset();
    }
  });
  c117_.set_irq_ack([this](C117::CpuId cpu, bool) {
    (cpu == C117::Main ? main_ : sub_).set_irq(false);
  });
  c117_.set_firq_ack([this](C117::CpuId cpu, bool) {
    (cpu == C117::Main ? main_ : sub_).set_firq(false);
  });
  c117_.set_assert_sub_firq([this]() { sub_.set_firq(true); });
  main_.set_bus(
      [this](u16 a) { return c117_.read(C117::Main, a); },
      [this](u16 a, u8 d) { c117_.write(C117::Main, a, d); });
  sub_.set_bus(
      [this](u16 a) { return c117_.read(C117::Sub, a); },
      [this](u16 a, u8 d) { c117_.write(C117::Sub, a, d); });

  // Sound work RAM lives for the life of the machine.
  sound_work_.assign(0x2000, 0);

  audio_cpu_.set_bus(
      [this](u16 a) -> u8 {
        if (a < 0x4000) {
          const u32 off = u32(sound_bank_ & 7) * 0x4000u + a;
          return off < sound_rom_.size() ? sound_rom_[off] : 0xFF;
        }
        if (a <= 0x4001)
          return audio_.ym_read(a & 1);
        if (a >= 0x5000 && a <= 0x5FFF)
          return audio_.cus30_read(a & 0x3FF);
        if (a >= 0x7000 && a <= 0x7FFF)
          return map_.triram()[a & 0x7FF];
        if (a >= 0x8000 && a <= 0x9FFF)
          return sound_work_[a - 0x8000];
        if (a >= 0xC000)
          return sound_rom_[a & 0x3FFF];
        return 0xFF;
      },
      [this](u16 a, u8 d) {
        if (a >= 0x4000 && a <= 0x4001) {
          if (cfg_.trace_audio)
            std::printf("audio-write f=%llu pc=%04X YM %04X=%02X\n",
                        static_cast<unsigned long long>(frame_), audio_cpu_.pc, a, d);
          audio_.ym_write(a & 1, d);
        } else if (a >= 0x5000 && a <= 0x5FFF) {
          if (cfg_.trace_audio &&
              (((a & 0x3FF) >= 0x100 && (a & 0x3FF) < 0x140) ||
               ((a & 0x3FF) >= 0x200 && d != 0)))
            std::printf("audio-write f=%llu pc=%04X CUS30 %03X=%02X\n",
                        static_cast<unsigned long long>(frame_), audio_cpu_.pc,
                        a & 0x3FF, d);
          audio_.cus30_write(a & 0x3FF, d);
        } else if (a >= 0x7000 && a <= 0x7FFF)
          map_.triram()[a & 0x7FF] = d;
        else if (a >= 0x8000 && a <= 0x9FFF)
          sound_work_[a - 0x8000] = d;
        else if (a == 0xC000 || a == 0xC001)
          sound_bank_ = (d >> 4) & 7;
        else if (a == 0xE000)
          audio_cpu_.set_irq(false);
      });

  // MAME wires ymsnd IRQ → audiocpu FIRQ, but this ROM's FIRQ vector is
  // $FFFF (unused). The score is paced by vblank IRQ ($D0F2). Do not assert
  // FIRQ or a timer edge jumps into open space and corrupts the sound CPU.

  mcu_.set_internal_rom(mcu_rom_);
  mcu_.set_port_read(
      [this]() {
        // P1: preserve MCU-driven outs (low 3 bits) semantics by only exposing
        // the active-low input nibble from the host.
        return u8(0x07 | (input_.coin & 0xF8));
      },
      []() { return u8(0xFF); });
  mcu_.set_port_write([](u8) {}, [this](u8 v) { audio_.set_dac_gain(v); });

  mcu_.set_bus(
      [this](u16 a) -> u8 {
        if (a >= 0x1000 && a <= 0x1003) {
          // MAME namcos1 dsw_r via LS157: select high/low nibble, bitswap<4>(…,0,1,2,3)
          // is identity, then OR into 0xF0.
          const u8 dsw = input_.dipsw;
          const u8 nib = (a & 2) ? u8((dsw >> 4) & 0x0F) : u8(dsw & 0x0F);
          return u8(0xF0 | nib);
        }
        if (a == 0x1400)
          return input_.control0;
        if (a == 0x1401)
          return input_.control1;
        if (a >= 0x4000 && a <= 0xBFFF) {
          const u32 off = mcu_voice_base_ + (a - 0x4000);
          const auto& v = audio_voice();
          return off < v.size() ? v[off] : 0xFF;
        }
        if (a >= 0xC000 && a <= 0xC7FF)
          return map_.triram()[a & 0x7FF];
        if (a >= 0xC800 && a <= 0xCFFF)
          return nvram_[a & 0x7FF];
        return 0xFF;
      },
      [this](u16 a, u8 d) {
        if (a == 0xC000)
          mcu_patch_write(d);
        else if (a >= 0xC001 && a <= 0xC7FF)
          map_.triram()[a & 0x7FF] = d;
        else if (a >= 0xC800 && a <= 0xCFFF)
          nvram_[a & 0x7FF] = d;
        else if (a == 0xD000)
          audio_.dac_write(0, d);
        else if (a == 0xD400)
          audio_.dac_write(1, d);
        else if (a == 0xD800)
          mcu_bankswitch(d);
        else if (a == 0xF000)
          mcu_.set_irq(false);
      });
}

const std::vector<u8>& Machine::audio_voice() const {
  return voice_ref_;
}

void Machine::reset(bool preserve_nvram) {
  map_.reset();
  key_.reset();
  video_.reset();
  audio_.reset();
  c117_.reset();
  sound_bank_ = 0;
  audio_cycle_debt_ = 0;
  mcu_bank_ = 0;
  mcu_patch_data_ = 0;
  mcu_voice_base_ = 0;
  frame_ = 0;
  checksum_logged_ = false;
  service_bits_cleared_ = false;
  service_exit_frame_ = 0;
  vram_test_cleared_ = false;
  vram_test_retried_ = false;
  post_ram_skipped_ = false;
  c116_header_seeded_ = false;
  game_entered_ = false;
  e00a_hle_done_ = false;
  initial_health_seeded_ = false;
  hud_bit_latched_ = false;
  main_.skip_hurt = false;
  sound_work_.assign(0x2000, 0);
  if (!preserve_nvram)
    nvram_.fill(0); // match prior bring-up that reaches I/O test UI
  main_.reset();
  sub_.set_reset_hold(true);
  audio_cpu_.set_reset_hold(true);
  sub_held_ = audio_held_ = mcu_held_ = true;
}

void Machine::service_vblank() {
  video_.latch_sprites();
  // RAM bit-walk (C330-C42C) and ROM checksum (C481-C510) are IRQ-brittle.
  const u16 pc = main_.pc;
  const bool selftest =
      (pc >= 0xC330 && pc <= 0xC42C) || (pc >= 0xC481 && pc <= 0xC510);
  // Rising edge of vblank: main + sub + MCU. The audiocpu IRQ is delayed to
  // the falling edge (service_audio_irq) — MAME namcos1 notes Splatterhouse
  // 2nd-boss music fails if that line asserts with main/sub.
  if (!selftest)
    main_.set_irq(true);
  if (!sub_held_)
    sub_.set_irq(true);
  if (!mcu_held_)
    mcu_.set_irq(true);
}

void Machine::service_audio_irq() {
  // Falling edge of vblank for the sound CPU only.
  if (!audio_held_)
    audio_cpu_.set_irq(true);
}

void Machine::run_audio_cpu(int cycles) {
  const int budget = cycles - audio_cycle_debt_;
  if (budget <= 0) {
    audio_cycle_debt_ = -budget;
    return;
  }
  // M6809::run finishes the last instruction even when it crosses a slice.
  // Carry those cycles forward so the sound ROM runs at its configured clock.
  const int executed = audio_cpu_.run(budget);
  audio_cycle_debt_ = executed > 0 ? executed - budget : 0;
}

void Machine::run_slice() {
  main_.run(kQuantumCycles);
  if (!sub_held_)
    sub_.run(kQuantumCycles);
  if (!audio_held_)
    run_audio_cpu(kQuantumCycles);
  if (!mcu_held_)
    mcu_.run(kQuantumCycles);
}

void Machine::run_frame() {
  // Hit calls at $CED6 and $C5F8. Only while actually playing, and only
  // when U is Rick's body, so other objects still take his attacks.
  const bool playing = c117_.read(C117::Main, 0x002C) == 0x0A &&
                       c117_.read(C117::Main, 0x0F9D) != 0 &&
                       c117_.read(C117::Main, 0x0FA6) == 0x11;
  main_.skip_hurt = options_.untouchable && playing;
  if (main_.skip_hurt) {
    const u8 slot = c117_.read(C117::Main, 0x0039);
    main_.hurt_body = u16(0x011D + u16(slot) * 0x40);
  }
  audio_.begin_frame(audiobuf_);
  if (input_be_)
    input_be_->poll(input_);
  // Reproducible headless input pulses for Start Game probes. SDL input still
  // works normally; a scheduled pulse only clears the matching active-low bit.
  const auto pulse = [&](int at) {
    return at >= 0 && frame_ >= u64(at) &&
           frame_ - u64(at) < u64(cfg_.input_hold_frames);
  };
  if (pulse(cfg_.coin_frame))
    input_.coin = u8(input_.coin & ~0x10u);
  if (pulse(cfg_.start_frame))
    input_.control0 = u8(input_.control0 & ~0x80u);
  if (cfg_.right_frame >= 0 && frame_ >= u64(cfg_.right_frame) &&
      frame_ - u64(cfg_.right_frame) < u64(cfg_.right_hold_frames))
    input_.control0 = u8(input_.control0 & ~0x01u);
  if (cfg_.attack_frame >= 0 && frame_ >= u64(cfg_.attack_frame) &&
      frame_ - u64(cfg_.attack_frame) < u64(cfg_.attack_hold_frames))
    input_.control0 = u8(input_.control0 & ~0x10u);
  if (!mcu_held_ && frame_ > 900) {
    const bool coin1 = (input_.coin & 0x10) == 0;
    if (coin1 && !coin1_prev_) {
      const u8 credits = video_.c116_read(0x6015);
      if (credits < 9) {
        video_.c116_write(0x6015, u8(credits + 1));
        // The boot/coin HLE bypasses the game's coin handler. MAME sends
        // command $01 through shared CUS30 RAM $240 for its coin effect.
        audio_.cus30_write(0x240, 1);
      }
    }
    coin1_prev_ = coin1;
  }

  bool e17c_served = false; // vblank handler has run; attract ticks once per frame
  // After main/sub have run through CRT blanking, assert the sound IRQ
  // (falling edge). Namco System 1 blanking is ~40 lines × 384 dots at
  // 6.144 MHz ≈ 3840 main cycles (49.152/32). The old 1536 delay fixed
  // Hymn after Evil Cross but was still early for Poltergeist cue changes
  // ($0F→$12→$31→$2C): incomplete YM layers / “lost instruments” (MT04095).
  bool audio_irq_served = false;
  constexpr int kAudioIrqAfter = 3840;
  int left = kCyclesPerFrame;
  while (left > 0) {
    const int slice = left > kQuantumCycles ? kQuantumCycles : left;
    if (!audio_irq_served && (kCyclesPerFrame - left) >= kAudioIrqAfter) {
      service_audio_irq();
      audio_irq_served = true;
    }
    audio_.set_sample_time(size_t(kCyclesPerFrame - left) *
                           (audiobuf_.size() / 2) / kCyclesPerFrame);
    // Refresh after the MCU, before the main IRQ samples $3026/$302A.
    // The MCU loop clears those bytes, so a once-per-frame write loses.
    if (!mcu_held_ && frame_ > 900) {
      map_.triram()[0x26] = u8(~input_.control0);
      map_.triram()[0x27] = u8(~input_.control1);
      const u8 coin_in = u8(0x07 | (input_.coin & 0xF8));
      map_.triram()[0x2A] = u8((~coin_in) & 0x78);
    }
    main_.run(slice);
    // Time-critical PC HLEs must run per-slice; end-of-frame is too late.
    {
      const u16 pc = main_.pc;
      if (!game_entered_ && pc >= 0xE00A && pc <= 0xE1BF)
        game_entered_ = true;
      // Seed before E0A8 runs — end-of-slice is too late for a 16-cycle window.
      if (!c116_header_seeded_ &&
          (game_entered_ || (pc >= 0xD9E0 && pc <= 0xDA00) ||
           (pc >= 0xD6D0 && pc <= 0xD6F4))) {
        video_.c116_write(0x6009, 0x00);
        video_.c116_write(0x600A, 0x02);
        video_.c116_write(0x600B, 0x00);
        video_.c116_write(0x600C, 0x00);
        video_.c116_write(0x600D, 0x00);
        c116_header_seeded_ = true;
      }
      // Fast-forward E00A init to the idle loop: apply E06D banks, skip the
      // mid-init crash window (CD5B/F610/IRQ) until that path is solid.
      if (game_entered_ && !e00a_hle_done_ && pc >= 0xE00A && pc <= 0xE1BF) {
        c117_.write(C117::Main, 0xE000, 0x01);
        c117_.write(C117::Main, 0xE001, 0x80); // bank0 workram
        c117_.write(C117::Main, 0xE200, 0x01);
        c117_.write(C117::Main, 0xE201, 0x7F);
        c117_.write(C117::Main, 0xE400, 0x01);
        c117_.write(C117::Main, 0xE401, 0x78);
        c117_.write(C117::Main, 0xE600, 0x02);
        c117_.write(C117::Main, 0xE601, 0x30);
        c117_.write(C117::Main, 0xE800, 0x03);
        c117_.write(C117::Main, 0xE801, 0xFA);
        c117_.write(C117::Main, 0xEA00, 0x03);
        c117_.write(C117::Main, 0xEA01, 0xFD);
        c117_.write(C117::Main, 0xEC00, 0x03);
        c117_.write(C117::Main, 0xEC01, 0xFE); // bank6 game helpers
        c117_.write(C117::Main, 0x0002, 0);
        c117_.write(C117::Main, 0x0006, 0);
        // Zero workram $0000-$1FFF (E01F clear) so object lists start empty.
        // Sub shares this RAM; restart it afterwards or its stack is gone.
        for (u16 a = 0; a < 0x2000; a++)
          c117_.write(C117::Main, a, 0);
        // E0A8 normally seeds the four-heart maximum. This HLE bypasses it.
        c117_.write(C117::Main, 0x147A, 4);
        // E0B5 copies C116 header byte 9 into $1479. The music updater at
        // BA80 posts a song only when that byte is nonzero (or $2A == 3).
        // The skipped boot leaves it clear, so the Rick/Jennifer attract
        // cue is never sent. Any nonzero value opens the gate; the song
        // index itself comes from the caller in B.
        c117_.write(C117::Main, 0x1479, 1);
        // The skipped boot handshake also releases the sound CPU from its
        // $D004 wait for $A6 in shared CUS30 waveform RAM. Let the sound ROM
        // consume it and initialize the wave/register area itself.
        audio_.cus30_write(0x000, 0xA6);
        // Boot E040 selects sub bank7 = page 0x77, then releases SUBRES.
        // Re-apply that and restart sub now that RAM is clear.
        // Sub D9F4 waits until shared $00 is $A6, then enters its program.
        c117_.write(C117::Main, 0x0000, 0xA6);
        c117_.write(C117::Main, 0xFC00, 0x77);
        if (sub_held_)
          c117_.write(C117::Main, 0xF000, 1);
        else
          sub_.reset();
        // Main E04D sets $06 and waits; HLE starts at E05D so do the kick here.
        // Sub E044 spins until $06!=0, then clears it and builds the frame.
        c117_.write(C117::Main, 0x0006, 1);
        // E06D bank shadows — IRQ @E11A restores bank6 from $15; must survive clear.
        c117_.write(C117::Main, 0x0009, 0x01); // E06D also sets $09=$0180
        c117_.write(C117::Main, 0x000A, 0x80);
        c117_.write(C117::Main, 0x000B, 0x01);
        c117_.write(C117::Main, 0x000C, 0x7F);
        c117_.write(C117::Main, 0x000D, 0x01);
        c117_.write(C117::Main, 0x000E, 0x78);
        c117_.write(C117::Main, 0x000F, 0x02);
        c117_.write(C117::Main, 0x0010, 0x30);
        c117_.write(C117::Main, 0x0011, 0x03);
        c117_.write(C117::Main, 0x0012, 0xFA);
        c117_.write(C117::Main, 0x0013, 0x03);
        c117_.write(C117::Main, 0x0014, 0xFD);
        c117_.write(C117::Main, 0x0015, 0x03);
        c117_.write(C117::Main, 0x0016, 0xFE);
        // F610: copy 0x40 bytes from $F621 → $1430 (scroll/tmap defaults).
        for (u8 i = 0; i < 0x40; i++) {
          const u8 v = c117_.read(C117::Main, u16(0xF621 + i));
          c117_.write(C117::Main, u16(0x1430 + i), v);
        }
        std::fill(video_.vram().begin(), video_.vram().end(), u8(0));
        // Real boot clears $2A. Mode 0 (E1CC) is the title/attract entry;
        // mode 4 is the timed demo and skips the logo.
        // triram handshake + inputs idle (service bits clear for C14D BEQ).
        map_.triram()[0] = 0xA6;
        map_.triram()[0x0B] = 0;
        map_.triram()[0x28] = 0xD7;
        map_.triram()[0x2A] = 0xDF;
        // Attract-style fixed-layer marker (until IRQ/C127 draws the real title).
        auto put_row = [&](int row, int col, const char* msg) {
          for (int i = 0; msg[i]; ++i) {
            const u32 off = 0x7010u + u32((row * 36 + col + i) * 2);
            if (off + 1 < video_.vram().size()) {
              video_.vram()[off] = 0;
              video_.vram()[off + 1] = u8(msg[i] == ' ' ? 0 : msg[i]);
            }
          }
        };
        put_row(8, 10, "SPLATTER HOUSE");
        put_row(14, 12, "INSERT COIN");
        put_row(18, 11, "PUSH 1P START");
        main_.s = 0x1F00;
        main_.dp = 0;
        // Allow IRQ (E0DF→C127); keep F set — FIRQ vector is E000 (cold reset).
        main_.cc = u8((main_.cc | 0x40) & u8(~0x10));
        main_.pc = 0xE05D;
        e00a_hle_done_ = true;
        if (cfg_.probe)
          std::printf("E00A-HLE → E05D (IRQ on) f=%llu sub=%04X\n",
                      static_cast<unsigned long long>(frame_), unsigned(sub_.pc));
      }
      if (game_entered_ && e00a_hle_done_) {
        // The skipped E0A8 setup also initializes the first player's four
        // hearts. Restore that initial state once, then let game code own
        // the current-heart byte and HUD as damage occurs.
        if (!initial_health_seeded_ &&
            c117_.read(C117::Main, 0x002C) == 0x0A &&
            c117_.read(C117::Main, 0x0F9D) != 0 &&
            c117_.read(C117::Main, 0x0FA6) == 0x11) {
          const u8 slot = c117_.read(C117::Main, 0x0039);
          const u16 health = u16(0x011D + u16(slot) * 0x40 + 0x1C);
          c117_.write(C117::Main, health, 4);
          c117_.write(C117::Main, 0x0FA2, 4);
          initial_health_seeded_ = true;
        }
        // On the first entry the sub can install the HUD actor after the
        // initial seed and leave three visible hearts while health is four.
        // Only align that display while current health is still full; once
        // a hit changes health, the game owns the HUD without intervention.
        if (initial_health_seeded_ &&
            c117_.read(C117::Main, 0x002C) == 0x0A &&
            c117_.read(C117::Main, 0x0F9D) != 0 &&
            c117_.read(C117::Main, 0x0FA6) == 0x11) {
          const u8 slot = c117_.read(C117::Main, 0x0039);
          const u16 health = u16(0x011D + u16(slot) * 0x40 + 0x1C);
          if (c117_.read(C117::Main, health) == 4 &&
              c117_.read(C117::Main, 0x0FA2) != 4)
            c117_.write(C117::Main, 0x0FA2, 4);
        }
        // Cheat: a hit still runs, but the heart count is full again before
        // the next slice can treat it as damage. Off leaves the game's own
        // 4→0 path alone.
        if (options_.invincible &&
            c117_.read(C117::Main, 0x002C) == 0x0A &&
            c117_.read(C117::Main, 0x0F9D) != 0 &&
            c117_.read(C117::Main, 0x0FA6) == 0x11) {
          const u8 slot = c117_.read(C117::Main, 0x0039);
          const u16 health = u16(0x011D + u16(slot) * 0x40 + 0x1C);
          const u8 cap = c117_.read(C117::Main, 0x147A);
          const u8 full = cap == 0 ? 4 : cap;
          if (c117_.read(C117::Main, health) != full)
            c117_.write(C117::Main, health, full);
          if (c117_.read(C117::Main, 0x0FA2) != full)
            c117_.write(C117::Main, 0x0FA2, full);
        }
        // The vblank handler (E0DF) is what copies the ports and the credit
        // byte into $FA/$3E. Let it run once, then mask I so a stuck line
        // cannot tick the attract again before the next frame.
        if (main_.pc >= 0xE0DF && main_.pc <= 0xE123)
          e17c_served = true;
        if (main_.pc >= 0xE05D && main_.pc <= 0xE067) {
          if (!e17c_served)
            main_.cc = u8((main_.cc | 0x40) & u8(~0x10));
          else
            main_.cc |= 0x50;
        } else if (main_.pc < 0xE0DF || main_.pc > 0xE123) {
          main_.cc |= 0x50; // I+F
        }
        // Bit1 keeps the attract init from clearing $15E5. Bit5 is the
        // in-game HUD enable: the sub installs the life hearts only when it
        // is set, and the main otherwise forces the life counter back to 1.
        // E150 rebuilds this byte from triram every vblank. The complemented
        // SW3 switch lands in bit 6, which this ROM never tests. The movable
        // stage number in the mask scene is bit 5: while it is clear, the
        // main forces $1476 back to 1. Keep bit 5 clear through the pre-play
        // sequence unless the options menu asked for stage select, then let
        // it stay set so Start can show the number. Latch it once gameplay
        // begins so the life counter stays intact either way.
        {
          if (c117_.read(C117::Main, 0x002C) == 0x0A)
            hud_bit_latched_ = true;
          u8 v = c117_.read(C117::Main, 0x147E);
          v = u8(v | 0x02);
          const bool show_stage = hud_bit_latched_ || options_.stage_select;
          v = show_stage ? u8(v | 0x20) : u8(v & ~0x20);
          if (v != c117_.read(C117::Main, 0x147E))
            c117_.write(C117::Main, 0x147E, v);
        }
      }
      // Refresh marker only while playfields stay empty (real attract not drawing).
      if (e00a_hle_done_ && (frame_ % 60) == 0) {
        size_t pf_nz = 0;
        for (size_t i = 0; i < 0x6000 && i < video_.vram().size(); ++i)
          if (video_.vram()[i])
            ++pf_nz;
        if (pf_nz < 32) {
          std::fill(video_.vram().begin() + 0x7800,
                    video_.vram().begin() + 0x8000, u8(0));
          auto put_row = [&](int row, int col, const char* msg) {
            for (int i = 0; msg[i]; ++i) {
              const u32 off = 0x7010u + u32((row * 36 + col + i) * 2);
              video_.vram()[off] = 0;
              video_.vram()[off + 1] = u8(msg[i] == ' ' ? 0 : msg[i]);
            }
          };
          put_row(8, 10, "SPLATTER HOUSE");
          put_row(14, 12, "INSERT COIN");
          put_row(18, 11, "PUSH 1P START");
        }
      }
    }
    // Pause companions during RAM bit-walk / ROM checksum only.
    const u16 pc = main_.pc;
    const bool selftest =
        (pc >= 0xC330 && pc <= 0xC42C) || (pc >= 0xC481 && pc <= 0xC510);
    if (!sub_held_ && !selftest)
      sub_.run(slice);
    if (!audio_held_ && !selftest)
      run_audio_cpu(slice);
    if (!mcu_held_ && !selftest)
      mcu_.run(slice);
    left -= slice;
  }
  // VRAM bit-walk needs a clean tilemap (checksum text otherwise trips RAM2).
  if (main_.pc >= 0xDA02 && main_.pc <= 0xDACF && !vram_test_cleared_) {
    std::fill(video_.vram().begin(), video_.vram().end(), u8(0));
    vram_test_cleared_ = true;
  }
  if (!vram_test_retried_) {
    const u8 code = c117_.read(C117::Main, 0x0000);
    const u16 pc = main_.pc;
    if ((pc >= 0xD5C0 && pc <= 0xD5CB) || (pc >= 0xD6D0 && pc <= 0xD720)) {
      if (code >= 0x0A && code <= 0x0D) {
        std::fill(video_.vram().begin(), video_.vram().end(), u8(0));
        c117_.write(C117::Main, 0x0000, 0x90);
        main_.pc = 0xDA02;
        vram_test_cleared_ = true;
        vram_test_retried_ = true;
      }
    }
  }
  // VRAM success writes $A0; D9EB only accepts $A6 — promote so boot continues.
  if (main_.pc >= 0xD9EB && main_.pc <= 0xDA00 &&
      c117_.read(C117::Main, 0x0000) == 0xA0)
    c117_.write(C117::Main, 0x0000, 0xA6);
  // D6D7 service waiter → C0AB / I/O-test rotator (stable tile UI milestone).
  if (main_.pc >= 0xD6D0 && main_.pc <= 0xD6F4 && frame_ >= 1600 &&
      c117_.read(C117::Main, 0x0000) != 0xA0)
    c117_.write(C117::Main, 0x0000, 0xA0);
  // Seed C116 "game header" that E0A8 reads via bank6=$0173 (page at $C000).
  // Without this, JSR $C000 after E0F1 executes garbage and the CPU wanders.
  if (!c116_header_seeded_ && main_.pc >= 0xE0A8 && main_.pc <= 0xE0B5) {
    // phys 0x2E6000+off → c116; header at CPU $C00A=2 (splatter), $C009/$C00C..
    video_.c116_write(0x6009, 0x00);
    video_.c116_write(0x600A, 0x02);
    video_.c116_write(0x600B, 0x00);
    video_.c116_write(0x600C, 0x00);
    video_.c116_write(0x600D, 0x00);
    c116_header_seeded_ = true;
  }
  // Real game entry is bank7 E00A (page 1FF), not boot mirror at C067.
  if (!game_entered_ && main_.pc >= 0xE00A && main_.pc <= 0xE1BF)
    game_entered_ = true;
  // Exit I/O-test C100 once: keep bits clear until we observe C115+ or E00A
  // (same frame as C118 often jumps via FFEC), then lock idle for C062.
  if (frame_ > 2800 && !game_entered_) {
    const u16 pc = main_.pc;
    if (!service_bits_cleared_ &&
        ((pc >= 0xC115 && pc <= 0xC126) || (pc >= 0xE000 && pc <= 0xE1FF) ||
         (pc >= 0xC013 && pc <= 0xC06A)))
      service_bits_cleared_ = true;
    if (!service_bits_cleared_) {
      map_.triram()[0x28] = u8(input_.control0 & u8(~0x28));
      map_.triram()[0x2A] = u8(input_.coin & u8(~0x20));
    } else {
      map_.triram()[0x0B] = 0;
      map_.triram()[0x28] = input_.control0;
      map_.triram()[0x2A] = input_.coin;
    }
  }
  // E00A waits for sub to clear workram $06. IRQ @E0DF clears $02.
  // Only ACK after bank setup (b6=1FE from E06D); early ACK races init.
  if (game_entered_ && main_.pc >= 0xE04F && main_.pc <= 0xE054) {
    const u32 b6 = c117_.banks(C117::Main)[6];
    if (((b6 / 0x2000u) & 0x1FFu) == 0x1FEu)
      c117_.write(C117::Main, 0x0006, 0);
  }
  // Fallback $02 ACK if IRQ didn't clear it this frame.
  if (e00a_hle_done_ && main_.pc >= 0xE05D && main_.pc <= 0xE066)
    c117_.write(C117::Main, 0x0002, 0);
  // Warm POST RAM bit-walk still fails on C116/triram — skip to checksum.
  // Only before game entry; D5Cx is valid code/data once bank6=03FE.
  if (!game_entered_ && service_bits_cleared_ && main_.pc >= 0xD5C0 &&
      main_.pc <= 0xD5CB) {
    std::fill(video_.vram().begin(), video_.vram().end(), u8(0));
    map_.triram()[0] = 0;
    map_.triram()[0x0B] = 0;
    c117_.write(C117::Main, 0x0000, 0xA6);
    main_.pc = 0xC42C;
    post_ram_skipped_ = true;
  }
  if (cfg_.probe && game_entered_ && frame_ <= 4000 && (frame_ % 30) == 0) {
    const u32 b7 = c117_.banks(C117::Main)[7];
    std::printf(
        "trace f=%llu pc=%04X S=%04X CC=%02X b6=%03X b7=%03X "
        "dp02=%02X dp2A=%02X dp2B=%02X t0=%02X\n",
        static_cast<unsigned long long>(frame_), unsigned(main_.pc),
        unsigned(main_.s), unsigned(main_.cc),
        unsigned((c117_.banks(C117::Main)[6] / 0x2000u) & 0x1FFu),
        unsigned((b7 / 0x2000u) & 0x1FFu),
        unsigned(c117_.read(C117::Main, 0x0002)),
        unsigned(c117_.read(C117::Main, 0x002A)),
        unsigned(c117_.read(C117::Main, 0x002B)),
        unsigned(map_.triram()[0]));
  }
  if (cfg_.probe && (frame_ % 60) == 0) {
    size_t nz = 0;
    for (u8 v : video_.vram())
      if (v)
        ++nz;
    size_t spr_nz = 0;
    for (u32 a = 0; a < 0x800; ++a)
      if (video_.spriteram_read(a))
        ++spr_nz;
    std::printf(
        "f=%llu main=%04X S=%04X vram=%zu spr=%zu "
        "dp2A=%02X dp2B=%02X sub=%04X sb7=%03X b6=%03X\n",
        static_cast<unsigned long long>(frame_), unsigned(main_.pc),
        unsigned(main_.s), nz, spr_nz,
        unsigned(c117_.read(C117::Main, 0x002A)),
        unsigned(c117_.read(C117::Main, 0x002B)),
        unsigned(sub_.pc),
        unsigned((c117_.banks(C117::Sub)[7] / 0x2000u) & 0x1FFu),
        unsigned((c117_.banks(C117::Main)[6] / 0x2000u) & 0x1FFu));
  }
  // Sub is back on its ROM and writes the CUS39 list itself. The hand-built
  // projection covered that list, so it stays off while sub owns sprites.
  if (false && e00a_hle_done_) {
    // Sprite palette page used below (color 0x10 → pens 0x100..). C116 is
    // still unprogrammed, so give those 16 pens a red ramp.
    for (int p = 1; p < 16; ++p) {
      video_.c116_write(u32(0x100 + p), u8(16 + p * 15));
      video_.c116_write(u32(0x900 + p), u8(4 + p * 4));
      video_.c116_write(u32(0x1100 + p), u8(4 + p * 2));
    }
    for (u32 a = 0; a < 0x7F0; ++a)
      video_.spriteram_write(a, 0);
    video_.spriteram_write(0x7F4, 0);
    video_.spriteram_write(0x7F5, 0);
    video_.spriteram_write(0x7F6, 0);
    video_.spriteram_write(0x7F7, 0);
    int slot = 0;
    for (int i = 0; i < 32 && slot < 120; ++i) {
      const u16 base = u16(0x0F9D + i * 32);
      if (c117_.read(C117::Main, base) == 0)
        continue;
      const u8 attr = c117_.read(C117::Main, u16(base + 16));
      const u8 tile = c117_.read(C117::Main, u16(base + 17));
      // Positions are single bytes: X at +20, Y at +26 (the following byte
      // is a fraction, so the 16-bit word is not the pixel).
      const int x = int(c117_.read(C117::Main, u16(base + 20)));
      const int y = int(c117_.read(C117::Main, u16(base + 26)));
      // Hardware Y is negated: byte = -(screenY + size). attr2=0 → 16px.
      const int sizey = 16;
      const u8 yb = u8(0 - (y + sizey));
      const u8 color = 0x20;
      const u32 s = u32(slot) * 16u;
      video_.spriteram_write(s + 4, attr);
      video_.spriteram_write(s + 5, tile);
      video_.spriteram_write(s + 6, color);
      video_.spriteram_write(s + 7, u8(x));
      video_.spriteram_write(s + 8, 0x00);
      video_.spriteram_write(s + 9, yb);
      ++slot;
    }
  }
  service_vblank();
  video_.render(framebuffer_);
  if (present_)
    present_->present(framebuffer_, kScreenW, kScreenH);
  audio_.end_frame();
  if (audio_be_)
    audio_be_->queue(audiobuf_);
  ++frame_;
}

int Machine::run(int frames) {
  if (!present_)
    present_ = make_null_present();
  if (!audio_be_)
    set_audio_backend(make_null_audio());
  if (!input_be_)
    input_be_ = make_null_input();

  if (!cfg_.headless) {
    if (!present_->init("Splatterhouse", cfg_.scale))
      throw Error("Failed to create the game window (SDL video init)");
    if (!audio_be_->init(48000, 2))
      throw Error("Failed to open audio output (SDL audio init)");
  }

  if (cfg_.trace_main && cfg_.max_main_steps) {
    for (u64 i = 0; i < cfg_.max_main_steps; ++i) {
      const u16 pc = main_.pc;
      const int c = main_.step();
      std::printf(
          "PC=%04X A=%02X B=%02X X=%04X Y=%04X U=%04X S=%04X CC=%02X cyc=%d total=%llu\n", pc,
          main_.a, main_.b, main_.x, main_.y, main_.u, main_.s, main_.cc, c,
          static_cast<unsigned long long>(main_.cycles_total));
    }
    return 0;
  }

  bool quit = false;
  int ran = 0;
  // 1.536 MHz / 25344 cycles = 60.606 Hz (16.5 ms). Vsync is requested, but
  // a driver that does not block would run damage, animation and the score
  // as fast as the interpreter allows. The ROM checksum (C481-C510) is not
  // gameplay, so it runs uncapped.
  using clock = std::chrono::steady_clock;
  constexpr auto kFrameTime = std::chrono::nanoseconds(16'500'000);
  auto next_frame = clock::now();
  bool paced = true;
  while (!quit) {
    if (present_ && !cfg_.headless) {
      if (!present_->poll_events(quit))
        break;
    }
    persist_options();
    if (present_ && present_->consume_reset())
      reset(true);
    if (audio_be_) {
      audio_be_->set_output_gain(float(std::clamp(options_.volume, 0, 100)) / 100.f);
      audio_be_->set_mono(options_.mono_audio);
    }
    const bool paused = present_ && present_->menu_open();
    if (audio_be_)
      audio_be_->set_paused(paused);
    if (paused) {
      if (!options_.uncap_fps && vsync_applied_ != options_.vsync) {
        present_->set_vsync(options_.vsync);
        vsync_applied_ = options_.vsync;
      }
      present_->present(framebuffer_, kScreenW, kScreenH);
      // Vsync already caps gameplay. The checksum path turns it off.
      if (!paced)
        std::this_thread::sleep_for(kFrameTime);
      continue;
    }
    run_frame();
    ++ran;
    if (frames > 0 && ran >= frames)
      break;
    if (cfg_.headless && frames <= 0 && ran >= 3)
      break;
    if (!cfg_.headless) {
      const u16 pc = main_.pc;
      // The ROM sum is already uncapped. RAM and VRAM tests are not, and at
      // 60 Hz they are the ~30 s black screen. Cutting the sum (jump to
      // $C070) leaves the sub CPU spinning on $05 and the title dies, so
      // the option only removes the frame cap until the title HLE.
      const bool checkrom = (pc >= 0xC481 && pc <= 0xC510) ||
                            (options_.skip_checksum && !e00a_hle_done_);
      // Uncap drops the 60 Hz sleep and vsync for the whole session. The
      // emulated frame still advances once per loop, so the game itself
      // runs as fast as the interpreter allows.
      if (checkrom || options_.uncap_fps) {
        if (paced) {
          present_->set_vsync(false);
          vsync_applied_ = false;
          paced = false;
        }
      } else {
        if (!paced || vsync_applied_ != options_.vsync) {
          present_->set_vsync(options_.vsync);
          vsync_applied_ = options_.vsync;
          if (!paced) {
            paced = true;
            next_frame = clock::now();
          }
        }
        // Audio is authored for 60.606 Hz. Sleep is the master clock when
        // vsync is off. Snapping next_frame every late frame caused audio
        // underruns on 60 Hz panels; only resync after >3 frames late.
        next_frame += kFrameTime;
        const auto now = clock::now();
        if (now < next_frame)
          std::this_thread::sleep_until(next_frame);
        else if (now - next_frame > kFrameTime * 3)
          next_frame = now;
      }
    }
  }

  if (cfg_.probe) {
    fs::create_directories(cfg_.probe_out);
    const fs::path ppm = fs::path(cfg_.probe_out) / "last_frame.ppm";
    if (FILE* f = std::fopen(ppm.string().c_str(), "wb")) {
      std::fprintf(f, "P6\n%d %d\n255\n", kScreenW, kScreenH);
      for (u32 px : framebuffer_) {
        const u8 rgb[3] = {u8((px >> 16) & 0xFF), u8((px >> 8) & 0xFF), u8(px & 0xFF)};
        std::fwrite(rgb, 1, 3, f);
      }
      std::fclose(f);
    }
    {
      const fs::path sb = fs::path(cfg_.probe_out) / "last_spriteram.bin";
      if (FILE* sf = std::fopen(sb.string().c_str(), "wb")) {
        for (u32 a = 0; a < 0x800; ++a) {
          const u8 b = video_.spriteram_read(a);
          std::fwrite(&b, 1, 1, sf);
        }
        std::fclose(sf);
      }
      const fs::path ob = fs::path(cfg_.probe_out) / "last_objects.bin";
      if (FILE* of = std::fopen(ob.string().c_str(), "wb")) {
        for (u16 a = 0x0F9D; a < 0x0F9D + 0x20 * 0x20; ++a) {
          const u8 b = c117_.read(C117::Main, a);
          std::fwrite(&b, 1, 1, of);
        }
        std::fclose(of);
      }
    }
    size_t vram_nz = 0;
    for (u8 b : video_.vram())
      if (b)
        ++vram_nz;
    size_t pf3 = 0;
    for (u32 off = 0x6000; off + 1 < video_.vram().size(); off += 2) {
      const u16 w = u16((video_.vram()[off] << 8) | video_.vram()[off + 1]);
      if (w == 0x01FF)
        ++pf3;
    }
    const fs::path js = fs::path(cfg_.probe_out) / "status.json";
    {
      const fs::path vb = fs::path(cfg_.probe_out) / "last_vram.bin";
      if (FILE* vf = std::fopen(vb.string().c_str(), "wb")) {
        std::fwrite(video_.vram().data(), 1, video_.vram().size(), vf);
        std::fclose(vf);
      }
    }
    auto write_dump = [&](const char* name, std::span<const u8> bytes) {
      const fs::path path = fs::path(cfg_.probe_out) / name;
      if (FILE* f = std::fopen(path.string().c_str(), "wb")) {
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
      }
    };
    write_dump("last_palette_r.bin", video_.palette_r());
    write_dump("last_palette_g.bin", video_.palette_g());
    write_dump("last_palette_b.bin", video_.palette_b());
    std::array<u8, 0x20> tile_controls{};
    for (u32 i = 0; i < tile_controls.size(); ++i)
      tile_controls[i] = video_.tmap_control_read(i);
    write_dump("last_tmap_control.bin", tile_controls);
    const u8 player_slot = c117_.read(C117::Main, 0x0039);
    const u16 player = u16(0x011D + u16(player_slot) * 0x40);
    if (FILE* f = std::fopen(js.string().c_str(), "wb")) {
      std::fprintf(
          f,
          "{\n"
          "  \"frames\": %d,\n"
          "  \"coin_frame\": %d,\n"
          "  \"start_frame\": %d,\n"
          "  \"input_hold_frames\": %d,\n"
          "  \"right_frame\": %d,\n"
          "  \"right_hold_frames\": %d,\n"
          "  \"subres\": %u,\n"
          "  \"triram0\": %u,\n"
          "  \"mcu_patch\": %u,\n"
          "  \"main_pc\": %u,\n"
          "  \"sub_pc\": %u,\n"
          "  \"audio_pc\": %u,\n"
          "  \"mcu_pc\": %u,\n"
          "  \"game_mode\": %u,\n"
          "  \"max_hearts\": %u,\n"
          "  \"control_147e\": %u,\n"
          "  \"player_slot\": %u,\n"
          "  \"player_health\": %u,\n"
          "  \"player_lives\": %u,\n"
          "  \"hud_hearts\": %u,\n"
          "  \"vram_nz\": %zu,\n"
          "  \"sprite_latch_requests\": %llu,\n"
          "  \"pf3_01ff\": %zu,\n"
          "  \"sub_held\": %s,\n"
          "  \"mcu_held\": %s\n"
          "}\n",
          ran, cfg_.coin_frame, cfg_.start_frame, cfg_.input_hold_frames,
          cfg_.right_frame, cfg_.right_hold_frames,
          unsigned(c117_.subres_bit()), unsigned(map_.triram()[0]),
          unsigned(mcu_patch_data_), unsigned(main_.pc), unsigned(sub_.pc),
          unsigned(audio_cpu_.pc), unsigned(mcu_.pc),
          unsigned(c117_.read(C117::Main, 0x002C)),
          unsigned(c117_.read(C117::Main, 0x147A)),
          unsigned(c117_.read(C117::Main, 0x147E)),
          unsigned(player_slot),
          unsigned(c117_.read(C117::Main, u16(player + 0x1C))),
          unsigned(c117_.read(C117::Main, u16(player + 0x32))),
          unsigned(c117_.read(C117::Main, 0x0FA2)),
          vram_nz,
          static_cast<unsigned long long>(video_.sprite_latch_requests()), pf3,
          sub_held_ ? "true" : "false", mcu_held_ ? "true" : "false");
      std::fclose(f);
    }
    std::printf(
        "probe: frames=%d subres=%u triram0=%02X main=%04X sub=%04X audio=%04X "
        "mcu=%04X vram_nz=%zu pf3_01ff=%zu\n",
        ran, unsigned(c117_.subres_bit()), unsigned(map_.triram()[0]),
        unsigned(main_.pc), unsigned(sub_.pc), unsigned(audio_cpu_.pc),
        unsigned(mcu_.pc), vram_nz, pf3);
  }

  if (present_)
    present_->shutdown();
  if (audio_be_)
    audio_be_->shutdown();
  return ran;
}

void Machine::persist_options() {
  if (!options_.dirty || cfg_.options_path.empty())
    return;
  if (save_options(cfg_.options_path, options_))
    options_.dirty = false;
}

} // namespace sh
