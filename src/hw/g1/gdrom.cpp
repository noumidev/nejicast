/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/g1/gdrom.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <scheduler.hpp>
#include <hw/holly/intc.hpp>

namespace hw::g1::gdrom {

enum : u32 {
    IO_GD_ALT_STATUS    = 0x005F7018,
    IO_GD_DEV_CONTROL   = 0x005F7018,
    IO_GD_DATA          = 0x005F7080,
    IO_GD_FEATURES      = 0x005F7084,
    IO_GD_SECTOR_COUNT  = 0x005F7088,
    IO_GD_SECTOR_NUMBER = 0x005F708C,
    IO_GD_BYTE_COUNT_LO = 0x005F7090,
    IO_GD_BYTE_COUNT_HI = 0x005F7094,
    IO_GD_STATUS        = 0x005F709C,
    IO_GD_COMMAND       = 0x005F709C,
};

#define GD_DEV_CONTROL   ctx.device_control
#define GD_STATUS        ctx.status
#define GD_REASON        ctx.interrupt_reason
#define GD_SECTOR_NUMBER ctx.sector_number
#define GD_BYTE_COUNT    ctx.byte_count

constexpr usize NUM_DATA_IN_BYTES = 12;

struct {
    std::vector<u8> data_in_bytes, data_out_bytes;
    usize data_out_ptr;

    union {
        u8 raw;

        struct {
            u8 check             : 1;
            u8                   : 1;
            u8 correctable_error : 1;
            u8 data_request      : 1;
            u8 seek_completed    : 1;
            u8 drive_fault       : 1;
            u8 data_ready        : 1;
            u8 busy              : 1;
        };
    } status;

    union {
        u8 raw;

        struct {
            u8                   : 1;
            u8 disable_interrupt : 1;
            u8                   : 6;
        };
    } device_control;

    union {
        u8 raw;

        struct {
            u8 is_command  : 1;
            u8 from_device : 1;
            u8             : 6;
        };
    } interrupt_reason;

    union {
        u8 raw;

        struct {
            u8 sense_key   : 4;
            u8 disc_format : 4;
        };
    } sector_number;

    union {
        u16 raw;

