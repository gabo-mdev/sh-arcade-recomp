#pragma once

#include "sh/types.hpp"

#include <functional>

namespace sh {

/// MC6809E interpreter (MAME-compatible cycle timing).
class M6809 {
public:
  static constexpr u8 CC_E = 0x80;
  static constexpr u8 CC_F = 0x40;
  static constexpr u8 CC_H = 0x20;
  static constexpr u8 CC_I = 0x10;
  static constexpr u8 CC_N = 0x08;
  static constexpr u8 CC_Z = 0x04;
  static constexpr u8 CC_V = 0x02;
  static constexpr u8 CC_C = 0x01;
  static constexpr u8 CC_NZVC = CC_N | CC_Z | CC_V | CC_C;
  static constexpr u8 CC_NZV = CC_N | CC_Z | CC_V;

  using BusRead = std::function<u8(u16)>;
  using BusWrite = std::function<void(u16, u8)>;
  using TraceFn = std::function<void(const M6809&)>;

  M6809() = default;

  void set_bus(BusRead read, BusWrite write);
  void set_trace(TraceFn fn) { trace_ = std::move(fn); }

  void reset();

  int step();
  int run(int cycles);

  void set_irq(bool asserted);
  void set_firq(bool asserted);
  void set_nmi(bool asserted);
  void set_halt(bool halted);
  void set_reset_hold(bool held);

  u16 pc = 0;
  u8 a = 0;
  u8 b = 0;
  u16 x = 0;
  u16 y = 0;
  u16 u = 0;
  u16 s = 0;
  u8 dp = 0;
  u8 cc = CC_I | CC_F;

  u64 cycles_total = 0;
  // When set, JSR $CFB9 is not taken if U is this body.
  bool skip_hurt = false;
  u16 hurt_body = 0;

private:
  struct M6809Core;
  friend struct M6809Core;

  BusRead read_;
  BusWrite write_;
  TraceFn trace_;

  bool irq_line_ = false;
  bool firq_line_ = false;
  bool nmi_line_ = false;
  bool nmi_pending_ = false;
  bool halt_ = false;
  bool reset_hold_ = false;
};

} // namespace sh
