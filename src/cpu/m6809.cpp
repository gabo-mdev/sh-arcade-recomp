
#include "cpu/m6809.hpp"

#include <array>
#include <bitset>
#include <cstring>

namespace sh {

constexpr std::array<u8, 256> kCycles1 = {{6,0,0,6,6,0,6,6,6,6,0,0,6,6,3,6,0,0,2,4,2,2,5,9,2,2,3,2,3,2,8,6,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5,2,5,3,6,20,11,2,19,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,6,6,6,6,6,6,6,6,6,6,6,6,6,6,3,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,4,7,2,2,2,4,2,2,2,2,2,2,2,2,4,7,3,2,4,4,4,6,4,4,4,4,4,4,4,4,6,7,5,5,4,4,4,6,4,4,4,4,4,4,4,4,6,7,5,5,5,5,5,7,5,5,5,5,5,5,5,5,7,8,6,6,2,2,2,4,2,2,2,2,2,2,2,2,3,2,3,3,4,4,4,6,4,4,4,4,4,4,4,4,5,5,5,5,4,4,4,6,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,7,5,5,5,5,5,5,5,5,6,6,6,6}};
constexpr std::array<u8, 256> kCycles2 = {{5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,2,2,8,8,8,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5}};
constexpr std::array<u8, 256> kCycles3 = {{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,38,2,11,7,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,19,20,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,5,2,2,2,2,2,2,2,2,20,5,4,5,2,2,2,2,2,2,2,2,2,2,2,2,20,5,4,5,2,2,2,2,2,2,2,2,2,2,2,2,20,5,4,5,2,2,2,2,2,2,2,2,2,2,2,2,20,5,4,5,2,2,2,2,2,2,6,14,10,6,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2}};
constexpr std::array<u8, 256> kIndexCycles = {{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,3,2,3,0,1,1,0,1,4,0,4,1,5,0,5,5,6,5,6,3,4,4,0,4,7,0,7,4,8,0,8,2,3,2,3,0,1,1,0,1,4,0,4,1,5,0,5,5,6,5,6,3,4,4,0,4,7,0,7,4,8,0,8,2,3,2,3,0,1,1,0,1,4,0,4,1,5,0,5,5,6,5,6,3,4,4,0,4,7,0,7,4,8,0,8,2,3,2,3,0,1,1,0,1,4,0,4,1,5,0,5,4,6,5,6,3,4,4,0,4,7,0,7,4,8,0,8}};

inline s8 sex8(u8 v) { return static_cast<s8>(v); }
inline u16 r16(u8 hi, u8 lo) { return (static_cast<u16>(hi) << 8) | lo; }

struct M6809::M6809Core {
  M6809& m;
  int cyc = 0;
  u8 idx_post = 0;
  int idx_extra = 0;

  explicit M6809Core(M6809& cpu) : m(cpu) {}

  u8 rd(u16 a) const { return m.read_ ? m.read_(a) : 0; }
  void wr(u16 a, u8 v) const { if (m.write_) m.write_(a, v); }

  u8 fetch() {
    u8 v = rd(m.pc++);
    return v;
  }
  u16 fetch16() {
    // 6809 is big-endian: high byte first.
    u8 hi = fetch();
    u8 lo = fetch();
    return r16(hi, lo);
  }

  // 6809 stack: pre-decrement on push, post-increment on pull.
  // After push, S points at the last stored byte (MSB of a 16-bit value).
  void push8(u8 v) { wr(--m.s, v); }
  void push16(u16 v) {
    push8(static_cast<u8>(v));        // LSB at higher address
    push8(static_cast<u8>(v >> 8)); // MSB at S
  }
  u8 pull8() {
    const u8 v = rd(m.s);
    ++m.s;
    return v;
  }
  u16 pull16() {
    u8 hi = pull8();
    u8 lo = pull8();
    return r16(hi, lo);
  }

  u16 direct_ea() { return r16(m.dp, fetch()); }
  u16 extended_ea() { return fetch16(); }

  u16& ireg(u8 rr) {
    switch (rr & 0x60) {
      case 0x00: return m.x;
      case 0x20: return m.y;
      case 0x40: return m.u;
      default: return m.s;
    }
  }

