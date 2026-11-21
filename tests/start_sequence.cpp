#include "machine/machine.hpp"
#include "machine/romset.hpp"

#include <cstdio>

int main() {
  const auto roms = sh::RomSet::load("rom");
  sh::MachineConfig cfg;
  cfg.headless = true;
  cfg.coin_frame = 2100;
  cfg.start_frame = 2200;
  cfg.right_frame = 3350;
  cfg.right_hold_frames = 500;
  sh::Machine machine(cfg);
  machine.load(roms);
  machine.reset();
  machine.set_input(sh::make_null_input());
  machine.set_present(sh::make_null_present());
  machine.set_audio_backend(sh::make_null_audio());
  const auto run_until = [&](sh::u64 frame) {
    while (machine.frame_index() < frame)
      machine.run_frame();
  };

  run_until(2200);
  if (machine.video().c116_read(0x6015) != 1) {
    std::fprintf(stderr, "scheduled coin did not produce one credit\n");
    return 1;
  }
  run_until(2250);
  if (machine.video().tmap_control_read(0x10) != 5 ||
      machine.video().tmap_control_read(0x19) != 2) {
    std::fprintf(stderr, "scheduled Start did not enter the mask scene\n");
    return 1;
  }
  run_until(2700); // middle of the mask scene
  const auto& mask_vram = machine.video().vram();
  if (mask_vram[0x74B9] != 0x20 || mask_vram[0x74BB] != 0x20 ||
      (machine.c117().read(sh::C117::Main, 0x147E) & 0x20) != 0) {
    std::fprintf(stderr, "stage-select number appeared in the mask scene\n");
    return 1;
  }
  const auto health = [&]() {
    const sh::u8 slot = machine.c117().read(sh::C117::Main, 0x39);
    return machine.c117().read(sh::C117::Main,
                               sh::u16(0x011D + sh::u16(slot) * 0x40 + 0x1C));
  };
  const auto hearts = [&]() { return machine.c117().read(sh::C117::Main, 0x0FA2); };
  run_until(3350); // first playable scene, before moving right
  if (health() != 4 || hearts() != 4) {
    std::fprintf(stderr, "player did not spawn with four hearts\n");
    return 1;
  }
  // Reproduce the reported first-start HUD mismatch without changing health.
  machine.c117().write(sh::C117::Main, 0x0FA2, 3);
  machine.run_frame();
  if (health() != 4 || hearts() != 4) {
    std::fprintf(stderr, "full-health HUD did not recover its fourth heart\n");
    return 1;
  }
  run_until(3460); // first enemy contact
  if (health() != 3 || hearts() != 3 ||
      machine.c117().read(sh::C117::Main, 0x2C) != 0x0A) {
    std::fprintf(stderr, "first hit: health=%u hearts=%u mode=%02X\n",
                 unsigned(health()), unsigned(hearts()),
                 unsigned(machine.c117().read(sh::C117::Main, 0x2C)));
    return 1;
  }
  run_until(3540); // a later, separate contact
  if (health() != 2 || hearts() != 2) {
    std::fprintf(stderr, "second hit did not remove exactly one heart\n");
    return 1;
  }
  run_until(3700); // later contacts exhaust the remaining hearts
  if (health() != 0 || hearts() != 0) {
    std::fprintf(stderr, "damage did not exhaust all four hearts\n");
    return 1;
  }
  run_until(4230); // death transition and automatic respawn
  const sh::u8 slot = machine.c117().read(sh::C117::Main, 0x39);
  const sh::u8 lives = machine.c117().read(
      sh::C117::Main, sh::u16(0x011D + sh::u16(slot) * 0x40 + 0x32));
  if (health() != 4 || hearts() != 4 || lives != 2 ||
      machine.c117().read(sh::C117::Main, 0x2C) != 0x0A) {
    std::fprintf(stderr, "respawn: health=%u hearts=%u lives=%u mode=%02X\n",
                 unsigned(health()), unsigned(hearts()), unsigned(lives),
                 unsigned(machine.c117().read(sh::C117::Main, 0x2C)));
    return 1;
  }
  // D-pad input during possession must not reveal or change the stage number.
  sh::MachineConfig mask_cfg = cfg;
  mask_cfg.right_frame = 2450;
  mask_cfg.right_hold_frames = 180;
  sh::Machine mask_machine(mask_cfg);
  mask_machine.load(roms);
  mask_machine.reset();
  mask_machine.set_input(sh::make_null_input());
  mask_machine.set_present(sh::make_null_present());
  mask_machine.set_audio_backend(sh::make_null_audio());
  while (mask_machine.frame_index() < 2700)
    mask_machine.run_frame();
  const auto& dpad_vram = mask_machine.video().vram();
  if (dpad_vram[0x74B9] != 0x20 || dpad_vram[0x74BB] != 0x20 ||
      (mask_machine.c117().read(sh::C117::Main, 0x147E) & 0x20) != 0) {
    std::fprintf(stderr, "D-pad exposed stage-select during possession\n");
    return 1;
  }

  sh::Machine god(cfg);
  god.load(roms);
  god.reset();
  god.options().invincible = true;
  god.set_input(sh::make_null_input());
  god.set_present(sh::make_null_present());
  god.set_audio_backend(sh::make_null_audio());
  while (god.frame_index() < 3700)
    god.run_frame();
  const sh::u8 god_slot = god.c117().read(sh::C117::Main, 0x39);
  const sh::u8 god_health = god.c117().read(
      sh::C117::Main, sh::u16(0x011D + sh::u16(god_slot) * 0x40 + 0x1C));
  const sh::u8 god_hearts = god.c117().read(sh::C117::Main, 0x0FA2);
  const sh::u8 god_lives = god.c117().read(
      sh::C117::Main, sh::u16(0x011D + sh::u16(god_slot) * 0x40 + 0x32));
  if (god_health != 4 || god_hearts != 4 || god_lives != 3 ||
      god.c117().read(sh::C117::Main, 0x2C) != 0x0A) {
    std::fprintf(stderr, "invincible: health=%u hearts=%u lives=%u mode=%02X\n",
                 unsigned(god_health), unsigned(god_hearts), unsigned(god_lives),
                 unsigned(god.c117().read(sh::C117::Main, 0x2C)));
    return 1;
  }
  return 0;
}
