/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/aica/bus.hpp>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <hw/aica/aica.hpp>

namespace hw::aica::bus {

constexpr usize ADDRESS_SPACE = 0x100000000;
constexpr usize PAGE_SIZE = 0x1000;
constexpr usize PAGE_MASK = PAGE_SIZE - 1;

enum : u32 {
    BASE_WAVE_RAM = 0x00000000,
};

enum : u32 {
    SIZE_WAVE_RAM = 0x00200000,
};

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
        aica::get_wave_ram_ptr(),
        BASE_WAVE_RAM,
        SIZE_WAVE_RAM,
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
    const u32 page = addr / PAGE_SIZE;
    const u32 offset = addr & PAGE_MASK;

    if (ctx.rd_table[page] != nullptr) {
        T data;

        std::memcpy(&data, &ctx.rd_table[page][offset], sizeof(data));

        return data;
    }

    std::printf("Unmapped ARM read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template u8 read(u32);
template u16 read(u32);
template u32 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    const u32 page = addr / PAGE_SIZE;
    const u32 offset = addr & PAGE_MASK;

    if (ctx.wr_table[page] != nullptr) {
        std::memcpy(&ctx.wr_table[page][offset], &data, sizeof(data));
        return;
    }

    std::printf("Unhandled ARM write32 @ %08X = %08X\n", addr, data);
    exit(1);
}

template void write(u32, u8);
template void write(u32, u16);
template void write(u32, u32);

void copy_from_bytes(
    const u32 addr,
    const u32 copy_size,
    const u32 total_size,
    const u8* bytes
) {
    for (u32 i = 0; i < copy_size; i++) {
        write<u8>(addr + i, bytes[i]);
    }

    for (u32 i = copy_size; i < total_size; i++) {
        write<u8>(addr + i, 0);
    }
}

// Same as HOLLY bus
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