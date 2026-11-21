#include "cpu/hd63701.hpp"

#include <cstdio>

namespace sh {

namespace {
constexpr u16 kRomBase = 0xF000;
constexpr u16 kP1ddr = 0x0000;
constexpr u16 kP1dr = 0x0001;
constexpr u16 kP2ddr = 0x0002;
constexpr u16 kP2dr = 0x0003;
constexpr u16 kTimerControl = 0x0008;
constexpr u16 kTimerCounterHi = 0x0009;
constexpr u16 kTimerCounterLo = 0x000A;
constexpr u16 kTimerCompareHi = 0x000B;
constexpr u16 kTimerCompareLo = 0x000C;
constexpr u8 kTimerCompareFlag = 0x40;
constexpr u8 kTimerOverflowFlag = 0x20;
constexpr u8 kTimerCompareEnable = 0x08;
constexpr u8 kTimerOverflowEnable = 0x04;
constexpr u16 kVecTimerCompare = 0xFFF4;
constexpr u16 kVecTimerOverflow = 0xFFF2;
constexpr u16 kVecIrq = 0xFFF8;
constexpr u16 kVecReset = 0xFFFE;
constexpr u16 kVecSwi = 0xFFFC;

inline u16 r16(u8 hi, u8 lo) { return (static_cast<u16>(hi) << 8) | lo; }

void set_nz8(HD63701& c, u8 v) {
  c.cc &= ~(HD63701::CC_N | HD63701::CC_Z | HD63701::CC_V);
  if (v & 0x80)
    c.cc |= HD63701::CC_N;
  if (v == 0)
    c.cc |= HD63701::CC_Z;
}

void set_nz16(HD63701& c, u16 v) {
  c.cc &= ~(HD63701::CC_N | HD63701::CC_Z | HD63701::CC_V);
  if (v & 0x8000)
    c.cc |= HD63701::CC_N;
  if (v == 0)
    c.cc |= HD63701::CC_Z;
}
} // namespace

void HD63701::set_bus(BusRead read, BusWrite write) {
  read_ = std::move(read);
  write_ = std::move(write);
}
void HD63701::set_internal_rom(std::span<const u8> rom) { internal_rom_ = rom; }
void HD63701::set_port_read(PortReadFn p1, PortReadFn p2) {
  p1_read_ = std::move(p1);
  p2_read_ = std::move(p2);
}
void HD63701::set_port_write(PortWriteFn p1, PortWriteFn p2) {
  p1_write_ = std::move(p1);
  p2_write_ = std::move(p2);
}

u8 HD63701::fetch8() { return read8(pc++); }
u16 HD63701::fetch16() {
  // 6801/63701 is big-endian.
  const u8 hi = fetch8();
  const u8 lo = fetch8();
  return r16(hi, lo);
}

u8 HD63701::read8(u16 addr) {
  if (addr == kP1ddr)
    return p1_ddr_;
  if (addr == kP1dr) {
    const u8 ext = p1_read_ ? p1_read_() : u8(0xFF);
    // Bits marked as outputs come from the data reg; inputs from the host.
    // Namco wires coin/service on the input side — if DDR accidentally marks
    // them as outs, force-host those active-low input bits (3..7).
    const u8 ddr = u8(p1_ddr_ & 0x07);
    return u8((p1_data_ & ddr) | (ext & ~ddr));
  }
  if (addr == kP2ddr)
    return p2_ddr_;
  if (addr == kP2dr) {
    const u8 ext = p2_read_ ? p2_read_() : u8(0xFF);
    return u8((p2_data_ & p2_ddr_) | (ext & ~p2_ddr_));
  }
  if (addr == kTimerControl) {
    timer_compare_ack_ = (timer_control_ & kTimerCompareFlag) != 0;
    timer_overflow_ack_ = (timer_control_ & kTimerOverflowFlag) != 0;
    return timer_control_;
  }
  if (addr == kTimerCounterHi)
    return u8(timer_counter_ >> 8);
  if (addr == kTimerCounterLo) {
    if (timer_overflow_ack_)
      timer_control_ &= ~kTimerOverflowFlag;
    timer_overflow_ack_ = false;
    return u8(timer_counter_);
  }
  if (addr == kTimerCompareHi)
    return u8(timer_compare_ >> 8);
  if (addr == kTimerCompareLo) {
    if (timer_compare_ack_)
      timer_control_ &= ~kTimerCompareFlag;
    timer_compare_ack_ = false;
    return u8(timer_compare_);
  }
  if (addr >= kRomBase) {
    if (!internal_rom_.empty()) {
      const u32 off = addr - kRomBase;
      if (off < internal_rom_.size())
        return internal_rom_[off];
    }
    return read_ ? read_(addr) : u8(0xFF);
  }
  if (addr < ram_.size())
    return ram_[addr];
  return read_ ? read_(addr) : u8(0xFF);
}

void HD63701::write8(u16 addr, u8 val) {
  if (addr == kP1ddr) {
    p1_ddr_ = val;
    return;
  }
  if (addr == kP1dr) {
    p1_data_ = val;
    if (p1_write_)
      p1_write_(u8(val & p1_ddr_));
    return;
  }
  if (addr == kP2ddr) {
    p2_ddr_ = val;
    return;
  }
  if (addr == kP2dr) {
    p2_data_ = val;
    if (p2_write_)
      p2_write_(u8(val & p2_ddr_));
    return;
  }
  if (addr == kTimerControl) {
    timer_control_ = u8((timer_control_ & 0xE0) | (val & 0x1F));
    return;
  }
  if (addr == kTimerCounterHi) {
    timer_counter_ = 0;
    return;
  }
  if (addr == kTimerCounterLo)
    return;
  if (addr == kTimerCompareHi || addr == kTimerCompareLo) {
    timer_compare_ = addr == kTimerCompareHi
                         ? u16((u16(val) << 8) | (timer_compare_ & 0xFF))
                         : u16((timer_compare_ & 0xFF00) | val);
    if (timer_compare_ack_)
      timer_control_ &= ~kTimerCompareFlag;
    timer_compare_ack_ = false;
    return;
  }
  if (addr < ram_.size()) {
    ram_[addr] = val;
    return;
  }
  if (write_)
    write_(addr, val);
}

void HD63701::reset() {
  a = b = 0;
  x = 0xFFFF;
  s = 0x00FF;
  cc = CC_I;
  cycles_total = 0;
  irq_line_ = false;
  timer_counter_ = 0;
  timer_compare_ = 0xFFFF;
  timer_control_ = 0;
  timer_compare_ack_ = timer_overflow_ack_ = false;
  run_debt_ = 0;
  pc = r16(read8(kVecReset), read8(u16(kVecReset + 1)));
}

void HD63701::set_irq(bool asserted) { irq_line_ = asserted; }

void HD63701::set_nz8(u8 v) { ::sh::set_nz8(*this, v); }

void HD63701::push8(u8 v) { write8(s--, v); }
void HD63701::push16(u16 v) {
  push8(u8(v));
  push8(u8(v >> 8));
}
u8 HD63701::pull8() { return read8(++s); }
u16 HD63701::pull16() {
  const u8 hi = pull8();
  const u8 lo = pull8();
  return r16(hi, lo);
}

u16 HD63701::d_reg() const { return r16(a, b); }
void HD63701::set_d(u16 v) {
  a = u8(v >> 8);
  b = u8(v);
}

u16 HD63701::ea(Mode mode) {
  switch (mode) {
  case Mode::Direct:
    return fetch8();
  case Mode::Extended:
    return fetch16();
  case Mode::Indexed:
    // 6801 indexed: unsigned 8-bit offset from X (not 6809 postbyte).
    return u16(x + fetch8());
  default:
    return 0;
  }
}

u16 HD63701::ea_indexed() { return ea(Mode::Indexed); }

u8 HD63701::add8(u8 v, u8 n, bool carry_in) {
  const u16 r = u16(v) + n + (carry_in && (cc & CC_C) ? 1 : 0);
  cc &= ~(CC_H | CC_N | CC_Z | CC_V | CC_C);
  if ((v ^ n ^ u8(r)) & 0x10)
    cc |= CC_H;
  if (r & 0x80)
    cc |= CC_N;
  if (u8(r) == 0)
    cc |= CC_Z;
  if (~(v ^ n) & (v ^ u8(r)) & 0x80)
    cc |= CC_V;
  if (r & 0x100)
    cc |= CC_C;
  return u8(r);
}

u8 HD63701::sub8(u8 v, u8 n, bool borrow_in) {
  const u16 r = u16(v) - n - (borrow_in && (cc & CC_C) ? 1 : 0);
  cc &= ~(CC_H | CC_N | CC_Z | CC_V | CC_C);
  if ((v ^ n ^ u8(r)) & 0x10)
    cc |= CC_H;
  if (r & 0x80)
    cc |= CC_N;
  if (u8(r) == 0)
    cc |= CC_Z;
  if ((v ^ n) & (v ^ u8(r)) & 0x80)
    cc |= CC_V;
  if (r & 0x100)
    cc |= CC_C;
  return u8(r);
}

u8 HD63701::and8(u8 v, u8 n) {
  v &= n;
  set_nz8(v);
  return v;
}
u8 HD63701::or8(u8 v, u8 n) {
  v |= n;
  set_nz8(v);
  return v;
}
u8 HD63701::eor8(u8 v, u8 n) {
  v ^= n;
  set_nz8(v);
  return v;
}

u8 HD63701::inc8(u8 v) {
  const u8 r = u8(v + 1);
  set_nz8(r);
  if (v == 0x7F)
    cc |= CC_V;
  return r;
}
u8 HD63701::dec8(u8 v) {
  const u8 r = u8(v - 1);
  set_nz8(r);
  if (v == 0x80)
    cc |= CC_V;
  return r;
}

int HD63701::branch8(bool take, int base) {
  const s8 d = static_cast<s8>(fetch8());
  if (take)
    pc = u16(pc + d);
  return take ? base + 1 : base;
}

int HD63701::service_irq() {
  if (!(cc & CC_I)) {
    u16 vector = 0;
    if (irq_line_)
      vector = kVecIrq;
    else if ((timer_control_ & (kTimerCompareEnable | kTimerCompareFlag)) ==
             (kTimerCompareEnable | kTimerCompareFlag))
      vector = kVecTimerCompare;
    else if ((timer_control_ & (kTimerOverflowEnable | kTimerOverflowFlag)) ==
             (kTimerOverflowEnable | kTimerOverflowFlag))
      vector = kVecTimerOverflow;
    if (!vector)
      return 0;
    push16(pc);
    push16(x);
    push8(a);
    push8(b);
    push8(cc);
    cc |= CC_I;
    pc = r16(read8(vector), read8(u16(vector + 1)));
    return 12;
  }
  return 0;
}

void HD63701::advance_timer(int cycles) {
  // Compare and overflow are independent, sticky events. A compare value is
  // encountered once per 16-bit wrap, even when an instruction spans it.
  const u16 before = timer_counter_;
  timer_counter_ = u16(timer_counter_ + cycles);
  const u16 compare_distance = u16(timer_compare_ - before);
  if (compare_distance != 0 && compare_distance <= cycles)
    timer_control_ |= kTimerCompareFlag;
  if (u32(before) + u32(cycles) >= 0x10000)
    timer_control_ |= kTimerOverflowFlag;
}

int HD63701::exec_one(u8 op) {
  switch (op) {
  // ---- inherent / CC ----
  case 0x01: // NOP
    return 2;
  case 0x04: // LSRD
  {
    u16 d = d_reg();
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (d & 1)
      cc |= CC_C;
    d >>= 1;
    set_d(d);
    set_nz16(*this, d);
    return 3;
  }
  case 0x05: // ASLD
  {
    u16 d = d_reg();
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (d & 0x8000)
      cc |= CC_C;
    d = u16(d << 1);
    set_d(d);
    set_nz16(*this, d);
    return 3;
  }
  case 0x06: // TAP
    cc = a & 0x3F;
    return 2;
  case 0x07: // TPA
    a = cc;
    return 2;
  case 0x08: // INX
    ++x;
    cc &= ~CC_Z;
    if (x == 0)
      cc |= CC_Z;
    return 3;
  case 0x09: // DEX
    --x;
    cc &= ~CC_Z;
    if (x == 0)
      cc |= CC_Z;
    return 3;
  case 0x0A: // CLV
    cc &= ~CC_V;
    return 2;
  case 0x0B: // SEV
    cc |= CC_V;
    return 2;
  case 0x0C: // CLC
    cc &= ~CC_C;
    return 2;
  case 0x0D: // SEC
    cc |= CC_C;
    return 2;
  case 0x0E: // CLI
    cc &= ~CC_I;
    return 2;
  case 0x0F: // SEI
    cc |= CC_I;
    return 2;
  case 0x10: // SBA
    a = sub8(a, b, false);
    return 2;
  case 0x11: // CBA
    (void)sub8(a, b, false);
    return 2;
  case 0x12: // NOP (undoc/6801)
  case 0x13:
    return 2;
  case 0x14: // NBA (undoc) / ignore
    return 2;
  case 0x15:
    return 2;
  case 0x16: // TAB
    b = a;
    set_nz8(b);
    return 2;
  case 0x17: // TBA
    a = b;
    set_nz8(a);
    return 2;
  case 0x18: // XGDX (6301)
  {
    const u16 t = x;
    x = d_reg();
    set_d(t);
    return 3;
  }
  case 0x19: // DAA — stub: leave A
    set_nz8(a);
    return 2;
  case 0x1A: // SLP (63701) — treat as NOP
    return 4;
  case 0x1B: // ABA
    a = add8(a, b, false);
    return 2;

  // ---- branches ----
  case 0x20:
    return branch8(true, 3);
  case 0x21:
    return branch8(false, 3);
  case 0x22:
    return branch8(!(cc & (CC_C | CC_Z)), 3);
  case 0x23:
    return branch8((cc & (CC_C | CC_Z)) != 0, 3);
  case 0x24:
    return branch8(!(cc & CC_C), 3);
  case 0x25:
    return branch8((cc & CC_C) != 0, 3);
  case 0x26:
    return branch8(!(cc & CC_Z), 3);
  case 0x27:
    return branch8((cc & CC_Z) != 0, 3);
  case 0x28:
    return branch8(!(cc & CC_V), 3); // BVC
  case 0x29:
    return branch8((cc & CC_V) != 0, 3); // BVS
  case 0x2A:
    return branch8(!(cc & CC_N), 3); // BPL
  case 0x2B:
    return branch8((cc & CC_N) != 0, 3); // BMI
  case 0x2C: // BGE
    return branch8(!(((cc & CC_N) != 0) ^ ((cc & CC_V) != 0)), 3);
  case 0x2D: // BLT
    return branch8((((cc & CC_N) != 0) ^ ((cc & CC_V) != 0)), 3);
  case 0x2E: // BGT
    return branch8(!((cc & CC_Z) || (((cc & CC_N) != 0) ^ ((cc & CC_V) != 0))), 3);
  case 0x2F: // BLE
    return branch8((cc & CC_Z) || (((cc & CC_N) != 0) ^ ((cc & CC_V) != 0)), 3);

  // ---- stack / RTI / RTS / WAI / SWI ----
  case 0x30: // TSX
    x = u16(s + 1);
    return 3;
  case 0x31: // INS
    ++s;
    return 3;
  case 0x32: // PULA
    a = pull8();
    return 4;
  case 0x33: // PULB
    b = pull8();
    return 4;
  case 0x34: // DES
    --s;
    return 3;
  case 0x35: // TXS
    s = u16(x - 1);
    return 3;
  case 0x36: // PSHA
    push8(a);
    return 3;
  case 0x37: // PSHB
    push8(b);
    return 3;
  case 0x38: // PULX
    x = pull16();
    return 5;
  case 0x39: // RTS
    pc = pull16();
    return 5;
  case 0x3A: // ABX
    x = u16(x + b);
    return 3;
  case 0x3B: // RTI
    cc = pull8();
    b = pull8();
    a = pull8();
    x = pull16();
    pc = pull16();
    return 12;
  case 0x3C: // PSHX
    push16(x);
    return 4;
  case 0x3D: // MUL
  {
    const u16 r = u16(a) * b;
    set_d(r);
    cc &= ~CC_C;
    if (b & 0x80)
      cc |= CC_C;
    return 10;
  }
  case 0x3E: // WAI — wait for interrupt (spin with I clear path)
    // Simplified: if IRQ pending and I clear, take it next step; else burn cycles.
    if (!(cc & CC_I) && irq_line_)
      return 9;
    pc--; // re-execute WAI
    return 9;
  case 0x3F: // SWI
    push16(pc);
    push16(x);
    push8(a);
    push8(b);
    push8(cc);
    cc |= CC_I;
    pc = r16(read8(kVecSwi), read8(u16(kVecSwi + 1)));
    return 12;

  // ---- accum A/B inherent ----
  case 0x40: // NEGA
  {
    const u8 r = u8(-a);
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (r & 0x80)
      cc |= CC_N;
    if (r == 0)
      cc |= CC_Z;
    if (a == 0x80)
      cc |= CC_V;
    if (r != 0)
      cc |= CC_C;
    a = r;
    return 2;
  }
  case 0x43: // COMA
    a = u8(~a);
    set_nz8(a);
    cc |= CC_C;
    return 2;
  case 0x44: // LSRA
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (a & 1)
      cc |= CC_C;
    a >>= 1;
    set_nz8(a);
    return 2;
  case 0x46: // RORA
  {
    const u8 c = (cc & CC_C) ? 0x80 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (a & 1)
      cc |= CC_C;
    a = u8((a >> 1) | c);
    set_nz8(a);
    return 2;
  }
  case 0x47: // ASRA
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (a & 1)
      cc |= CC_C;
    a = u8((a & 0x80) | (a >> 1));
    set_nz8(a);
    return 2;
  case 0x48: // ASLA
  {
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (a & 0x80)
      cc |= CC_C;
    a = u8(a << 1);
    set_nz8(a);
    return 2;
  }
  case 0x49: // ROLA
  {
    const u8 cin = (cc & CC_C) ? 1 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (a & 0x80)
      cc |= CC_C;
    a = u8((a << 1) | cin);
    set_nz8(a);
    return 2;
  }
  case 0x4A: // DECA
    a = dec8(a);
    return 2;
  case 0x4C: // INCA
    a = inc8(a);
    return 2;
  case 0x4D: // TSTA
    set_nz8(a);
    return 2;
  case 0x4F: // CLRA
    a = 0;
    set_nz8(a);
    cc &= ~CC_C;
    return 2;

  case 0x50: // NEGB
  {
    const u8 r = u8(-b);
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (r & 0x80)
      cc |= CC_N;
    if (r == 0)
      cc |= CC_Z;
    if (b == 0x80)
      cc |= CC_V;
    if (r != 0)
      cc |= CC_C;
    b = r;
    return 2;
  }
  case 0x53: // COMB
    b = u8(~b);
    set_nz8(b);
    cc |= CC_C;
    return 2;
  case 0x54: // LSRB
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (b & 1)
      cc |= CC_C;
    b >>= 1;
    set_nz8(b);
    return 2;
  case 0x56: // RORB
  {
    const u8 c = (cc & CC_C) ? 0x80 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (b & 1)
      cc |= CC_C;
    b = u8((b >> 1) | c);
    set_nz8(b);
    return 2;
  }
  case 0x57: // ASRB
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (b & 1)
      cc |= CC_C;
    b = u8((b & 0x80) | (b >> 1));
    set_nz8(b);
    return 2;
  case 0x58: // ASLB
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (b & 0x80)
      cc |= CC_C;
    b = u8(b << 1);
    set_nz8(b);
    return 2;
  case 0x59: // ROLB
  {
    const u8 cin = (cc & CC_C) ? 1 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (b & 0x80)
      cc |= CC_C;
    b = u8((b << 1) | cin);
    set_nz8(b);
    return 2;
  }
  case 0x5A: // DECB
    b = dec8(b);
    return 2;
  case 0x5C: // INCB
    b = inc8(b);
    return 2;
  case 0x5D: // TSTB
    set_nz8(b);
    return 2;
  case 0x5F: // CLRB
    b = 0;
    set_nz8(b);
    cc &= ~CC_C;
    return 2;

  // ---- indexed RMW / control ----
  case 0x60: // NEG ,X
  {
    const u16 addr = ea_indexed();
    const u8 v = read8(addr);
    const u8 r = u8(-v);
    write8(addr, r);
    set_nz8(r);
    cc &= ~CC_C;
    if (r != 0)
      cc |= CC_C;
    return 6;
  }
  case 0x61: // AIM #imm,X (6301)
  {
    const u8 imm = fetch8();
    const u16 addr = ea_indexed();
    const u8 r = and8(read8(addr), imm);
    write8(addr, r);
    return 6;
  }
  case 0x62: // OIM #imm,X
  {
    const u8 imm = fetch8();
    const u16 addr = ea_indexed();
    const u8 r = or8(read8(addr), imm);
    write8(addr, r);
    return 6;
  }
  case 0x63: // COM ,X
  {
    const u16 addr = ea_indexed();
    const u8 r = u8(~read8(addr));
    write8(addr, r);
    set_nz8(r);
    cc |= CC_C;
    return 6;
  }
  case 0x64: // LSR ,X
  {
    const u16 addr = ea_indexed();
    u8 v = read8(addr);
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 1)
      cc |= CC_C;
    v >>= 1;
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x65: // EIM #imm,X
  {
    const u8 imm = fetch8();
    const u16 addr = ea_indexed();
    const u8 r = eor8(read8(addr), imm);
    write8(addr, r);
    return 6;
  }
  case 0x66: // ROR ,X
  {
    const u16 addr = ea_indexed();
    u8 v = read8(addr);
    const u8 cin = (cc & CC_C) ? 0x80 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 1)
      cc |= CC_C;
    v = u8((v >> 1) | cin);
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x67: // ASR ,X
  {
    const u16 addr = ea_indexed();
    u8 v = read8(addr);
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 1)
      cc |= CC_C;
    v = u8((v & 0x80) | (v >> 1));
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x68: // ASL ,X
  {
    const u16 addr = ea_indexed();
    u8 v = read8(addr);
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 0x80)
      cc |= CC_C;
    v = u8(v << 1);
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x69: // ROL ,X
  {
    const u16 addr = ea_indexed();
    u8 v = read8(addr);
    const u8 cin = (cc & CC_C) ? 1 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 0x80)
      cc |= CC_C;
    v = u8((v << 1) | cin);
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x6A: // DEC ,X
  {
    const u16 addr = ea_indexed();
    const u8 r = dec8(read8(addr));
    write8(addr, r);
    return 6;
  }
  case 0x6C: // INC ,X
  {
    const u16 addr = ea_indexed();
    const u8 r = inc8(read8(addr));
    write8(addr, r);
    return 6;
  }
  case 0x6D: // TST ,X
  {
    const u16 addr = ea_indexed();
    set_nz8(read8(addr));
    cc &= ~CC_C;
    return 6;
  }
  case 0x6E: // JMP ,X
    pc = ea_indexed();
    return 3;
  case 0x6F: // CLR ,X
  {
    const u16 addr = ea_indexed();
    write8(addr, 0);
    set_nz8(0);
    cc &= ~CC_C;
    return 6;
  }

  // ---- extended RMW / HD63701 bit ops direct ----
  case 0x70: // NEG ext
  {
    const u16 addr = ea(Mode::Extended);
    const u8 r = u8(-read8(addr));
    write8(addr, r);
    set_nz8(r);
    return 6;
  }
  case 0x71: // AIM #imm,$dir
  {
    const u8 imm = fetch8();
    const u16 addr = ea(Mode::Direct);
    const u8 r = and8(read8(addr), imm);
    write8(addr, r);
    return 6;
  }
  case 0x72: // OIM #imm,$dir (HD63701)
  {
    const u8 imm = fetch8();
    const u16 addr = ea(Mode::Direct);
    const u8 r = or8(read8(addr), imm);
    write8(addr, r);
    return 6;
  }
  case 0x73: // COM ext
  {
    const u16 addr = ea(Mode::Extended);
    const u8 r = u8(~read8(addr));
    write8(addr, r);
    set_nz8(r);
    cc |= CC_C;
    return 6;
  }
  case 0x74: // LSR ext
  {
    const u16 addr = ea(Mode::Extended);
    u8 v = read8(addr);
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 1)
      cc |= CC_C;
    v >>= 1;
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x75: // EIM #imm,$dir
  {
    const u8 imm = fetch8();
    const u16 addr = ea(Mode::Direct);
    const u8 r = eor8(read8(addr), imm);
    write8(addr, r);
    return 6;
  }
  case 0x76: // ROR ext
  {
    const u16 addr = ea(Mode::Extended);
    u8 v = read8(addr);
    const u8 cin = (cc & CC_C) ? 0x80 : 0;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (v & 1)
      cc |= CC_C;
    v = u8((v >> 1) | cin);
    write8(addr, v);
    set_nz8(v);
    return 6;
  }
  case 0x7A: // DEC ext
  {
    const u16 addr = ea(Mode::Extended);
    write8(addr, dec8(read8(addr)));
    return 6;
  }
  case 0x7B: // TIM #imm,$dir
  {
    const u8 imm = fetch8();
    const u16 addr = ea(Mode::Direct);
    set_nz8(u8(read8(addr) & imm));
    return 4;
  }
  case 0x7C: // INC ext
  {
    const u16 addr = ea(Mode::Extended);
    write8(addr, inc8(read8(addr)));
    return 6;
  }
  case 0x7D: // TST ext
  {
    const u16 addr = ea(Mode::Extended);
    set_nz8(read8(addr));
    cc &= ~CC_C;
    return 6;
  }
  case 0x7E: // JMP ext
    pc = fetch16();
    return 3;
  case 0x7F: // CLR ext
  {
    const u16 addr = ea(Mode::Extended);
    write8(addr, 0);
    set_nz8(0);
    cc &= ~CC_C;
    return 6;
  }

  // ---- A immediate / direct / indexed / extended ----
  case 0x80:
    a = sub8(a, fetch8(), false);
    return 2;
  case 0x81:
    (void)sub8(a, fetch8(), false);
    return 2;
  case 0x82:
    a = sub8(a, fetch8(), true);
    return 2;
  case 0x83: // SUBD #
  {
    const u16 imm = fetch16();
    const u32 r = u32(d_reg()) - imm;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    set_d(u16(r));
    return 4;
  }
  case 0x84:
    a = and8(a, fetch8());
    return 2;
  case 0x85: // BITA #
    set_nz8(u8(a & fetch8()));
    return 2;
  case 0x86:
    a = fetch8();
    set_nz8(a);
    return 2;
  case 0x88:
    a = eor8(a, fetch8());
    return 2;
  case 0x89:
    a = add8(a, fetch8(), true);
    return 2;
  case 0x8A:
    a = or8(a, fetch8());
    return 2;
  case 0x8B:
    a = add8(a, fetch8(), false);
    return 2;
  case 0x8C: // CPX #
  {
    const u16 imm = fetch16();
    const u32 r = u32(x) - imm;
    cc &= ~(CC_N | CC_Z | CC_V | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if ((x ^ imm) & (x ^ u16(r)) & 0x8000)
      cc |= CC_V;
    if (r & 0x10000)
      cc |= CC_C;
    return 4;
  }
  case 0x8D: // BSR
  {
    const s8 d = static_cast<s8>(fetch8());
    push16(pc);
    pc = u16(pc + d);
    return 6;
  }
  case 0x8E: // LDS #
    s = fetch16();
    set_nz16(*this, s);
    return 3;
  case 0x8F: // STS # — illegal on 6801; nop
    return 2;

  case 0x90:
    a = sub8(a, read8(ea(Mode::Direct)), false);
    return 3;
  case 0x91:
    (void)sub8(a, read8(ea(Mode::Direct)), false);
    return 3;
  case 0x92:
    a = sub8(a, read8(ea(Mode::Direct)), true);
    return 3;
  case 0x93: // SUBD dir
  {
    const u16 addr = ea(Mode::Direct);
    const u16 m = r16(read8(addr), read8(u16(addr + 1)));
    const u32 r = u32(d_reg()) - m;
    set_d(u16(r));
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 5;
  }
  case 0x94:
    a = and8(a, read8(ea(Mode::Direct)));
    return 3;
  case 0x95:
    set_nz8(u8(a & read8(ea(Mode::Direct))));
    return 3;
  case 0x96:
    a = read8(ea(Mode::Direct));
    set_nz8(a);
    return 3;
  case 0x97:
    write8(ea(Mode::Direct), a);
    set_nz8(a);
    return 3;
  case 0x98:
    a = eor8(a, read8(ea(Mode::Direct)));
    return 3;
  case 0x99:
    a = add8(a, read8(ea(Mode::Direct)), true);
    return 3;
  case 0x9A:
    a = or8(a, read8(ea(Mode::Direct)));
    return 3;
  case 0x9B:
    a = add8(a, read8(ea(Mode::Direct)), false);
    return 3;
  case 0x9C: // CPX dir
  {
    const u16 addr = ea(Mode::Direct);
    const u16 m = r16(read8(addr), read8(u16(addr + 1)));
    const u32 r = u32(x) - m;
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 5;
  }
  case 0x9D: // JSR dir
  {
    const u16 addr = ea(Mode::Direct);
    push16(pc);
    pc = addr;
    return 5;
  }
  case 0x9E: // LDS dir
  {
    const u16 addr = ea(Mode::Direct);
    s = r16(read8(addr), read8(u16(addr + 1)));
    set_nz16(*this, s);
    return 4;
  }
  case 0x9F: // STS dir
  {
    const u16 addr = ea(Mode::Direct);
    write8(addr, u8(s >> 8));
    write8(u16(addr + 1), u8(s));
    set_nz16(*this, s);
    return 4;
  }

  case 0xA0:
    a = sub8(a, read8(ea_indexed()), false);
    return 4;
  case 0xA1:
    (void)sub8(a, read8(ea_indexed()), false);
    return 4;
  case 0xA2:
    a = sub8(a, read8(ea_indexed()), true);
    return 4;
  case 0xA4:
    a = and8(a, read8(ea_indexed()));
    return 4;
  case 0xA5:
    set_nz8(u8(a & read8(ea_indexed())));
    return 4;
  case 0xA6:
    a = read8(ea_indexed());
    set_nz8(a);
    return 4;
  case 0xA7:
    write8(ea_indexed(), a);
    set_nz8(a);
    return 4;
  case 0xA8:
    a = eor8(a, read8(ea_indexed()));
    return 4;
  case 0xA9:
    a = add8(a, read8(ea_indexed()), true);
    return 4;
  case 0xAA:
    a = or8(a, read8(ea_indexed()));
    return 4;
  case 0xAB:
    a = add8(a, read8(ea_indexed()), false);
    return 4;
  case 0xAD: // JSR ,X
  {
    const u16 addr = ea_indexed();
    push16(pc);
    pc = addr;
    return 6;
  }
  case 0xAE: // LDS ,X
  {
    const u16 addr = ea_indexed();
    s = r16(read8(addr), read8(u16(addr + 1)));
    set_nz16(*this, s);
    return 5;
  }
  case 0xAF: // STS ,X
  {
    const u16 addr = ea_indexed();
    write8(addr, u8(s >> 8));
    write8(u16(addr + 1), u8(s));
    return 5;
  }

  case 0xB0:
    a = sub8(a, read8(ea(Mode::Extended)), false);
    return 4;
  case 0xB1:
    (void)sub8(a, read8(ea(Mode::Extended)), false);
    return 4;
  case 0xB2:
    a = sub8(a, read8(ea(Mode::Extended)), true);
    return 4;
  case 0xB3: // SUBD ext
  {
    const u16 addr = ea(Mode::Extended);
    const u16 m = r16(read8(addr), read8(u16(addr + 1)));
    const u32 r = u32(d_reg()) - m;
    set_d(u16(r));
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 6;
  }
  case 0xB4:
    a = and8(a, read8(ea(Mode::Extended)));
    return 4;
  case 0xB5: // BITA ext
    set_nz8(u8(a & read8(ea(Mode::Extended))));
    return 4;
  case 0xB6:
    a = read8(ea(Mode::Extended));
    set_nz8(a);
    return 4;
  case 0xB7:
    write8(ea(Mode::Extended), a);
    set_nz8(a);
    return 4;
  case 0xB8:
    a = eor8(a, read8(ea(Mode::Extended)));
    return 4;
  case 0xB9:
    a = add8(a, read8(ea(Mode::Extended)), true);
    return 4;
  case 0xBA:
    a = or8(a, read8(ea(Mode::Extended)));
    return 4;
  case 0xBB:
    a = add8(a, read8(ea(Mode::Extended)), false);
    return 4;
  case 0xBC: // CPX ext
  {
    const u16 addr = ea(Mode::Extended);
    const u16 m = r16(read8(addr), read8(u16(addr + 1)));
    const u32 r = u32(x) - m;
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 6;
  }
  case 0xBD: // JSR ext
  {
    const u16 addr = ea(Mode::Extended);
    push16(pc);
    pc = addr;
    return 6;
  }
  case 0xBE: // LDS ext
  {
    const u16 addr = ea(Mode::Extended);
    s = r16(read8(addr), read8(u16(addr + 1)));
    set_nz16(*this, s);
    return 5;
  }
  case 0xBF: // STS ext
  {
    const u16 addr = ea(Mode::Extended);
    write8(addr, u8(s >> 8));
    write8(u16(addr + 1), u8(s));
    return 5;
  }

  // ---- B / D / X ----
  case 0xC0:
    b = sub8(b, fetch8(), false);
    return 2;
  case 0xC1:
    (void)sub8(b, fetch8(), false);
    return 2;
  case 0xC2:
    b = sub8(b, fetch8(), true);
    return 2;
  case 0xC3: // ADDD #
  {
    const u16 imm = fetch16();
    const u32 r = u32(d_reg()) + imm;
    set_d(u16(r));
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 4;
  }
  case 0xC4:
    b = and8(b, fetch8());
    return 2;
  case 0xC5:
    set_nz8(u8(b & fetch8()));
    return 2;
  case 0xC6:
    b = fetch8();
    set_nz8(b);
    return 2;
  case 0xC8:
    b = eor8(b, fetch8());
    return 2;
  case 0xC9:
    b = add8(b, fetch8(), true);
    return 2;
  case 0xCA:
    b = or8(b, fetch8());
    return 2;
  case 0xCB:
    b = add8(b, fetch8(), false);
    return 2;
  case 0xCC: // LDD #
    set_d(fetch16());
    set_nz16(*this, d_reg());
    return 3;
  case 0xCD: // STD # illegal
    return 2;
  case 0xCE: // LDX #
    x = fetch16();
    set_nz16(*this, x);
    return 3;
  case 0xCF: // STX # illegal
    return 2;

  case 0xD0:
    b = sub8(b, read8(ea(Mode::Direct)), false);
    return 3;
  case 0xD1:
    (void)sub8(b, read8(ea(Mode::Direct)), false);
    return 3;
  case 0xD2:
    b = sub8(b, read8(ea(Mode::Direct)), true);
    return 3;
  case 0xD3: // ADDD dir
  {
    const u16 addr = ea(Mode::Direct);
    const u16 m = r16(read8(addr), read8(u16(addr + 1)));
    const u32 r = u32(d_reg()) + m;
    set_d(u16(r));
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 5;
  }
  case 0xD4:
    b = and8(b, read8(ea(Mode::Direct)));
    return 3;
  case 0xD5:
    set_nz8(u8(b & read8(ea(Mode::Direct))));
    return 3;
  case 0xD6:
    b = read8(ea(Mode::Direct));
    set_nz8(b);
    return 3;
  case 0xD7:
    write8(ea(Mode::Direct), b);
    set_nz8(b);
    return 3;
  case 0xD8:
    b = eor8(b, read8(ea(Mode::Direct)));
    return 3;
  case 0xD9:
    b = add8(b, read8(ea(Mode::Direct)), true);
    return 3;
  case 0xDA:
    b = or8(b, read8(ea(Mode::Direct)));
    return 3;
  case 0xDB:
    b = add8(b, read8(ea(Mode::Direct)), false);
    return 3;
  case 0xDC: // LDD dir
  {
    const u16 addr = ea(Mode::Direct);
    set_d(r16(read8(addr), read8(u16(addr + 1))));
    set_nz16(*this, d_reg());
    return 4;
  }
  case 0xDD: // STD dir
  {
    const u16 addr = ea(Mode::Direct);
    write8(addr, a);
    write8(u16(addr + 1), b);
    set_nz16(*this, d_reg());
    return 4;
  }
  case 0xDE: // LDX dir
  {
    const u16 addr = ea(Mode::Direct);
    x = r16(read8(addr), read8(u16(addr + 1)));
    set_nz16(*this, x);
    return 4;
  }
  case 0xDF: // STX dir
  {
    const u16 addr = ea(Mode::Direct);
    write8(addr, u8(x >> 8));
    write8(u16(addr + 1), u8(x));
    set_nz16(*this, x);
    return 4;
  }

  case 0xE0:
    b = sub8(b, read8(ea_indexed()), false);
    return 4;
  case 0xE1:
    (void)sub8(b, read8(ea_indexed()), false);
    return 4;
  case 0xE2:
    b = sub8(b, read8(ea_indexed()), true);
    return 4;
  case 0xE4:
    b = and8(b, read8(ea_indexed()));
    return 4;
  case 0xE5:
    set_nz8(u8(b & read8(ea_indexed())));
    return 4;
  case 0xE6:
    b = read8(ea_indexed());
    set_nz8(b);
    return 4;
  case 0xE7:
    write8(ea_indexed(), b);
    set_nz8(b);
    return 4;
  case 0xE8:
    b = eor8(b, read8(ea_indexed()));
    return 4;
  case 0xE9:
    b = add8(b, read8(ea_indexed()), true);
    return 4;
  case 0xEA:
    b = or8(b, read8(ea_indexed()));
    return 4;
  case 0xEB:
    b = add8(b, read8(ea_indexed()), false);
    return 4;
  case 0xEC: // LDD ,X
  {
    const u16 addr = ea_indexed();
    set_d(r16(read8(addr), read8(u16(addr + 1))));
    set_nz16(*this, d_reg());
    return 5;
  }
  case 0xED: // STD ,X
  {
    const u16 addr = ea_indexed();
    write8(addr, a);
    write8(u16(addr + 1), b);
    return 5;
  }
  case 0xEE: // LDX ,X
  {
    const u16 addr = ea_indexed();
    x = r16(read8(addr), read8(u16(addr + 1)));
    set_nz16(*this, x);
    return 5;
  }
  case 0xEF: // STX ,X
  {
    const u16 addr = ea_indexed();
    write8(addr, u8(x >> 8));
    write8(u16(addr + 1), u8(x));
    return 5;
  }

  case 0xF0:
    b = sub8(b, read8(ea(Mode::Extended)), false);
    return 4;
  case 0xF1:
    (void)sub8(b, read8(ea(Mode::Extended)), false);
    return 4;
  case 0xF2:
    b = sub8(b, read8(ea(Mode::Extended)), true);
    return 4;
  case 0xF3: // ADDD ext
  {
    const u16 addr = ea(Mode::Extended);
    const u16 m = r16(read8(addr), read8(u16(addr + 1)));
    const u32 r = u32(d_reg()) + m;
    set_d(u16(r));
    cc &= ~(CC_N | CC_Z | CC_C);
    if (r & 0x8000)
      cc |= CC_N;
    if ((r & 0xFFFF) == 0)
      cc |= CC_Z;
    if (r & 0x10000)
      cc |= CC_C;
    return 6;
  }
  case 0xF4:
    b = and8(b, read8(ea(Mode::Extended)));
    return 4;
  case 0xF5:
    set_nz8(u8(b & read8(ea(Mode::Extended))));
    return 4;
  case 0xF6:
    b = read8(ea(Mode::Extended));
    set_nz8(b);
    return 4;
  case 0xF7:
    write8(ea(Mode::Extended), b);
    set_nz8(b);
    return 4;
  case 0xF8:
    b = eor8(b, read8(ea(Mode::Extended)));
    return 4;
  case 0xF9:
    b = add8(b, read8(ea(Mode::Extended)), true);
    return 4;
  case 0xFA:
    b = or8(b, read8(ea(Mode::Extended)));
    return 4;
  case 0xFB:
    b = add8(b, read8(ea(Mode::Extended)), false);
    return 4;
  case 0xFC: // LDD ext
  {
    const u16 addr = ea(Mode::Extended);
    set_d(r16(read8(addr), read8(u16(addr + 1))));
    set_nz16(*this, d_reg());
    return 5;
  }
  case 0xFD: // STD ext
  {
    const u16 addr = ea(Mode::Extended);
    write8(addr, a);
    write8(u16(addr + 1), b);
    return 5;
  }
  case 0xFE: // LDX ext
  {
    const u16 addr = ea(Mode::Extended);
    x = r16(read8(addr), read8(u16(addr + 1)));
    set_nz16(*this, x);
    return 5;
  }
  case 0xFF: // STX ext
  {
    const u16 addr = ea(Mode::Extended);
    write8(addr, u8(x >> 8));
    write8(u16(addr + 1), u8(x));
    return 5;
  }

  default:
    // Illegal / unimplemented — advance 1 to avoid hang, log once in debug builds.
    return 2;
  }
}

int HD63701::step() {
  if (const int irq = service_irq()) {
    cycles_total += u64(irq);
    advance_timer(irq);
    return irq;
  }
  const u8 op = fetch8();
  const int c = exec_one(op);
  cycles_total += u64(c);
  advance_timer(c);
  return c;
}

int HD63701::run(int budget) {
  int total = 0;
  while (total + run_debt_ < budget) {
    const int c = step();
    if (c <= 0)
      break;
    total += c;
  }
  run_debt_ += total - budget;
  return total;
}

} // namespace sh
