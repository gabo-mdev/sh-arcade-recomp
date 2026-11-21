#pragma once

#include "sh/types.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace sh {

/// Minimal HD63701 (6801-family) for Namco System 1 MCU I/O.
class HD63701 {
public:
  static constexpr u8 CC_H = 0x20;
  static constexpr u8 CC_I = 0x10;
  static constexpr u8 CC_N = 0x08;
  static constexpr u8 CC_Z = 0x04;
  static constexpr u8 CC_V = 0x02;
  static constexpr u8 CC_C = 0x01;

  using BusRead = std::function<u8(u16)>;
  using BusWrite = std::function<void(u16, u8)>;
  using PortReadFn = std::function<u8()>;
  using PortWriteFn = std::function<void(u8)>;

  HD63701() = default;

  void set_bus(BusRead read, BusWrite write);
  void set_internal_rom(std::span<const u8> rom);
  void set_port_read(PortReadFn p1, PortReadFn p2);
  void set_port_write(PortWriteFn p1, PortWriteFn p2);

  void reset();

  int step();
  int run(int cycles);

  void set_irq(bool asserted);

  u16 pc = 0;
  u8 a = 0;
  u8 b = 0;
  u16 x = 0;
  u16 s = 0;
  u8 cc = CC_I;

  u64 cycles_total = 0;

private:
  enum class Mode : u8 { Inherent, Immediate, Direct, Extended, Indexed, Relative };

  BusRead read_;
  BusWrite write_;
  std::span<const u8> internal_rom_{};
  PortReadFn p1_read_;
  PortReadFn p2_read_;
  PortWriteFn p1_write_;
  PortWriteFn p2_write_;

  std::array<u8, 256> ram_{};
  u8 p1_ddr_ = 0;
  u8 p2_ddr_ = 0;
  u8 p1_data_ = 0;
  u8 p2_data_ = 0;

  // HD63701 free-running timer (E-clock cycles) and output compare.
  u16 timer_counter_ = 0;
  u16 timer_compare_ = 0xFFFF;
  u8 timer_control_ = 0;
  bool timer_compare_ack_ = false;
  bool timer_overflow_ack_ = false;
  int run_debt_ = 0;
  void advance_timer(int cycles);

  bool irq_line_ = false;

  u8 fetch8();
  u16 fetch16();
  u8 read8(u16 addr);
  void write8(u16 addr, u8 val);

  u16 ea(Mode mode);
  u16 ea_indexed();

  void set_nz8(u8 v);
  u8 add8(u8 v, u8 n, bool carry);
  u8 sub8(u8 v, u8 n, bool borrow);
  u8 and8(u8 v, u8 n);
  u8 or8(u8 v, u8 n);
  u8 eor8(u8 v, u8 n);
  u8 inc8(u8 v);
  u8 dec8(u8 v);

  u16 d_reg() const;
  void set_d(u16 v);

  void push8(u8 v);
  void push16(u16 v);
  u8 pull8();
  u16 pull16();

  int service_irq();
  int exec_one(u8 op);
  int branch8(bool take, int not_taken);
};

} // namespace sh