  u16 indexed_ea() {
    idx_post = fetch();
    idx_extra = kIndexCycles[idx_post];
    u16 base = 0;
    const u8 rr = idx_post;

    if (rr & 0x80) {
      // Complex form: bits6-5=reg, bit4=indirect, bits3-0=mode.
      // Mode must be masked with 0x0F — including bit4 in the switch
      // (old & 0x7F) made [,R++] (0x91) fall into default EA=0.
      const u8 mode = rr & 0x0F;
      u16& r = ireg(rr);
      switch (mode) {
      case 0x00:
        base = r;
        ++r;
        break;
      case 0x01:
        base = r;
        r += 2;
        break;
      case 0x02:
        --r;
        base = r;
        break;
      case 0x03:
        r -= 2;
        base = r;
        break;
      case 0x04:
        base = r;
        break;
      case 0x05:
        base = static_cast<u16>(static_cast<s16>(r) + static_cast<s8>(m.b));
        break;
      case 0x06:
        base = static_cast<u16>(static_cast<s16>(r) + static_cast<s8>(m.a));
        break;
      case 0x08:
        base = static_cast<u16>(static_cast<s16>(r) + sex8(fetch()));
        break;
      case 0x09:
        base = static_cast<u16>(static_cast<s16>(r) + static_cast<s16>(fetch16()));
        break;
      case 0x0B:
        base = static_cast<u16>(static_cast<s16>(r) + static_cast<s16>(regD()));
        break;
      case 0x0C:
        base = static_cast<u16>(m.pc + sex8(fetch()));
        break;
      case 0x0D:
        base = static_cast<u16>(m.pc + static_cast<s16>(fetch16()));
        break;
      case 0x0F:
        base = fetch16();
        break;
      default:
        base = 0;
        break;
      }
      if (idx_post & 0x10) {
        const u16 ind = r16(rd(base), rd(base + 1));
        base = ind;
      }
      return base;
    }

    // 5-bit signed offset (bit7=0): bits 6-5 select register, bits 4-0 are
    // the offset — must sign-extend from 5 bits, not from the whole postbyte.
    {
      const u16 reg = (rr & 0x60) == 0x00   ? m.x
                      : (rr & 0x60) == 0x20 ? m.y
                      : (rr & 0x60) == 0x40 ? m.u
                                           : m.s;
      const int off5 = (rr & 0x10) ? int(rr & 0x0F) - 16 : int(rr & 0x0F);
      base = static_cast<u16>(static_cast<s16>(reg) + off5);
    }
    return base;
  }

  void set_nz8(u8 r) {
    m.cc &= ~(M6809::CC_N | M6809::CC_Z);
    if (r & 0x80) m.cc |= M6809::CC_N;
    if (r == 0) m.cc |= M6809::CC_Z;
  }
  void set_nz16(u16 r) {
    m.cc &= ~(M6809::CC_N | M6809::CC_Z);
    if (r & 0x8000) m.cc |= M6809::CC_N;
    if (r == 0) m.cc |= M6809::CC_Z;
  }

  u8 flags8(u8 a, u8 b, u16 r, u8 mask) {
    m.cc &= ~mask;
    if ((mask & M6809::CC_H) && ((a ^ b ^ static_cast<u8>(r)) & 0x10))
      m.cc |= M6809::CC_H;
    if ((mask & M6809::CC_N) && (r & 0x80))
      m.cc |= M6809::CC_N;
    if ((mask & M6809::CC_Z) && (static_cast<u8>(r) == 0))
      m.cc |= M6809::CC_Z;
    if ((mask & M6809::CC_V) && ((a ^ b ^ static_cast<u8>(r) ^ (r >> 1)) & 0x80))
      m.cc |= M6809::CC_V;
    if ((mask & M6809::CC_C) && (r & 0x100))
      m.cc |= M6809::CC_C;
    return static_cast<u8>(r);
  }

  u16 flags16(u16 a, u16 b, u32 r, u8 mask) {
    m.cc &= ~mask;
    if ((mask & M6809::CC_N) && (r & 0x8000))
      m.cc |= M6809::CC_N;
    if ((mask & M6809::CC_Z) && ((r & 0xFFFF) == 0))
      m.cc |= M6809::CC_Z;
    if ((mask & M6809::CC_V) && ((a ^ b ^ static_cast<u16>(r) ^ (r >> 1)) & 0x8000))
      m.cc |= M6809::CC_V;
    if ((mask & M6809::CC_C) && (r & 0x10000))
      m.cc |= M6809::CC_C;
    return static_cast<u16>(r);
  }

  void mem_neg(u16 ea) {
    u8 v = rd(ea);
    u8 r = flags8(0, v, static_cast<u16>(-static_cast<s8>(v)), M6809::CC_NZVC);
    wr(ea, r);
  }
  void mem_com(u16 ea) {
    u8 v = rd(ea);
    m.cc &= ~M6809::CC_V;
    m.cc |= M6809::CC_C;
    v = static_cast<u8>(~v);
    set_nz8(v);
    wr(ea, v);
  }
  void mem_lsr(u16 ea) {
    u8 v = rd(ea);
    m.cc &= ~(M6809::CC_N | M6809::CC_V);
    if (v & 1) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    v >>= 1;
    set_nz8(v);
    wr(ea, v);
  }
  void mem_ror(u16 ea) {
    u8 v = rd(ea);
    u8 c = (m.cc & M6809::CC_C) ? 0x80 : 0;
    m.cc &= ~(M6809::CC_N | M6809::CC_V);
    if (v & 1) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    v = (v >> 1) | c;
    set_nz8(v);
    wr(ea, v);
  }
  void mem_asr(u16 ea) {
    u8 v = rd(ea);
    m.cc &= ~M6809::CC_V;
    if (v & 1) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    u8 msb = v & 0x80;
    v = (v >> 1) | msb;
    set_nz8(v);
    wr(ea, v);
  }
  void mem_asl(u16 ea) {
    u8 v = rd(ea);
    m.cc &= ~M6809::CC_V;
    if (v & 0x80) m.cc |= M6809::CC_V;
    if (v & 0x80) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    v <<= 1;
    set_nz8(v);
    wr(ea, v);
  }
  void mem_rol(u16 ea) {
    u8 v = rd(ea);
    u8 c = (m.cc & M6809::CC_C) ? 1 : 0;
    m.cc &= ~M6809::CC_V;
    if ((v ^ (v << 1)) & 0x80) m.cc |= M6809::CC_V;
    u16 t = static_cast<u16>(v << 1) | c;
    if (v & 0x80) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    v = static_cast<u8>(t);
    set_nz8(v);
    wr(ea, v);
  }
  void mem_dec(u16 ea) {
    u8 v = rd(ea);
    v = flags8(v, 1, static_cast<u16>(v - 1), M6809::CC_NZV);
    wr(ea, v);
  }
  void mem_inc(u16 ea) {
    u8 v = rd(ea);
    v = flags8(v, 1, static_cast<u16>(v + 1), M6809::CC_NZV);
    wr(ea, v);
  }
  void mem_tst(u16 ea) {
    u8 v = rd(ea);
    set_nz8(v);
    m.cc &= ~M6809::CC_C;
  }
  void mem_clr(u16 ea) {
    m.cc &= ~(M6809::CC_N | M6809::CC_V | M6809::CC_C);
    m.cc |= M6809::CC_Z;
    wr(ea, 0);
  }

