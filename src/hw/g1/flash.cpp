/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#include <hw/g1/flash.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <scheduler.hpp>
#include <common/file.hpp>

namespace hw::g1::flash {

constexpr usize FLASH_ROM_SIZE = 0x20000;

enum class FlashState {
    Idle,
    Unlock_1,
    Unlock_2,
    ByteProgram
};

enum {
    FLASH_COMMAND_BYTE_PROGRAM = 0xA0
};

std::vector<u8> flash_bytes;

FlashState state;

static void state_transition(const FlashState new_state) {
    state = new_state;
}

void initialize(const char* flash_path) {
    flash_bytes = common::load_file(flash_path);

    if (flash_bytes.size() != FLASH_ROM_SIZE) {
        std::printf("Invalid FLASH ROM size %zu\n", flash_bytes.size());
        exit(1);
    }
}

void reset() {
    state_transition(FlashState::Idle);
}

void shutdown() {}

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped FLASH write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

constexpr u32 UNLOCK_ADDRESS_1 = 0x5555;
constexpr u32 UNLOCK_ADDRESS_2 = 0x2AAA;

constexpr u8 UNLOCK_DATA_1 = 0xAA;
constexpr u8 UNLOCK_DATA_2 = 0x55;

#define FLASH_ADDRESS addr & (FLASH_ROM_SIZE - 1)

template<>
void write(const u32 addr, const u8 data) {
    std::printf("FLASH write8 @ %08X = %02X\n", addr, data);

    switch (state) {
        case FlashState::Idle:
            if (((addr & 0xFFFF) == UNLOCK_ADDRESS_1) && (data == UNLOCK_DATA_1)) {
                state_transition(FlashState::Unlock_1);
            } else {
                std::printf("FLASH Unhandled idle state transition @ %08X = %02X\n", addr, data);
                exit(1);
            }
            break;
        case FlashState::Unlock_1:
            if (((addr & 0xFFFF) == UNLOCK_ADDRESS_2) && (data == UNLOCK_DATA_2)) {
                state_transition(FlashState::Unlock_2);
            } else {
                std::printf("FLASH Unhandled unlock (1) state transition @ %08X = %02X\n", addr, data);
                exit(1);
            }
            break;
        case FlashState::Unlock_2:
            if ((addr & 0xFFFF) == UNLOCK_ADDRESS_1) {
                // Software can send erase/program commands here
                switch (data) {
                    case FLASH_COMMAND_BYTE_PROGRAM:
                        state_transition(FlashState::ByteProgram);
                        break;
                    default:
                        std::printf("FLASH Unimplemented command %02X\n", data);
                        exit(1);
                }
            } else {
                std::printf("FLASH Unhandled unlock (2) state transition @ %08X = %02X\n", addr, data);
                exit(1);
            }
            break;
        case FlashState::ByteProgram:
            std::printf("FLASH Byte program @ %05zX = %02X\n", FLASH_ADDRESS, data);

            flash_bytes[FLASH_ADDRESS] = data;

            state_transition(FlashState::Idle);
            break;
    }
}

template void write(u32, u16);
template void write(u32, u32);
template void write(u32, u64);

// For HOLLY access
u8* get_flash_rom_ptr() {
    return flash_bytes.data();
}

}
