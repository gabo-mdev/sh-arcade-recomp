#include "machine/machine.hpp"
#include "machine/romset.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  const int end = argc > 1 ? std::atoi(argv[1]) : 3500;
  const int right_at = argc > 2 ? std::atoi(argv[2]) : -1;
  const int attack_at = argc > 3 ? std::atoi(argv[3]) : -1;
  sh::MachineConfig cfg;
  cfg.headless = true;
  cfg.trace_audio = true;
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
  machine.set_audio_backend(sh::make_null_audio());
  sh::u16 last_pc = 0;
  sh::u8 last_0 = 0, last_1 = 0, last_command = 0;
  sh::u8 last_voice_command[4]{};
  sh::u64 last_dac0 = 0, last_dac1 = 0;
  sh::u64 register_changes = 0;
  sh::u8 last_regs[64]{};
  for (int frame = 0; frame < end; ++frame) {
    machine.run_frame();
    const sh::u8 a = machine.virtmap().read(0x2FE000);
    const sh::u8 b = machine.virtmap().read(0x2FE001);
    const sh::u16 pc = machine.audiocpu().pc;
    const sh::u8 command = machine.virtmap().read(0x2FE3F3);
    if (frame >= 3300) {
      for (int i = 0; i < 4; ++i) {
        const sh::u8 value = machine.virtmap().triram()[0x2B + i];
        if (value != last_voice_command[i])
          std::printf("voice-command f=%d off=%02X value=%02X\n",
                      frame, 0x2B + i, value);
        last_voice_command[i] = value;
      }
    }
    if (frame >= cfg.coin_frame - 2 && frame <= cfg.coin_frame + 20)
      std::printf("coin f=%d main=%04X sub=%04X mode=%02X tri2A=%02X tri2B=%02X credits=%02X command=%02X\n",
                  frame, machine.maincpu().pc, machine.subcpu().pc,
                  machine.c117().read(sh::C117::Main, 0x002C),
                  machine.virtmap().triram()[0x2A],
                  machine.virtmap().triram()[0x2B],
                  machine.video().c116_read(0x6015), command);
    const sh::u64 dac0 = machine.audio().dac_writes(0);
    const sh::u64 dac1 = machine.audio().dac_writes(1);
    if (frame % 250 == 0)
      std::printf("audio-stats f=%d mcu=%04X dac=%llu,%llu\n", frame,
                  machine.mcu().pc,
                  static_cast<unsigned long long>(dac0),
                  static_cast<unsigned long long>(dac1));
    if (frame >= 3300 && (dac0 != last_dac0 || dac1 != last_dac1))
      std::printf("dac f=%d writes=%llu,%llu noncenter=%llu,%llu\n", frame,
                  static_cast<unsigned long long>(dac0 - last_dac0),
                  static_cast<unsigned long long>(dac1 - last_dac1),
                  static_cast<unsigned long long>(machine.audio().dac_noncenter_writes(0)),
                  static_cast<unsigned long long>(machine.audio().dac_noncenter_writes(1)));
    last_dac0 = dac0;
    last_dac1 = dac1;
    for (int reg = 0; reg < 64; ++reg) {
      const sh::u8 value = machine.virtmap().read(0x2FE100 + reg);
      if (value != last_regs[reg]) {
        ++register_changes;
        std::printf("register f=%d off=%03X value=%02X\n", frame,
                    0x100 + reg, value);
        last_regs[reg] = value;
      }
    }
    if (frame < 25 || (frame < 300 && frame % 10 == 0) ||
        frame % 100 == 0 || a != last_0 || b != last_1 ||
        command != last_command ||
        (pc != last_pc && frame < 100)) {
      std::printf("f=%d pc=%04X cus30=%02X,%02X regs=%02X,%02X command=%02X mode=%02X\n",
                  frame, pc, a, b,
                  machine.virtmap().read(0x2FE100),
                  machine.virtmap().read(0x2FE104),
                  command,
                  machine.c117().read(sh::C117::Main, 0x002C));
    }
    last_pc = pc;
    last_0 = a;
    last_1 = b;
    last_command = command;
  }
  std::printf("register_changes=%llu\n",
              static_cast<unsigned long long>(register_changes));
}