  u8 reg_neg8(u8& r) {
    r = flags8(0, r, static_cast<u16>(-static_cast<s8>(r)), M6809::CC_NZVC);
    return r;
  }
  u8 reg_com8(u8& r) {
    m.cc &= ~M6809::CC_V;
    m.cc |= M6809::CC_C;
    r = static_cast<u8>(~r);
    set_nz8(r);
    return r;
  }
  void reg_lsr8(u8& r) {
    m.cc &= ~(M6809::CC_N | M6809::CC_V);
    if (r & 1) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    r >>= 1;
    set_nz8(r);
  }
  void reg_ror8(u8& r) {
    u8 c = (m.cc & M6809::CC_C) ? 0x80 : 0;
    m.cc &= ~(M6809::CC_N | M6809::CC_V);
    if (r & 1) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    r = (r >> 1) | c;
    set_nz8(r);
  }
  void reg_asr8(u8& r) {
    m.cc &= ~M6809::CC_V;
    if (r & 1) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    u8 msb = r & 0x80;
    r = (r >> 1) | msb;
    set_nz8(r);
  }
  void reg_asl8(u8& r) {
    m.cc &= ~M6809::CC_V;
    if (r & 0x80) m.cc |= M6809::CC_V;
    if (r & 0x80) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    r <<= 1;
    set_nz8(r);
  }
  void reg_rol8(u8& r) {
    u8 c = (m.cc & M6809::CC_C) ? 1 : 0;
    m.cc &= ~M6809::CC_V;
    if ((r ^ (r << 1)) & 0x80) m.cc |= M6809::CC_V;
    u16 t = static_cast<u16>(r << 1) | c;
    if (r & 0x80) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
    r = static_cast<u8>(t);
    set_nz8(r);
  }
  u8 reg_dec8(u8& r) {
    r = flags8(r, 1, static_cast<u16>(r - 1), M6809::CC_NZV);
    return r;
  }
  u8 reg_inc8(u8& r) {
    r = flags8(r, 1, static_cast<u16>(r + 1), M6809::CC_NZV);
    return r;
  }
  void reg_tst8(u8 r) {
    set_nz8(r);
    m.cc &= ~M6809::CC_C;
  }
  u8 reg_clr8(u8& r) {
    m.cc &= ~(M6809::CC_N | M6809::CC_V | M6809::CC_C);
    m.cc |= M6809::CC_Z;
    r = 0;
    return r;
  }

  enum MemOp : u8 {
    NEG, COM, LSR, ROR, ASR, ASL, ROL, DEC, INC, TST, IL, JMP, CLR
  };

  void mem_op(u16 ea, MemOp op) {
    switch (op) {
      case NEG: mem_neg(ea); break;
      case COM: mem_com(ea); break;
      case LSR: mem_lsr(ea); break;
      case ROR: mem_ror(ea); break;
      case ASR: mem_asr(ea); break;
      case ASL: mem_asl(ea); break;
      case ROL: mem_rol(ea); break;
      case DEC: mem_dec(ea); break;
      case INC: mem_inc(ea); break;
      case TST: mem_tst(ea); break;
      case CLR: mem_clr(ea); break;
      case JMP: m.pc = ea; break;
      default: break;
    }
  }

  void reg_op(u8& r, MemOp op) {
    switch (op) {
      case NEG: reg_neg8(r); break;
      case COM: reg_com8(r); break;
      case LSR: reg_lsr8(r); break;
      case ROR: reg_ror8(r); break;
      case ASR: reg_asr8(r); break;
      case ASL: reg_asl8(r); break;
      case ROL: reg_rol8(r); break;
      case DEC: reg_dec8(r); break;
      case INC: reg_inc8(r); break;
      case TST: reg_tst8(r); break;
      case CLR: reg_clr8(r); break;
      default: break;
    }
  }

  static MemOp lo_mem(u8 lo) {
    switch (lo) {
      case 0: case 1: return NEG;
      case 2: return INC; // illegal treated as inc for 0x02
      case 3: return COM;
      case 4: case 5: return LSR;
      case 6: return ROR;
      case 7: return ASR;
      case 8: return ASL;
      case 9: return ROL;
      case 0x0A: return DEC;
      case 0x0B: return DEC;
      case 0x0C: return INC;
      case 0x0D: return TST;
      case 0x0E: return JMP;
      case 0x0F: return CLR;
      default: return IL;
    }
  }

  enum AluOp : u8 {
    SUB, CMP, SBC, SUB16, AND, BIT, LD, ST, EOR, ADC, OR, ADD,
    LD16, ST16, JSR, ADDD
  };

