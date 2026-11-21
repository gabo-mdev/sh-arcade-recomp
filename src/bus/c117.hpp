#pragma once

#include "sh/types.hpp"

#include <array>
#include <functional>

namespace sh {

class VirtMap;

/// Namco CUS117 MMU + IRQ controller for main/sub 6809.
class C117 {
public:
  enum CpuId { Main = 0, Sub = 1 };

  using IrqFn = std::function<void(CpuId, bool assert_line)>;
  using FirqFn = std::function<void(CpuId, bool assert_line)>;
  using SubresFn = std::function<void(bool hold_in_reset)>;
  using WatchdogFn = std::function<void(CpuId)>;

  explicit C117(VirtMap& map);

  void reset();

  u8 read(CpuId cpu, u16 addr);
  void write(CpuId cpu, u16 addr, u8 data);

  u32 remap(CpuId cpu, u16 addr) const {
    return offsets_[cpu][addr >> 13] | (addr & 0x1FFF);
  }

  void set_irq_ack(IrqFn fn) { irq_ack_ = std::move(fn); }
  void set_firq_ack(FirqFn fn) { firq_ack_ = std::move(fn); }
  void set_subres(SubresFn fn) { subres_cb_ = std::move(fn); }
  void set_watchdog(WatchdogFn fn) { watchdog_ = std::move(fn); }
  void set_assert_sub_firq(std::function<void()> fn) { assert_sub_firq_ = std::move(fn); }
  void set_bank7_probe(std::function<void(u16 offset, u8 data, u32 prev)> fn) {
    bank7_probe_ = std::move(fn);
  }

  const std::array<u32, 8>& banks(CpuId cpu) const { return offsets_[cpu]; }
  u8 subres_bit() const { return subres_bit_; }

private:
  void register_w(CpuId cpu, u16 offset, u8 data);
  void bankswitch(CpuId cpu, int bank, int a0, u8 data);

  VirtMap& map_;
  std::array<std::array<u32, 8>, 2> offsets_{};
  u8 subres_bit_ = 0;
  IrqFn irq_ack_;
  FirqFn firq_ack_;
  SubresFn subres_cb_;
  WatchdogFn watchdog_;
  std::function<void()> assert_sub_firq_;
  std::function<void(u16, u8, u32)> bank7_probe_;
};

} // namespace sh
