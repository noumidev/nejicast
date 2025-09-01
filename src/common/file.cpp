/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <common/file.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace common {

std::vector<u8> load_file(const char* path) {
    FILE* file = std::fopen(path, "rb");

    if (file == nullptr) {
        std::printf("Failed to open file \"%s\"\n", path);
        exit(1);
    }

    std::fseek(file, 0, SEEK_END);
    const usize size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    std::vector<u8> file_bytes(size);

    if (std::fread(file_bytes.data(), sizeof(u8), size, file) != size) {
        std::printf("Failed to read file \"%s\"\n", path);
        exit(1);
    }

    return file_bytes;
}

}
