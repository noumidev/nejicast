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
#include <vector>

#include <hw/g1/g1.hpp>
#include <hw/holly/holly.hpp>
#include <hw/holly/intc.hpp>

namespace hw::holly::bus {

constexpr usize ADDRESS_SPACE = 0x20000000;
constexpr usize PAGE_SIZE = 0x1000;
constexpr usize PAGE_MASK = PAGE_SIZE - 1;

namespace MemoryBase {
    enum : u32 {
        BootRom = 0x00000000,
        Intc    = 0x005F6900,
        G1      = 0x005F7400,
        Dram    = 0x0C000000,
    };
}

namespace MemorySize {
    enum : u32 {
        BootRom = 0x00200000,
        Io      = 0x00000100,
        Dram    = 0x01000000,
    };
}

struct {
    // Pagetables for software fastmem
    std::array<u8*, ADDRESS_SPACE / PAGE_SIZE> rd_table, wr_table;

    std::array<u8, MemorySize::Dram> dram;
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

    map_memory(
        ctx.dram.data(),
        MemoryBase::Dram,
        MemorySize::Dram,
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

    switch (addr & ~(MemorySize::Io - 1)) {
        case MemoryBase::Intc:
            return hw::holly::intc::read<T>(addr);
        case MemoryBase::G1:
            return hw::g1::read<T>(addr);
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

    switch (addr & ~(MemorySize::Io - 1)) {
        case MemoryBase::Intc:
            return hw::holly::intc::write<T>(addr, data);
        case MemoryBase::G1:
            return hw::g1::write<T>(addr, data);
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