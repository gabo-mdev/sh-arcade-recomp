#include "machine/machine.hpp"
#include "machine/romset.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  const int right_at = argc > 1 ? std::atoi(argv[1]) : 3350;
  const int end = argc > 2 ? std::atoi(argv[2]) : 4500;
  const int attack_at = argc > 3 ? std::atoi(argv[3]) : -1;
  const auto roms = sh::RomSet::load("rom");
  sh::MachineConfig cfg;
  cfg.headless = true;
  cfg.coin_frame = 2100;
  cfg.start_frame = 2200;
  cfg.right_frame = right_at;
  cfg.right_hold_frames = end - right_at;
  cfg.attack_frame = attack_at;
  sh::Machine machine(cfg);
  machine.load(roms);
  machine.reset();
  machine.set_input(sh::make_null_input());
  machine.set_present(sh::make_null_present());
  machine.set_audio_backend(sh::make_null_audio());
  sh::u8 last_mode = 0xff, last_health = 0xff, last_actor = 0xff;
  for (int frame = 0; frame < end; ++frame) {
    machine.run_frame();
    const auto rd = [&](sh::u16 addr) { return machine.c117().read(sh::C117::Main, addr); };
    const sh::u8 mode = rd(0x002C);
    const sh::u8 slot = rd(0x0039);
    const sh::u16 player = sh::u16(0x011D + sh::u16(slot) * 0x40);
    const sh::u8 health = rd(sh::u16(player + 0x1C));
    const sh::u8 actor = rd(0x0F9D);
    if (frame >= right_at - 10 &&
        (mode != last_mode || health != last_health || actor != last_actor || frame % 10 == 0)) {
      std::printf("f=%d mode=%02X health=%u hearts=%u lives=%u actor=%02X char=%02X "
                  "147A=%02X 147E=%02X pc=%04X sub=%04X\n",
                  frame, mode, health, rd(0x0FA2), rd(sh::u16(player + 0x32)),
                  actor, rd(0x0FA6), rd(0x147A), rd(0x147E),
                  machine.maincpu().pc, machine.subcpu().pc);
    }
    last_mode = mode;
    last_health = health;
    last_actor = actor;
  }
}
