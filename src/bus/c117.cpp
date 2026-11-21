#include "bus/c117.hpp"
#include "bus/virtmap.hpp"

#include <cstdio>

namespace sh {

C117::C117(VirtMap& map) : map_(map) {}

void C117::reset() {
  offsets_ = {};
  offsets_[Main][0] = 0x0180u * 0x2000u;
  offsets_[Main][1] = 0x0180u * 0x2000u;
  offsets_[Main][7] = 0x03FFu * 0x2000u;
  offsets_[Sub][0] = 0x0180u * 0x2000u;
  offsets_[Sub][7] = 0x03FFu * 0x2000u;
  subres_bit_ = 0;
  // MAME asserts SUBRES line at reset → hold non-main devices in reset.
  if (subres_cb_)
    subres_cb_(true);
}

u8 C117::read(CpuId cpu, u16 addr) {
  return map_.read(remap(cpu, addr));
}

void C117::write(CpuId cpu, u16 addr, u8 data) {
  if (addr < 0xE000)
    map_.write(remap(cpu, addr), data);
  else
    register_w(cpu, addr, data);
}

void C117::bankswitch(CpuId cpu, int bank, int a0, u8 data) {
  u32& b = offsets_[cpu][bank];
  if (a0 == 0) {
    // Main ROM windows must keep a chip-select. C127 / IRQ tails can STA
    // bank regs with A=0 as an address strobe; chip 0 maps below 0x400000
    // (open bus / workram) and the CPU wanders.
    u8 chip = data & 0x03;
    if (cpu == Main && (bank == 0 || bank == 6 || bank == 7) && chip == 0)
      chip = (bank == 0) ? 0x01 : 0x03;
    b = (b & 0x1FE000u) | (u32(chip) * 0x200000u);
  } else {
    b = (b & 0x600000u) | (u32(data) * 0x2000u);
  }
}

void C117::register_w(CpuId cpu, u16 offset, u8 data) {
  const int reg = (offset >> 9) & 0xF;
  switch (reg) {
  case 0: case 1: case 2: case 3:
  case 4: case 5: case 6: case 7:
    bankswitch(cpu, reg, offset & 1, data);
    break;
  case 8: // F000 SUBRES
    if (cpu == Main) {
      subres_bit_ = data & 1;
      // MAME: m_subres_cb(m_subres ? CLEAR_LINE : ASSERT_LINE)
      if (subres_cb_)
        subres_cb_(subres_bit_ == 0);
    }
    break;
  case 9:
    if (watchdog_)
      watchdog_(cpu);
    break;
  case 11:
    if (irq_ack_)
      irq_ack_(cpu, false);
    break;
  case 12:
    if (firq_ack_)
      firq_ack_(cpu, false);
    break;
  case 13:
    if (cpu == Main && assert_sub_firq_)
      assert_sub_firq_();
    break;
  case 14:
    if (cpu == Main)
      offsets_[Sub][7] = 0x600000u | (u32(data) * 0x2000u);
    break;
  default:
    break;
  }
}

} // namespace sh
