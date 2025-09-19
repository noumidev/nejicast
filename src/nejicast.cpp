/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <nejicast.hpp>

#include <cstdio>
#include <cstring>

#include <scheduler.hpp>
#include <common/elf.hpp>
#include <hw/cpu/cpu.hpp>
#include <hw/g1/g1.hpp>
#include <hw/g2/g2.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/holly.hpp>
#include <hw/pvr/pvr.hpp>

constexpr int NUM_ARGS = 4;

namespace nejicast {

void initialize(const common::Config& config) {
    scheduler::initialize();

    hw::cpu::initialize();
    hw::g1::initialize(config.boot_path, config.flash_path);
    hw::g2::initialize();
    hw::holly::initialize();
    hw::pvr::initialize();

    common::load_elf(config.elf_path);
}

void shutdown() {
    scheduler::shutdown();

    hw::cpu::shutdown();
    hw::g1::shutdown();
    hw::g2::shutdown();
    hw::holly::shutdown();
    hw::pvr::shutdown();
}

void reset() {
    scheduler::reset();

    hw::cpu::reset();
    hw::g1::reset();
    hw::g2::reset();
    hw::holly::reset();
    hw::pvr::reset();
}

void sideload(const u32 entry) {
    hw::holly::bus::setup_for_sideload();
    hw::cpu::setup_for_sideload(entry);
}

void run() {
    while (true) {
        scheduler::run();
    }
}

}

int main(int argc, char** argv) {
    if (argc < NUM_ARGS) {
        std::puts("Usage: nejicast [path to boot ROM] [path to FLASH ROM] [path to ELF]");
        return 1;
    }

    common::Config config {
        .boot_path = argv[1],
        .flash_path = argv[2],
        .elf_path = argv[3]
    };
    
    nejicast::reset();
    nejicast::initialize(config);
    nejicast::run();
    nejicast::shutdown();

    return 0;
}
