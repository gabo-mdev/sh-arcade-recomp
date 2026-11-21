#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <array>
#include <stdexcept>

namespace sh {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;

inline constexpr int kScreenW = 288;
inline constexpr int kScreenH = 224;
inline constexpr double kRefreshHz = 60.606061; // ~49.152MHz/8 / (384*264)

struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

} // namespace sh
