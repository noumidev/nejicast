/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/holly/bus.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <hw/g1/g1.hpp>
#include <hw/g1/gdrom.hpp>
#include <hw/g2/aica.hpp>
#include <hw/g2/g2.hpp>
#include <hw/g2/modem.hpp>
#include <hw/holly/holly.hpp>
#include <hw/holly/intc.hpp>
#include <hw/holly/maple.hpp>
#include <hw/pvr/core.hpp>
#include <hw/pvr/interface.hpp>

namespace hw::holly::bus {

constexpr usize ADDRESS_SPACE = 0x20000000;
constexpr usize PAGE_SIZE = 0x1000;
constexpr usize PAGE_MASK = PAGE_SIZE - 1;

enum : u32 {
    BASE_BOOT_ROM  = 0x00000000,
    BASE_FLASH_ROM = 0x00200000,
    BASE_INTC      = 0x005F6900,
    BASE_MAPLE     = 0x005F6C00,
    BASE_GDROM     = 0x005F7000,
    BASE_G1        = 0x005F7400,
    BASE_G2        = 0x005F7800,
    BASE_PVR_IF    = 0x005F7C00,
    BASE_PVR_CORE  = 0x005F8000,
    BASE_MODEM     = 0x00600000,
    BASE_AICA      = 0x00700000,
    BASE_WAVE_RAM  = 0x00800000,
    BASE_VIDEO_RAM = 0x05000000,
    BASE_DRAM      = 0x0C000000,
};

enum : u32 {
    SIZE_BOOT_ROM  = 0x00200000,
    SIZE_FLASH_ROM = 0x00020000,
    SIZE_IO        = 0x00000100,
    SIZE_MODEM     = 0x00000800,
    SIZE_PVR_CORE  = 0x00002000,
    SIZE_AICA      = 0x00008000,
    SIZE_WAVE_RAM  = 0x00200000,
    SIZE_VIDEO_RAM = 0x00800000,
    SIZE_DRAM      = 0x01000000,
};

struct {
    // Pagetables for software fastmem
    std::array<u8*, ADDRESS_SPACE / PAGE_SIZE> rd_table, wr_table;

    std::array<u8, SIZE_DRAM> dram;
} ctx;

static bool is_aligned(const u64 addr, const u64 align) {
    return (addr & (align - 1)) == 0;
}

static void map_memory(
    u8* mem,
    const u32 addr,
    const u32 size,
    const bool map_for_read,
    const bool map_for_write
) {
    assert(is_aligned(addr, PAGE_SIZE));
    assert(is_aligned(size, PAGE_SIZE));

    const u32 first_page = addr / PAGE_SIZE;
    const u32 num_pages = size / PAGE_SIZE;

    for (u32 page = first_page; page < (first_page + num_pages); page++) {
        const u32 mem_idx = page - first_page;

        if (map_for_read) {
            assert(ctx.rd_table[page] == nullptr);

            ctx.rd_table[page] = &mem[mem_idx * PAGE_SIZE];
        }

        if (map_for_write) {
            assert(ctx.wr_table[page] == nullptr);

            ctx.wr_table[page] = &mem[mem_idx * PAGE_SIZE];
        }
    }
}

void initialize() {
    map_memory(
        g1::get_boot_rom_ptr(),
        BASE_BOOT_ROM,
        SIZE_BOOT_ROM,
        true,
        false
    );

    map_memory(
        g1::get_flash_rom_ptr(),
        BASE_FLASH_ROM,
        SIZE_FLASH_ROM,
        true,
        false
    );

    map_memory(
        g2::aica::get_wave_ram_ptr(),
        BASE_WAVE_RAM,
        SIZE_WAVE_RAM,
        true,
        true
    );

    map_memory(
        pvr::core::get_video_ram_ptr(),
        BASE_VIDEO_RAM,
        SIZE_VIDEO_RAM,
        true,
        true
    );

    map_memory(
        ctx.dram.data(),
        BASE_DRAM,
        SIZE_DRAM,
        true,
        true
    );
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    assert(addr < ADDRESS_SPACE);

    const u32 page = addr / PAGE_SIZE;
    const u32 offset = addr & PAGE_MASK;

    if (ctx.rd_table[page] != nullptr) {
        T data;

        std::memcpy(&data, &ctx.rd_table[page][offset], sizeof(data));

        return data;
    }

    switch (addr & ~(SIZE_IO - 1)) {
        case BASE_INTC:
            return hw::holly::intc::read<T>(addr);
        case BASE_MAPLE:
            return hw::holly::maple::read<T>(addr);
        case BASE_GDROM:
            return hw::g1::gdrom::read<T>(addr);
        case BASE_G1:
            return hw::g1::read<T>(addr);
        case BASE_G2:
            return hw::g2::read<T>(addr);
        case BASE_PVR_IF:
            return hw::pvr::interface::read<T>(addr);
    }

    if ((addr & ~(SIZE_PVR_CORE - 1)) == BASE_PVR_CORE) {
        return hw::pvr::core::read<T>(addr);
    }

    if ((addr & ~(SIZE_MODEM - 1)) == BASE_MODEM) {
        return hw::g2::modem::read<T>(addr);
    }

    if ((addr & ~(SIZE_AICA - 1)) == BASE_AICA) {
        return hw::g2::aica::read<T>(addr);
    }

    // Redirect read
    return hw::holly::read<T>(addr);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    assert(addr < ADDRESS_SPACE);

    const u32 page = addr / PAGE_SIZE;
    const u32 offset = addr & PAGE_MASK;

    if (ctx.wr_table[page] != nullptr) {
        std::memcpy(&ctx.wr_table[page][offset], &data, sizeof(data));
        return;
    }

    switch (addr & ~(SIZE_IO - 1)) {
        case BASE_INTC:
            return hw::holly::intc::write<T>(addr, data);
        case BASE_MAPLE:
            return hw::holly::maple::write<T>(addr, data);
        case BASE_GDROM:
            return hw::g1::gdrom::write<T>(addr, data);
        case BASE_G1:
            return hw::g1::write<T>(addr, data);
        case BASE_G2:
            return hw::g2::write<T>(addr, data);
        case BASE_PVR_IF:
            return hw::pvr::interface::write<T>(addr, data);
    }

    if ((addr & ~(SIZE_PVR_CORE - 1)) == BASE_PVR_CORE) {
        return hw::pvr::core::write<T>(addr, data);
    }

    if ((addr & ~(SIZE_MODEM - 1)) == BASE_MODEM) {
        return hw::g2::modem::write<T>(addr, data);
    }

    if ((addr & ~(SIZE_AICA - 1)) == BASE_AICA) {
        return hw::g2::aica::write<T>(addr, data);
    }

    // Redirect write
    hw::holly::write<T>(addr, data);
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u32);
template void write(u32, u64);

void dump_memory(
    const u32 addr,
    const u32 size,
    const char* path
) {
    FILE* file = std::fopen(path, "w+b");

    std::vector<u8> file_bytes(size);

    for (u32 i = 0; i < size; i++) {
        file_bytes[i] = read<u8>(addr + i);
    }

    std::fwrite(file_bytes.data(), sizeof(u8), size, file);
    std::fclose(file);
}

}