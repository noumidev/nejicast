/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <nejicast.hpp>

#include <cstdio>

#include <hw/cpu/cpu.hpp>
#include <hw/g1/g1.hpp>
#include <hw/holly/bus.hpp>

constexpr int NUM_ARGS = 2;

constexpr i64 CYCLES_PER_SLICE = 128;

namespace nejicast {

void initialize(const common::Config& config) {
    hw::cpu::initialize();
    hw::g1::initialize(config.boot_path);
    hw::holly::bus::initialize();
}

void shutdown() {
    hw::cpu::shutdown();
    hw::g1::shutdown();
    hw::holly::bus::shutdown();
}

void reset() {
    hw::cpu::reset();
    hw::g1::reset();
    hw::holly::bus::reset();
}

void run() {
    while (true) {
        *hw::cpu::get_cycles() = CYCLES_PER_SLICE;

        hw::cpu::step();
    }
}

}

int main(int argc, char** argv) {
    if (argc < NUM_ARGS) {
        std::puts("Usage: nejicast [path to boot ROM]");
        return 1;
    }

    common::Config config {.boot_path = argv[1]};
    
    nejicast::reset();
    nejicast::initialize(config);
    nejicast::run();
    nejicast::shutdown();

    return 0;
}