  // 6809 lo-nibble meaning differs for groups 8-B vs C-F.
  static AluOp lo_alu(u8 grp, u8 lo) {
    lo &= 0x0F;
    if (grp >= 0xC) {
      static constexpr AluOp t[] = {
          SUB, CMP, SBC, ADDD, AND, BIT, LD, ST,
          EOR, ADC, OR, ADD, LD16, ST16, LD16, ST16};
      return t[lo];
    }
    static constexpr AluOp t[] = {
        SUB, CMP, SBC, SUB16, AND, BIT, LD, ST,
        EOR, ADC, OR, ADD, CMP, JSR, LD16, ST16};
    return t[lo];
  }

  u16 regD() const { return static_cast<u16>((m.a << 8) | m.b); }
  void setD(u16 v) {
    m.a = static_cast<u8>(v >> 8);
    m.b = static_cast<u8>(v);
  }

  // True when the 16-bit op targets D (SUBD/ADDD/LDD/STD/CMPD).
  bool is_d_op(AluOp aop, bool page2, u8 op) const {
    const u8 lo = op & 0x0F;
    const u8 grp = op >> 4;
    if (page2)
      return lo == 0x3; // CMPD / etc. on page 2 uses x3
    if (aop == SUB16 || aop == ADDD)
      return true;
    if (grp >= 0xC && (aop == LD16 || aop == ST16) && lo == 0xC)
      return true; // LDD
    if (grp >= 0xC && aop == ST16 && lo == 0xD)
      return true; // STD
    return false;
  }

  u16& dst16(int page, u8 op) {
    const u8 grp = op >> 4;
    const u8 lo = op & 0x0F;
    if (page == 3) {
      // Prefix 11: x3 = CMPU, xC = CMPS.
      if (lo == 0x0C)
        return m.s;
      return m.u;
    }
    if (page == 2) {
      // Prefix 10: 8E/8F LDY/STY, CE/CF LDS/STS. CMPD is is_d_op.
      if (grp >= 0xC)
        return m.s;
      return m.y;
    }
    // page 0: xE/xF in 8-B = LDX/STX; in C-F = LDU/STU
    if (grp >= 0xC)
      return m.u;
    return m.x;
  }

  void alu16_d(u16 src, AluOp op) {
    u16 d = regD();
    alu16(d, src, op);
    setD(d);
  }

  void st16_reg(u16& reg, u16 ea) {
    wr(ea, static_cast<u8>(reg >> 8));
    wr(ea + 1, static_cast<u8>(reg));
  }

  void alu8(u8& dst, u8 src, AluOp op, u16 ea = 0) {
    switch (op) {
      case SUB:
        dst = flags8(dst, src, static_cast<u16>(dst - src), M6809::CC_NZVC);
        break;
      case CMP:
        (void)flags8(dst, src, static_cast<u16>(dst - src), M6809::CC_NZVC);
        break;
      case SBC:
        dst = flags8(dst, src, static_cast<u16>(dst - src - ((m.cc & M6809::CC_C) ? 1 : 0)),
                     M6809::CC_NZVC);
        break;
      case AND:
        dst = dst & src;
        m.cc &= ~M6809::CC_V;
        set_nz8(dst);
        break;
      case BIT:
        set_nz8(static_cast<u8>(dst & src));
        m.cc &= ~M6809::CC_V;
        break;
      case LD:
        dst = src;
        set_nz8(dst);
        m.cc &= ~M6809::CC_V;
        break;
      case ST:
        wr(ea, src);
        break;
      case EOR:
        dst ^= src;
        set_nz8(dst);
        m.cc &= ~M6809::CC_V;
        break;
      case ADC:
        // ADCA/ADCB set H. DAA uses it; without H a BCD score or life byte
        // that carries out of the low nibble stays uncorrected.
        dst = flags8(dst, src, static_cast<u16>(dst + src + ((m.cc & M6809::CC_C) ? 1 : 0)),
                    M6809::CC_H | M6809::CC_NZVC);
        break;
      case OR:
        dst |= src;
        set_nz8(dst);
        m.cc &= ~M6809::CC_V;
        break;
      case ADD:
        dst = flags8(dst, src, static_cast<u16>(dst + src), M6809::CC_H | M6809::CC_NZVC);
        break;
      default:
        break;
    }
  }

  void alu16(u16& dst, u16 src, AluOp op, u16 ea = 0) {
    switch (op) {
      case SUB16:
        dst = flags16(dst, src, static_cast<u32>(dst) - src, M6809::CC_NZVC);
        break;
      case ADDD:
      case ADD:
        dst = flags16(dst, src, static_cast<u32>(dst) + src, M6809::CC_NZVC);
        break;
      case CMP:
        (void)flags16(dst, src, static_cast<u32>(dst) - src, M6809::CC_NZVC);
        break;
      case LD16:
        dst = src;
        set_nz16(dst);
        m.cc &= ~M6809::CC_V;
        break;
      case ST16:
        wr(ea, static_cast<u8>(src >> 8));
        wr(ea + 1, static_cast<u8>(src));
        set_nz16(src);
        m.cc &= ~M6809::CC_V;
        break;
      case JSR:
        push16(m.pc);
        m.pc = ea;
        break;
      default:
        break;
    }
  }