        struct {
            u8 lo;
            u8 hi;
        };
    } byte_count;
} ctx;

static void reset_data_in_buffer() {
    std::vector<u8> temp;

    ctx.data_in_bytes.swap(temp);
}

[[maybe_unused]]
static void reset_data_out_buffer() {
    std::vector<u8> temp;

    ctx.data_out_bytes.swap(temp);

    ctx.data_out_ptr = 0;
}

constexpr int GDROM_INTERRUPT = 0;

static void finish_non_data_command() {
    GD_STATUS.busy = 0;

    hw::holly::intc::assert_external_interrupt(GDROM_INTERRUPT);
}

static void prepare_packet_transfer() {
    reset_data_in_buffer();

    GD_STATUS.busy = 0;
    GD_STATUS.data_request = 1;

    GD_REASON.is_command = 1;
    GD_REASON.from_device = 0;
}

static void ata_packet() {
    std::puts("ATA PACKET");

    prepare_packet_transfer();
}

static void ata_set_features() {
    std::puts("ATA SET_FEATURES");

    finish_non_data_command();
}

enum {
    ATA_COMMAND_PACKET       = 0xA0,
    ATA_COMMAND_SET_FEATURES = 0xEF,
};

static void execute_ata_command(const int command) {
    switch (command) {
        case ATA_COMMAND_PACKET:
            ata_packet();
            break;
        case ATA_COMMAND_SET_FEATURES:
            ata_set_features();
            break;
        default:
            std::printf("Unhandled ATA command %02d\n", command);
            exit(1);
    }
}

static void finish_spi_non_data_command() {
    GD_REASON.is_command = 1;
    GD_REASON.from_device = 1;

    GD_STATUS.busy = 0;
    GD_STATUS.data_request = 0;
    GD_STATUS.data_ready = 1;

    hw::holly::intc::assert_external_interrupt(GDROM_INTERRUPT);
}

static void finish_spi_host_pio_command(const u16 size) {
    GD_REASON.is_command = 0;
    GD_REASON.from_device = 1;

    GD_STATUS.busy = 0;
    GD_STATUS.data_request = 1;

    GD_BYTE_COUNT.raw = size;

    hw::holly::intc::assert_external_interrupt(GDROM_INTERRUPT);
}

static void finish_host_pio_transfer() {
    GD_REASON.is_command = 1;
    GD_REASON.from_device = 1;
    
    GD_STATUS.busy = 0;
    GD_STATUS.data_request = 0;
    GD_STATUS.data_ready = 1;

    hw::holly::intc::assert_external_interrupt(GDROM_INTERRUPT);
}

enum {
    SENSE_KEY_NO_SENSE,
};

enum {
    DISC_FORMAT_GDROM = 8,
};

static void spi_test_unit() {
    std::puts("SPI TEST_UNIT");

    GD_SECTOR_NUMBER.sense_key = SENSE_KEY_NO_SENSE;
    GD_SECTOR_NUMBER.disc_format = DISC_FORMAT_GDROM;

    finish_spi_non_data_command();
}

#define SPI_STARTING_ADDRESS  ctx.data_in_bytes[2]
#define SPI_ALLOCATION_LENGTH ctx.data_in_bytes[4]

static void spi_req_mode() {
    std::printf("SPI REQ_MODE (address: %u, length: %u)\n", SPI_STARTING_ADDRESS, SPI_ALLOCATION_LENGTH);

    reset_data_out_buffer();

    for (u8 i = 0; i < SPI_ALLOCATION_LENGTH; i++) {
        ctx.data_out_bytes.push_back(0);
    }

    finish_spi_host_pio_command(SPI_ALLOCATION_LENGTH);
}

#define SPI_COMMAND ctx.data_in_bytes[0]

enum {
    SPI_COMMAND_TEST_UNIT = 0x00,
    SPI_COMMAND_REQ_MODE  = 0x11,
};

static void execute_spi_command(const int command) {
    assert(ctx.data_in_bytes.size() == NUM_DATA_IN_BYTES);

    switch (command) {
        case SPI_COMMAND_TEST_UNIT:
            spi_test_unit();
            break;
        case SPI_COMMAND_REQ_MODE:
            spi_req_mode();
            break;
        default:
            std::printf("Unhandled SPI command %02d\n", command);
            exit(1);
    }
}

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped GD-ROM read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u8 read(const u32 addr) {
    switch (addr) {
        case IO_GD_ALT_STATUS:
            std::puts("GD_ALT_STATUS read8");

            return GD_STATUS.raw;
        case IO_GD_SECTOR_NUMBER:
            std::puts("GD_SECTOR_NUMBER read8");

            return GD_SECTOR_NUMBER.raw;
        case IO_GD_BYTE_COUNT_LO:
            std::puts("GD_BYTE_COUNT_LO read8");

            return GD_BYTE_COUNT.lo;
        case IO_GD_BYTE_COUNT_HI:
            std::puts("GD_BYTE_COUNT_HI read8");

            return GD_BYTE_COUNT.hi;
        case IO_GD_STATUS:
            std::puts("GD_STATUS read8");

            hw::holly::intc::clear_external_interrupt(GDROM_INTERRUPT);

            return GD_STATUS.raw;
        default:
            std::printf("Unmapped GD-ROM read8 @ %08X\n", addr);
            exit(1);
    }
}

template<>
u16 read(const u32 addr) {
    switch (addr) {
        case IO_GD_DATA:
            {
                std::puts("GD_DATA read16");

                u16 data = ctx.data_out_bytes[ctx.data_out_ptr++];

                if (ctx.data_out_ptr < ctx.data_out_bytes.size()) {
                    data |= ctx.data_out_bytes[ctx.data_out_ptr++] << 8;
                }

                if (ctx.data_out_ptr == ctx.data_out_bytes.size()) {
                    finish_host_pio_transfer();
                }

                return data;
            }
        default:
            std::printf("Unmapped GD-ROM read16 @ %08X\n", addr);
            exit(1);
    }
}

template u32 read(u32);
template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped GD-ROM write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u8 data) {
    switch (addr) {
        case IO_GD_DEV_CONTROL:
            std::printf("GD_DEV_CONTROL write8 = %02X\n", data);

            GD_DEV_CONTROL.raw = data;
            break;
        case IO_GD_FEATURES:
            std::printf("GD_FEATURES write8 = %02X\n", data);
            break;
        case IO_GD_SECTOR_COUNT:
            std::printf("GD_SECTOR_COUNT write8 = %02X\n", data);
            break;
        case IO_GD_BYTE_COUNT_LO:
            std::printf("GD_BYTE_COUNT_LO write8 = %02X\n", data);

            GD_BYTE_COUNT.lo = data;
            break;
        case IO_GD_BYTE_COUNT_HI:
            std::printf("GD_BYTE_COUNT_HI write8 = %02X\n", data);

            GD_BYTE_COUNT.hi = data;
            break;
        case IO_GD_COMMAND:
            std::printf("GD_COMMAND write8 = %02X\n", data);

            // Unsure about the timings of all the commands, so we'll just delay them by a bit
            scheduler::schedule_event("ATA", execute_ata_command, data, 4096);

            GD_STATUS.busy = 1;
            break;
        default:
            std::printf("Unmapped GD-ROM write8 @ %08X = %02X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case IO_GD_DATA:
            std::printf("GD_DATA write16 = %04X\n", data);

            assert(ctx.data_in_bytes.size() < NUM_DATA_IN_BYTES);

            ctx.data_in_bytes.push_back(data);
            ctx.data_in_bytes.push_back(data >> 8);

            if (ctx.data_in_bytes.size() >= NUM_DATA_IN_BYTES) {
                scheduler::schedule_event("SPI", execute_spi_command, SPI_COMMAND, 4096);

                GD_STATUS.busy = 1;
                GD_STATUS.data_request = 0;
            }
            break;
        default:
            std::printf("Unmapped GD-ROM write16 @ %08X = %04X\n", addr, data);
            exit(1);
    }
}

template void write(u32, u32);
template void write(u32, u64);

}
