/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

namespace scheduler {

typedef void (*Callback)(const int);

constexpr i64 HOLLY_CLOCKRATE = 100000000;
constexpr i64 PIXEL_CLOCKRATE = 13500000;

constexpr i64 SCHEDULER_CLOCKRATE = HOLLY_CLOCKRATE;

void initialize();
void reset();
void shutdown();

template<i64 clockrate>
i64 to_scheduler_cycles(const i64 cycles) {
    return (SCHEDULER_CLOCKRATE * cycles) / clockrate;
}

void schedule_event(const char* name, Callback callback, const int arg, const i64 cycles);

void run();

}