  bool branch_cond(u8 op) {
    switch (op) {
      case 0x20: return true;
      case 0x21: return false;
      case 0x22: return (m.cc & (M6809::CC_Z | M6809::CC_C)) == 0;
      case 0x23: return (m.cc & (M6809::CC_Z | M6809::CC_C)) != 0;
      case 0x24: return (m.cc & M6809::CC_C) == 0;
      case 0x25: return (m.cc & M6809::CC_C) != 0;
      case 0x26: return (m.cc & M6809::CC_Z) == 0;
      case 0x27: return (m.cc & M6809::CC_Z) != 0;
      case 0x28: return (m.cc & M6809::CC_V) == 0;
      case 0x29: return (m.cc & M6809::CC_V) != 0;
      case 0x2A: return (m.cc & M6809::CC_N) == 0;
      case 0x2B: return (m.cc & M6809::CC_N) != 0;
      case 0x2C: return ((m.cc & (M6809::CC_N | M6809::CC_V)) == 0) ||
                       ((m.cc & (M6809::CC_N | M6809::CC_V)) == (M6809::CC_N | M6809::CC_V));
      case 0x2D: return ((m.cc & (M6809::CC_N | M6809::CC_V)) == M6809::CC_N) ||
                       ((m.cc & (M6809::CC_N | M6809::CC_V)) == M6809::CC_V);
      case 0x2E: return ((m.cc & M6809::CC_Z) == 0) &&
                       (((m.cc & (M6809::CC_N | M6809::CC_V)) == 0) ||
                        ((m.cc & (M6809::CC_N | M6809::CC_V)) == (M6809::CC_N | M6809::CC_V)));
      case 0x2F: return ((m.cc & M6809::CC_Z) == 0) &&
                       (((m.cc & (M6809::CC_N | M6809::CC_V)) == M6809::CC_N) ||
                        ((m.cc & (M6809::CC_N | M6809::CC_V)) == M6809::CC_V));
      default: return false;
    }
  }

  void push_regs(u16 mask, u16& sp) {
    auto p8 = [&](u8 v) { wr(--sp, v); };
    auto p16 = [&](u16 v) {
      p8(static_cast<u8>(v));
      p8(static_cast<u8>(v >> 8));
    };
    if (mask & 0x80) p16(m.pc);
    if (mask & 0x40) p16(sp == m.s ? m.u : m.s); // simplified
    if (mask & 0x20) p16(m.y);
    if (mask & 0x10) p16(m.x);
    if (mask & 0x08) p8(m.dp);
    if (mask & 0x04) p8(m.b);
    if (mask & 0x02) p8(m.a);
    if (mask & 0x01) p8(m.cc);
  }

  void pull_regs(u16 mask, u16& sp) {
    // Match pull8: read at S, then post-increment (S points at last pushed byte).
    auto g8 = [&]() -> u8 {
      const u8 v = rd(sp);
      ++sp;
      return v;
    };
    auto g16 = [&]() -> u16 {
      u8 hi = g8();
      u8 lo = g8();
      return r16(hi, lo);
    };
    if (mask & 0x01) m.cc = g8();
    const bool entire = (m.cc & M6809::CC_E) != 0;
    if (entire) {
      if (mask & 0x02) m.a = g8();
      if (mask & 0x04) m.b = g8();
      if (mask & 0x08) m.dp = g8();
      if (mask & 0x10) m.x = g16();
      if (mask & 0x20) m.y = g16();
      if (mask & 0x40) {
        if (&sp == &m.s) m.u = g16(); else m.s = g16();
      }
    }
    if (mask & 0x80) m.pc = g16();
  }

  int take_vector(u16 vec, u16 mask, bool set_i, bool set_f, bool set_e) {
    cyc += 4;
    // E must be correct in the stacked CC so RTI restores the full register set.
    if (set_e)
      m.cc |= M6809::CC_E;
    else
      m.cc &= ~M6809::CC_E;
    push_regs(mask, m.s);
    if (set_i)
      m.cc |= M6809::CC_I;
    if (set_f)
      m.cc |= M6809::CC_F;
    m.pc = r16(rd(vec), rd(vec + 1));
    cyc += 5;
    return cyc;
  }

  int service_int() {
    if (m.nmi_pending_) {
      m.nmi_pending_ = false;
      return take_vector(0xFFFC, 0xFF, true, true, true);
    }
    if (!(m.cc & M6809::CC_F) && m.firq_line_) {
      // FIRQ stacks only PC+CC with E clear.
      return take_vector(0xFFF6, 0x81, true, true, false);
    }
    if (!(m.cc & M6809::CC_I) && m.irq_line_) {
      return take_vector(0xFFF8, 0xFF, true, false, true);
    }
    return 0;
  }

  u16 reg_get(u8 code) {
    switch (code & 0x0F) {
      case 0: return regD();
      case 1: return m.x;
      case 2: return m.y;
      case 3: return m.u;
      case 4: return m.s;
      case 5: return m.pc;
      case 8: return m.a | 0xFF00;
      case 9: return m.b | 0xFF00;
      case 10: return m.cc | (static_cast<u16>(m.cc) << 8);
      case 11: return m.dp | (static_cast<u16>(m.dp) << 8);
      default: return 0xFFFF;
    }
  }

  void reg_put(u8 code, u16 v) {
    switch (code & 0x0F) {
      case 0: setD(v); break;
      case 1: m.x = v; break;
      case 2: m.y = v; break;
      case 3: m.u = v; break;
      case 4: m.s = v; break;
      case 5: m.pc = v; break;
      case 8: m.a = static_cast<u8>(v); break;
      case 9: m.b = static_cast<u8>(v); break;
      case 10: m.cc = static_cast<u8>(v); break;
      case 11: m.dp = static_cast<u8>(v); break;
      default: break;
    }
  }

