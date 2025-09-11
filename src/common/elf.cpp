/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <common/elf.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <nejicast.hpp>
#include <common/file.hpp>
#include <hw/holly/bus.hpp>

namespace common {

constexpr u32 ELF_SIGNATURE = 0x464C457F;

constexpr usize PROGRAM_SEGMENT_SIZE = 0x20;

enum {
    ELF_OFFSET_SIGNATURE  = 0x00,
    ELF_OFFSET_CLASS      = 0x04,
    ELF_OFFSET_DATA       = 0x05,
    ELF_OFFSET_TYPE       = 0x10,
    ELF_OFFSET_ENTRYPOINT = 0x18,
    ELF_OFFSET_PH_OFFSET  = 0x1C,
    ELF_OFFSET_PH_ENTRIES = 0x2C,
};

enum {
    ELF_TYPE_EXECUTABLE = 2,
};

enum {
    PH_OFFSET_TYPE        = 0x00,
    PH_OFFSET_FILE_OFFSET = 0x04,
    PH_OFFSET_VIRT_ADDR   = 0x08,
    PH_OFFSET_PHYS_ADDR   = 0x0C,
    PH_OFFSET_FILE_SIZE   = 0x10,
    PH_OFFSET_MEMORY_SIZE = 0x14,
};

enum {
    PROGRAM_TYPE_LOAD = 1,
};

template<typename T>
static T get(const std::vector<u8>& bytes, const usize offset) {
    assert((offset + sizeof(T)) <= bytes.size());

    T data;

    std::memcpy(&data, &bytes[offset], sizeof(T));

    return data;
}

void load_elf(const char* path) {
    const std::vector<u8> elf_bytes = load_file(path);

    // Sanity checks
    assert(get<u32>(elf_bytes, ELF_OFFSET_SIGNATURE) == ELF_SIGNATURE);
    assert(get<u8>(elf_bytes, ELF_OFFSET_CLASS) == 1);
    assert(get<u8>(elf_bytes, ELF_OFFSET_DATA) == 1);
    assert(get<u16>(elf_bytes, ELF_OFFSET_TYPE) == ELF_TYPE_EXECUTABLE);

    const u32 entry = get<u32>(elf_bytes, ELF_OFFSET_ENTRYPOINT);

    std::printf("ELF entrypoint = %08X\n", entry);

    nejicast::sideload(entry);

    const u32 ph_offset = get<u32>(elf_bytes, ELF_OFFSET_PH_OFFSET);
    const u16 ph_num_entries = get<u16>(elf_bytes, ELF_OFFSET_PH_ENTRIES);

    std::printf("ELF program header offset = %08X, number of entries = %u\n", ph_offset, ph_num_entries);

    for (u16 ph_entry = 0; ph_entry < ph_num_entries; ph_entry++) {
        const usize ph_base = ph_offset + ph_entry * PROGRAM_SEGMENT_SIZE;

        // Sanity checks
        assert(get<u32>(elf_bytes, ph_base + PH_OFFSET_TYPE) == PROGRAM_TYPE_LOAD);

        const u32 ph_offset = get<u32>(elf_bytes, ph_base + PH_OFFSET_FILE_OFFSET);
        const u32 ph_vaddr = get<u32>(elf_bytes, ph_base + PH_OFFSET_VIRT_ADDR);
        const u32 ph_paddr = get<u32>(elf_bytes, ph_base + PH_OFFSET_PHYS_ADDR);
        const u32 ph_filesz = get<u32>(elf_bytes, ph_base + PH_OFFSET_FILE_SIZE);
        const u32 ph_memsz = get<u32>(elf_bytes, ph_base + PH_OFFSET_MEMORY_SIZE);

        std::printf("ELF program segment %u, offset = %08X, vaddr = %08X, paddr = %08X, filesz = %08X, memsz = %08X\n",
            ph_entry,
            ph_offset,
            ph_vaddr,
            ph_paddr,
            ph_filesz,
            ph_memsz
        );

        hw::holly::bus::copy_from_bytes(
            ph_vaddr,
            ph_filesz,
            ph_memsz,
            &elf_bytes[ph_offset]
        );
    }
}

}
