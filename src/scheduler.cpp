/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <scheduler.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

#include <hw/cpu/cpu.hpp>

namespace scheduler {

constexpr i64 FRAME_CYCLES = SCHEDULER_CLOCKRATE / 60;

constexpr i64 MAX_CYCLES = 512;

struct Event {
    Callback callback;

    int arg;
    i64 timestamp;

    bool operator>(const Event &other) const {
        return timestamp > other.timestamp;
    }
};

typedef std::priority_queue<Event, std::vector<Event>, std::greater<Event>> EventQueue;

static EventQueue scheduled_events;

static i64 global_timestamp;
static i64 elapsed_cycles;

static void set_cpu_cycles_and_step(const i64 cycles) {
    *hw::cpu::get_cycles() = cycles;

    hw::cpu::step();
}

void initialize() {}

void reset() {
    // Clear event queue
    EventQueue temp;
    scheduled_events.swap(temp);

    global_timestamp = 0;
    elapsed_cycles = 0;
}

void shutdown() {}

void schedule_event(const char *name, Callback callback, const int arg, const i64 cycles) {
    if (
        (std::strcmp(name, "HBLANK") != 0) &&
        (std::strcmp(name, "SCIF_TX") != 0)
    ) {
        std::printf("Scheduling event %s with arg = %d, cycles = %lld\n", name, arg, cycles);
    }

    scheduled_events.emplace(Event{callback, arg, global_timestamp + cycles - *hw::cpu::get_cycles()});
}

bool run() {
    const i64 new_timestamp = global_timestamp + MAX_CYCLES;

    elapsed_cycles += MAX_CYCLES;

    while (!scheduled_events.empty() && (scheduled_events.top().timestamp <= new_timestamp)) {
        const Callback callback = scheduled_events.top().callback;

        const int arg = scheduled_events.top().arg;
        const i64 timestamp = scheduled_events.top().timestamp;

        scheduled_events.pop();

        set_cpu_cycles_and_step(timestamp - global_timestamp);

        global_timestamp = timestamp;

        callback(arg);
    }

    set_cpu_cycles_and_step(new_timestamp - global_timestamp);

    global_timestamp = new_timestamp;

    if (elapsed_cycles >= FRAME_CYCLES) {
        elapsed_cycles -= FRAME_CYCLES;

        return false;
    }

    return true;
}

}
