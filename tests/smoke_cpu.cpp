#include "cpu/m6809.hpp"
#include "cpu/hd63701.hpp"

#include <array>
#include <cstdio>

using namespace sh;

int main() {
  std::array<u8, 0x10000> mem{};
  // LDA #$42 ; NOP ; JMP $0000
  mem[0x0000] = 0x86;
  mem[0x0001] = 0x42;
  mem[0x0002] = 0x12; // NOP
  mem[0x0003] = 0x7E;
  mem[0x0004] = 0x00;
  mem[0x0005] = 0x00;
  mem[0xFFFE] = 0x00;
  mem[0xFFFF] = 0x00;

  M6809 cpu;
  cpu.set_bus([&](u16 a) { return mem[a]; },
              [&](u16 a, u8 d) { mem[a] = d; });
  cpu.reset();
  if (cpu.pc != 0x0000) {
    std::printf("FAIL reset PC=%04X\n", cpu.pc);
    return 1;
  }
  // HD63701 branch opcodes: BVC/BVS test V, BPL/BMI test N.
  std::array<u8, 0x1000> mcu_rom{};
  HD63701 mcu;
  mcu.set_internal_rom(mcu_rom);
  const auto check_branch = [&](u8 opcode, u8 flags, bool taken) {
    mcu_rom[0] = opcode;
    mcu_rom[1] = 2;
    mcu.pc = 0xF000;
    mcu.cc = u8(HD63701::CC_I | flags);
    mcu.step();
    const u16 expected = taken ? 0xF004 : 0xF002;
    if (mcu.pc != expected) {
      std::printf("FAIL HD63701 branch %02X flags=%02X PC=%04X expected=%04X\n",
                  opcode, flags, mcu.pc, expected);
      return false;
    }
    return true;
  };
  if (!check_branch(0x28, 0, true) ||
      !check_branch(0x28, HD63701::CC_V, false) ||
      !check_branch(0x29, HD63701::CC_V, true) ||
      !check_branch(0x29, 0, false) ||
      !check_branch(0x2A, 0, true) ||
      !check_branch(0x2A, HD63701::CC_N, false) ||
      !check_branch(0x2B, HD63701::CC_N, true) ||
      !check_branch(0x2B, 0, false))
    return 1;
  cpu.step(); // LDA
  if (cpu.a != 0x42) {
    std::printf("FAIL LDA A=%02X\n", cpu.a);
    return 1;
  }
  cpu.step(); // NOP
  cpu.step(); // JMP
  if (cpu.pc != 0x0000) {
    std::printf("FAIL JMP PC=%04X\n", cpu.pc);
    return 1;
  }
  std::printf("smoke_cpu OK cycles=%llu\n",
              static_cast<unsigned long long>(cpu.cycles_total));
  return 0;
}
