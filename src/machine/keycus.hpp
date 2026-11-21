#pragma once

#include "sh/types.hpp"

#include <array>
#include <random>

namespace sh {

/// Splatterhouse keycus type 3, id 181.
class KeyCus {
public:
  void reset() {
    key_.fill(0);
    rng_.seed(0xC0FFEEu);
  }

  u8 read(u16 offset) {
    const int op = (offset & 0x70) >> 4;
    if (op == key_reg_)
      return u8(key_id_);
    if (op == key_rng_)
      return u8(rng_() & 0xFF);
    // Remaining ops disabled for Splatterhouse (-1 in MAME init).
    return 0;
  }

  void write(u16 offset, u8 data) {
    key_[size_t((offset & 0x70) >> 4)] = data;
  }

private:
  // init_splatter: key_type_3_init(181, 3, 4, -1, -1, -1, -1)
  static constexpr int key_id_ = 181;
  static constexpr int key_reg_ = 3;
  static constexpr int key_rng_ = 4;

  std::array<u8, 8> key_{};
  std::mt19937 rng_;
};

} // namespace sh
