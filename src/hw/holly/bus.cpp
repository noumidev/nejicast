/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <cstddef>
#include <hw/holly/bus.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hw/g1/g1.hpp>

namespace hw::holly::bus {

constexpr usize ADDRESS_SPACE = 0x20000000;
constexpr usize PAGE_SIZE = 0x1000;
constexpr usize PAGE_MASK = PAGE_SIZE - 1;

namespace MemoryBase {
    enum : u32 {
        BootRom = 0x00000000,
        G1      = 0x005F7400,
    };
}

namespace MemorySize {
    enum : u32 {
        BootRom = 0x00200000,
        Io      = 0x00000100,
    };
}

struct {
    // Pagetables for software fastmem
    std::array<u8*, ADDRESS_SPACE / PAGE_SIZE> rd_table, wr_table;
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
        MemoryBase::BootRom,
        MemorySize::BootRom,
        true, 
        false
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

    switch (addr & ~(MemorySize::Io - 1)) {
        case MemoryBase::G1:
            return hw::g1::read<T>(addr);
    }

    std::printf("Unmapped read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    assert(addr < ADDRESS_SPACE);

    const u32 page = addr / PAGE_SIZE;
    const u32 offset = addr & PAGE_MASK;

    if (ctx.wr_table[page] != nullptr) {
        std::memcpy(&ctx.wr_table[page][offset], &data, sizeof(data));
        return;
    }

    switch (addr & ~(MemorySize::Io - 1)) {
        case MemoryBase::G1:
            return hw::g1::write<T>(addr, data);
    }

    std::printf("Unmapped write%zu @ %08X = %0*X\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), data);
    exit(1);
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u32);

}