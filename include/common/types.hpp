/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstring>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using usize = std::size_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

inline u32 bswap24_from_buf(const u8* buf) {
    return (buf[0] << 16) | (buf[1] << 8) | buf[2];
}

inline void bswap24_to_buf(const u32 data, u8* buf) {
    buf[0] = data >> 16;
    buf[1] = data >> 8;
    buf[2] = data;
}

inline u32 from_f32(const f32 data) {
    u32 n;

    std::memcpy(&n, &data, sizeof(n));

    return n;
}

inline f32 to_f32(const u32 data) {
    f32 f;

    std::memcpy(&f, &data, sizeof(f));

    return f;
}