  void daa() {
    u16 t = m.a;
    u8 cf = 0;
    u8 lsn = m.a & 0x0F;
    u8 msn = m.a & 0xF0;
    if (lsn > 9 || (m.cc & M6809::CC_H)) cf |= 0x06;
    if (msn > 0x80 && lsn > 9) cf |= 0x60;
    if (msn > 0x90 || (m.cc & M6809::CC_C)) cf |= 0x60;
    t = m.a + cf;
    m.cc &= ~M6809::CC_V;
    if (t & 0x100) m.cc |= M6809::CC_C;
    m.a = static_cast<u8>(t);
    set_nz8(m.a);
  }

  void mul() {
    u16 r = static_cast<u16>(m.a) * static_cast<u16>(m.b);
    setD(r);
    set_nz16(r);
    if (m.b & 0x80) m.cc |= M6809::CC_C; else m.cc &= ~M6809::CC_C;
  }

  int stack_push(u8 post, bool is_s) {
    u16& sp = is_s ? m.s : m.u;
    auto ps = [&](u8 v){ wr(--sp, v); };
    auto ps16 = [&](u16 v){ ps(static_cast<u8>(v)); ps(static_cast<u8>(v>>8)); };
    if (post & 0x80) ps16(m.pc);
    if (post & 0x40) { if (is_s) ps16(m.u); else ps16(m.s); }
    if (post & 0x20) ps16(m.y);
    if (post & 0x10) ps16(m.x);
    if (post & 0x08) ps(m.dp);
    if (post & 0x04) ps(m.b);
    if (post & 0x02) ps(m.a);
    if (post & 0x01) ps(m.cc);
    cyc += kCycles2[post & 0x3F ? 0x34 : 0x34];
    return cyc;
  }

  int stack_pull(u8 post, bool is_s) {
    u16& sp = is_s ? m.s : m.u;
    if (post & 0x01) m.cc = pull8();
    if (post & 0x02) m.a = pull8();
    if (post & 0x04) m.b = pull8();
    if (post & 0x08) m.dp = pull8();
    if (post & 0x10) m.x = pull16();
    if (post & 0x20) m.y = pull16();
    if (post & 0x40) {
      if (is_s) m.u = pull16(); else m.s = pull16();
    }
    if (post & 0x80) m.pc = pull16();
    (void)rd(sp + 1);
    cyc += 5 + static_cast<int>(std::bitset<8>(post).count()) * 2;
    return cyc;
  }

  int exec_alu_range(u8 op, int page, u8 subop) {
    const u8 raw = page ? subop : op;
    const u8 grp = raw >> 4;
    const u8 lo = raw & 0x0F;
    AluOp aop = lo_alu(grp, lo);
    // Page 0 lo==3 is SUBD. Prefix 10/11 reuse that slot as CMPD / CMPU.
    if (page != 0 && lo == 0x3 && aop == SUB16)
      aop = CMP;
    u8& r8 = (grp < 0xC) ? m.a : m.b;

    u16 ea = 0;
    idx_extra = 0;

    auto do16_imm = [&]() {
      const u16 v = fetch16();
      if (is_d_op(aop, page == 2, raw)) {
        if (aop == SUB16)
          alu16_d(v, SUB16);
        else if (aop == ADDD)
          alu16_d(v, ADDD);
        else if (aop == CMP)
          alu16_d(v, CMP);
        else {
          setD(v);
          set_nz16(v);
          m.cc &= ~M6809::CC_V;
        }
      } else if (aop == CMP) {
        alu16(dst16(page, raw), v, CMP);
      } else {
        alu16(dst16(page, raw), v, LD16);
      }
    };

    auto do16_ea = [&](u16 addr) {
      if (aop == ST16) {
        const u16 v = is_d_op(aop, page == 2, raw) ? regD() : dst16(page, raw);
        wr(addr, static_cast<u8>(v >> 8));
        wr(addr + 1, static_cast<u8>(v));
        set_nz16(v);
        m.cc &= ~M6809::CC_V;
        return;
      }
      if (aop == JSR) {
        push16(m.pc);
        m.pc = addr;
        return;
      }
      const u16 v = r16(rd(addr), rd(addr + 1));
      if (is_d_op(aop, page == 2, raw)) {
        if (aop == SUB16)
          alu16_d(v, SUB16);
        else if (aop == ADDD)
          alu16_d(v, ADDD);
        else if (aop == CMP)
          alu16_d(v, CMP);
        else {
          setD(v);
          set_nz16(v);
          m.cc &= ~M6809::CC_V;
        }
      } else if (aop == CMP) {
        alu16(dst16(page, raw), v, CMP);
      } else {
        alu16(dst16(page, raw), v, LD16);
      }
    };

    const bool wide = (aop == SUB16 || aop == ADDD || aop == LD16 || aop == ST16 ||
                       aop == JSR || (aop == CMP && (lo == 0xC || page != 0)));

    if (grp == 0x8 || grp == 0xC) {
      if (wide) {
        if (aop == JSR) {
          // 0x8D BSR: relative call (not an immediate 16-bit EA).
          const s8 d = sex8(fetch());
          push16(m.pc);
          m.pc = static_cast<u16>(m.pc + d);
        } else if (aop == ST16) {
          // no immediate store
        } else {
          do16_imm();
        }
      } else {
        const u8 imm = fetch();
        alu8(r8, imm, aop);
      }
    } else if (grp == 0x9 || grp == 0xD) {
      ea = direct_ea();
      if (wide)
        do16_ea(ea);
      else if (aop == ST)
        alu8(r8, r8, ST, ea);
      else
        alu8(r8, rd(ea), aop);
    } else if (grp == 0xA || grp == 0xE) {
      ea = indexed_ea();
      if (wide)
        do16_ea(ea);
      else if (aop == ST)
        alu8(r8, r8, ST, ea);
      else
        alu8(r8, rd(ea), aop);
    } else if (grp == 0xB || grp == 0xF) {
      ea = extended_ea();
      if (wide)
        do16_ea(ea);
      else if (aop == ST)
        alu8(r8, r8, ST, ea);
      else
        alu8(r8, rd(ea), aop);
    }

    u8 cy = page ? kCycles2[subop] : kCycles1[op];
    if (grp == 0xA || grp == 0xE)
      cy = static_cast<u8>(cy + idx_extra);
    if (page)
      cy = static_cast<u8>(cy + kCycles3[subop]);
    return cy;
  }

