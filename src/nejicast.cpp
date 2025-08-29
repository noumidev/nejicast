/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <nejicast.hpp>

#include <cstdio>

#include <hw/cpu/cpu.hpp>
#include <hw/g1/g1.hpp>
#include <hw/g2/g2.hpp>
#include <hw/holly/holly.hpp>
#include <hw/pvr/core.hpp>
#include <hw/pvr/interface.hpp>
#include <hw/pvr/spg.hpp>
#include <hw/pvr/ta.hpp>

constexpr int NUM_ARGS = 3;

constexpr i64 CYCLES_PER_SLICE = 128;

namespace nejicast {

void initialize(const common::Config& config) {
    hw::cpu::initialize();
    hw::g1::initialize(config.boot_path, config.flash_path);
    hw::g2::initialize();
    hw::holly::initialize();
    hw::pvr::core::initialize();
    hw::pvr::interface::initialize();
    hw::pvr::spg::initialize();
    hw::pvr::ta::initialize();
}

void shutdown() {
    hw::cpu::shutdown();
    hw::g1::shutdown();
    hw::g2::shutdown();
    hw::holly::shutdown();
    hw::pvr::core::shutdown();
    hw::pvr::interface::shutdown();
    hw::pvr::spg::shutdown();
    hw::pvr::ta::shutdown();
}

void reset() {
    hw::cpu::reset();
    hw::g1::reset();
    hw::g2::reset();
    hw::holly::reset();
    hw::pvr::core::reset();
    hw::pvr::interface::reset();
    hw::pvr::spg::reset();
    hw::pvr::ta::reset();
}

void run() {
    while (true) {
        *hw::cpu::get_cycles() = CYCLES_PER_SLICE;

        hw::cpu::step();
        hw::pvr::spg::step(CYCLES_PER_SLICE / 7);
    }
}

}

int main(int argc, char** argv) {
    if (argc < NUM_ARGS) {
        std::puts("Usage: nejicast [path to boot ROM] [path to FLASH ROM]");
        return 1;
    }

    common::Config config {.boot_path = argv[1], .flash_path = argv[2]};
    
    nejicast::reset();
    nejicast::initialize(config);
    nejicast::run();
    nejicast::shutdown();

    return 0;
}