  int exec_page2(u8 sub) {
    // Nested page prefixes: the sub-opcode was already fetched by the caller.
    if (sub == 0x10)
      return exec_page2(fetch());
    if (sub == 0x11)
      return exec_page3(fetch());
    if (sub >= 0x20 && sub <= 0x2F) {
      bool take = branch_cond(sub);
      s16 disp = static_cast<s16>(fetch16());
      if (take) m.pc = static_cast<u16>(m.pc + disp);
      return kCycles2[sub] + (take ? kCycles3[sub] : 0);
    }
    if (sub == 0x3F) {
      m.cc |= M6809::CC_E;
      push_regs(0xFF, m.s);
      m.cc |= M6809::CC_I | M6809::CC_F;
      m.pc = r16(rd(0xFFF4), rd(0xFFF5));
      return 20;
    }
    if (sub == 0x3E) {
      m.cc |= M6809::CC_E;
      push_regs(0xFF, m.s);
      m.cc |= M6809::CC_I | M6809::CC_F;
      m.pc = r16(rd(0xFFF4), rd(0xFFF5));
      return 20;
    }
    u8 mapped = sub;
    return exec_alu_range(mapped, 2, sub);
  }

  int exec_page3(u8 sub) {
    if (sub == 0x10)
      return exec_page3(fetch());
    if (sub == 0x11)
      return exec_page2(fetch());
    if (sub == 0x3F) {
      m.cc |= M6809::CC_E;
      push_regs(0xFF, m.s);
      m.cc |= M6809::CC_I | M6809::CC_F;
      m.pc = r16(rd(0xFFF2), rd(0xFFF3));
      return 20;
    }
    if (sub == 0x3E) {
      m.cc |= M6809::CC_E;
      push_regs(0xFF, m.s);
      m.cc |= M6809::CC_I | M6809::CC_F;
      m.pc = r16(rd(0xFFF6), rd(0xFFF7));
      return 20;
    }
    if ((sub & 0x80) == 0)
      return exec_one(0x11); // re-dispatch as page1 after prefix — simplified
    return exec_alu_range(sub, 3, sub);
  }

  int exec_one(u8 op) {
    idx_extra = 0;
    u8 hi = op >> 4;
    u8 lo = op & 0x0F;

    // 0x10 / 0x11 are page prefixes — fetch the sub-opcode once (do NOT discard a byte).
    if (op == 0x10)
      return 1 + exec_page2(fetch());
    if (op == 0x11)
      return 1 + exec_page3(fetch());

    if (hi < 4) {
      MemOp mop = lo_mem(lo);
      if (hi == 0) {
        u16 ea = direct_ea();
        mem_op(ea, mop);
        return kCycles1[op];
      }
      if (hi == 1) {
        // 0x10-0x1F special
      }
      if (hi == 2) {
        bool take = branch_cond(op);
        s8 d = sex8(fetch());
        if (take) m.pc = static_cast<u16>(m.pc + d);
        return kCycles1[op] + (take ? 1 : 0);
      }
      if (hi == 3) {
        switch (op) {
          case 0x30: case 0x31: case 0x32: case 0x33: {
            u16 ea = indexed_ea();
            u16& r = (op == 0x30) ? m.x : (op == 0x31) ? m.y : (op == 0x32) ? m.s : m.u;
            r = ea;
            // LEAX/LEAY set Z from the result; LEAS/LEAU do not affect flags.
            if (op == 0x30 || op == 0x31) {
              m.cc &= ~M6809::CC_Z;
              if (r == 0)
                m.cc |= M6809::CC_Z;
            }
            return static_cast<u8>(kCycles1[op] + idx_extra);
          }
          case 0x34: return stack_push(fetch(), true);
          case 0x35: return stack_pull(fetch(), true);
          case 0x36: return stack_push(fetch(), false);
          case 0x37: return stack_pull(fetch(), false);
          case 0x38: m.cc &= ~fetch(); return kCycles1[op];
          case 0x39: m.pc = pull16(); return kCycles1[op];
          case 0x3A: m.x = static_cast<u16>(m.x + m.b); return kCycles1[op];
          case 0x3B: {
            pull_regs(0xFF, m.s);
            return kCycles1[op];
          }
          case 0x3C: {
            u8 msk = fetch();
            m.cc &= msk;
            while (!service_int()) { /* wait */ break; }
            return kCycles2[0x3C] + 19;
          }
          case 0x3D: mul(); return kCycles1[op];
          case 0x3F: {
            m.cc |= M6809::CC_E;
            push_regs(0xFF, m.s);
            m.cc |= M6809::CC_I | M6809::CC_F;
            m.pc = r16(rd(0xFFFA), rd(0xFFFB));
            return kCycles1[op];
          }
          default: break;
        }
      }
    }

    if (hi == 4) { reg_op(m.a, lo_mem(lo)); return kCycles1[op]; }
    if (hi == 5) { reg_op(m.b, lo_mem(lo)); return kCycles1[op]; }
    if (hi == 6) {
      u16 ea = indexed_ea();
      mem_op(ea, lo_mem(lo));
      return static_cast<u8>(kCycles1[op] + idx_extra);
    }
    if (hi == 7) {
      u16 ea = extended_ea();
      mem_op(ea, lo_mem(lo));
      return kCycles1[op];
    }

    // 0x8D BSR must not fall into the ALU decoder (lo_alu maps xD → JSR).
    if (op == 0x8D) {
      const s8 d = sex8(fetch());
      push16(m.pc);
      m.pc = static_cast<u16>(m.pc + d);
      return kCycles3[op];
    }

    if (hi >= 8) return exec_alu_range(op, false, 0);

    switch (op) {
      case 0x12: return kCycles1[op];
      case 0x13:
        while (!service_int()) break;
        return kCycles1[op];
      case 0x16: {
        s16 d = static_cast<s16>(fetch16());
        m.pc = static_cast<u16>(m.pc + d);
        return kCycles1[op];
      }
      case 0x17: {
        s16 d = static_cast<s16>(fetch16());
        push16(m.pc);
        m.pc = static_cast<u16>(m.pc + d);
        return kCycles1[op];
      }
      case 0x19: daa(); return kCycles1[op];
      case 0x1A: m.cc |= fetch(); return kCycles1[op];
      case 0x1C: m.cc &= fetch(); return kCycles1[op];
      case 0x1D: // SEX: A becomes the sign of B. V is cleared.
        m.a = (m.b & 0x80) ? 0xFF : 0x00;
        m.cc &= static_cast<u8>(~M6809::CC_V);
        set_nz16(regD());
        return kCycles1[op];
      case 0x1E:
      case 0x1F: {
        u8 pb = fetch();
        u8 src = pb >> 4;
        u8 dst = pb & 0x0F;
        bool ex = (op == 0x1E);
        bool s16 = src < 8;
        bool d16 = dst < 8;
        if (ex && s16 == d16) {
          u16 vs = reg_get(src);
          u16 vd = reg_get(dst);
          reg_put(src, vd);
          reg_put(dst, vs);
        } else if (!ex) {
          reg_put(dst, reg_get(src));
        }
        return kCycles2[op];
      }
      default: break;
    }

    return kCycles1[op];
  }

  int step_cpu() {
    cyc = 0;
    int irq_cyc = service_int();
    if (irq_cyc) return irq_cyc;
    u8 op = fetch();
    cyc = exec_one(op);
    return cyc;
  }
};

void M6809::set_bus(BusRead read, BusWrite write) {
  read_ = std::move(read);
  write_ = std::move(write);
}

void M6809::reset() {
  if (reset_hold_) return;
  a = b = dp = 0;
  x = y = u = 0xFFFF;
  s = 0xFFFF;
  cc = CC_I | CC_F;
  irq_line_ = firq_line_ = nmi_line_ = false;
  nmi_pending_ = false;
  if (read_) {
    // Big-endian reset vector at FFFE/FFFF.
    u8 hi = read_(0xFFFE);
    u8 lo = read_(0xFFFF);
    pc = r16(hi, lo);
  } else {
    pc = 0;
  }
}

void M6809::set_irq(bool asserted) { irq_line_ = asserted; }
void M6809::set_firq(bool asserted) { firq_line_ = asserted; }
void M6809::set_nmi(bool asserted) {
  if (asserted && !nmi_line_) nmi_pending_ = true;
  nmi_line_ = asserted;
}
void M6809::set_halt(bool halted) { halt_ = halted; }
void M6809::set_reset_hold(bool held) { reset_hold_ = held; }

int M6809::step() {
  if (reset_hold_ || halt_ || !read_) return 0;
  // Call sites of the hit routine. Confirm the opcode: other banks also
  // occupy $C000, and only this page has JSR $CFB9 here.
  // $C5F8 writes a knockback velocity on return, so that store is skipped too.
  if (skip_hurt && u == hurt_body && (pc == 0xCED6 || pc == 0xC5F8) &&
      read_(pc) == 0xBD && read_(u16(pc + 1)) == 0xCF && read_(u16(pc + 2)) == 0xB9) {
    pc = pc == 0xCED6 ? u16(0xCED9) : u16(0xC600);
  }
  M6809Core core(*this);
  int c = core.step_cpu();
  cycles_total += static_cast<u64>(c);
  if (trace_) trace_(*this);
  return c;
}

int M6809::run(int budget) {
  if (reset_hold_ || halt_ || budget <= 0) return 0;
  int total = 0;
  while (total < budget) {
    int c = step();
    if (c <= 0) break;
    total += c;
  }
  return total;
}

} // namespace sh
