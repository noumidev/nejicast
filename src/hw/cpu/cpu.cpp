/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/cpu.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hw/cpu/ocio.hpp>
#include <hw/holly/bus.hpp>

namespace hw::cpu {

// Instruction bit macros
#define IMM get_bits(instr, 0,  7)
#define D   get_bits(instr, 0,  3)
#define M   get_bits(instr, 4,  7)
#define N   get_bits(instr, 8, 11)

static inline u32 get_mask(const u32 start, const u32 end) {
    assert(start <= end);

    return (UINT32_MAX << start) & (UINT32_MAX >> ((8 * sizeof(u32) - 1) - end));
}

static inline u32 get_bits(const u32 n, const u32 start, const u32 end) {
    return (n & get_mask(start, end)) >> start;
}

// Register file macros
#define PC          ctx.pc
#define CPC         ctx.current_pc
#define NPC         ctx.next_pc
#define SPC         ctx.spc
#define GPRS        ctx.gprs
#define BANKED_GPRS ctx.banked_gprs
#define SGR         ctx.sgr
#define SR          ctx.sr
#define SSR         ctx.ssr
#define GBR         ctx.gbr
#define VBR         ctx.vbr
#define DBR         ctx.dbr

constexpr usize NUM_REGS = 16;
constexpr usize NUM_BANKED_REGS = 8;

constexpr usize INSTR_TABLE_SIZE = 0x10000;

struct {
    // PC and delay slot helpers
    u32 pc, current_pc, next_pc;
    u32 spc;

    // GPRs, banked GPRs
    u32 gprs[NUM_REGS], banked_gprs[NUM_BANKED_REGS];
    u32 sgr;

    // Base registers
    u32 gbr, vbr, dbr;

    union {
        u32 raw;

        struct {
            u32 t               :  1;
            u32 saturate_mac    :  1;
            u32                 :  2;
            u32 interrupt_mask  :  4;
            u32 q               :  1;
            u32 m               :  1;
            u32                 :  5;
            u32 disable_fpu     :  1;
            u32                 : 12;
            u32 block_exception :  1;
            u32 select_bank     :  1;
            u32 is_privileged   :  1;
            u32                 :  1;
        };
    } sr, ssr;

    std::array<i64(*)(const u16), INSTR_TABLE_SIZE> instr_table;

    i64 cycles;
} ctx;

static void dump_registers() {
    u32* bank_0 = (SR.select_bank) ? BANKED_GPRS : GPRS; 
    u32* bank_1 = (SR.select_bank) ? GPRS : BANKED_GPRS;

    for (usize i = 0; i < NUM_BANKED_REGS; i++) {
        std::printf("[R%zu_BANK0] %08X ", i, bank_0[i]);

        if ((i % 4) == 3) {
            std::puts("");
        }
    }

    for (usize i = 0; i < NUM_BANKED_REGS; i++) {
        std::printf("[R%zu_BANK1] %08X ", i, bank_1[i]);

        if ((i % 4) == 3) {
            std::puts("");
        }
    }

    for (usize i = 8; i < NUM_REGS; i++) {
        std::printf("[R%zu%*c] %08X ", i, (i < 10) ? 6 : 5, ' ', GPRS[i]);

        if ((i % 4) == 3) {
            std::puts("");
        }
    }

    printf("[PC      ] %08X\n", CPC);
    printf("[SR      ] %08X\n", SR.raw);
}

static void swap_banks() {
    u32 temp_bank[NUM_BANKED_REGS];

    const u32 size = sizeof(u32) * NUM_BANKED_REGS;

    std::memcpy(temp_bank, GPRS, size);
    std::memcpy(GPRS, BANKED_GPRS, size);
    std::memcpy(BANKED_GPRS, temp_bank, size);
}

static void set_sr(const u32 sr) {
    const u32 old_select_bank = SR.select_bank;

    SR.raw = sr;

    if (old_select_bank != SR.select_bank) {
        swap_banks();
    }
}

static void jump(const u32 addr) {
    PC = addr;
    NPC = addr + sizeof(u16);
}

[[maybe_unused]]
static void delayed_jump(const u32 addr) {
    NPC = addr;
}

namespace ExceptionEvent {
    enum : u32 {
        Reset = 0,
    };
}

namespace ExceptionOffset {
    enum : u32 {
        Reset = 0,
    };
}

static void raise_exception(const u32 event, const u32 offset) {
    constexpr u32 RESET_VECTOR = 0xA0000000;

    std::printf("SH-4 exception @ %08X (code: %03X)\n", CPC, event);

    // Save exception context
    SPC = PC;
    SSR = SR;
    SGR = GPRS[15];

    auto new_sr = SR;

    new_sr.block_exception = 1;
    new_sr.is_privileged = 1;
    new_sr.select_bank = 1;

    set_sr(new_sr.raw);

    if (event == ExceptionEvent::Reset) {
        // Reset exception enables FPU(?)
        SR.disable_fpu = 0;
    }

    ocio::set_exception_event(event);

    if (event == ExceptionEvent::Reset) {
        jump(RESET_VECTOR);
    } else {
        jump(VBR + offset);
    }
}

namespace PrivilegedRegion {
    enum : u32 {
        P0 = 0x00000000,
        P1 = 0x80000000,
        P2 = 0xA0000000,
        P3 = 0xC0000000,
        P4 = 0xE0000000,
    };
}

constexpr u32 P0_MASK = 0x7FFFFFFF;
constexpr u32 PRIV_MASK = 0x1FFFFFFF;

template<typename T>
static T read(const u32 addr) {
    assert(SR.is_privileged);
    
    u32 masked_addr = addr & PRIV_MASK;

    if (addr < PrivilegedRegion::P1) {
        masked_addr = addr & P0_MASK;

        std::printf("Unimplemented P0 read%zu @ %08X\n", 8 * sizeof(T), masked_addr);
        exit(1);
    } else if (addr < PrivilegedRegion::P2) {
        std::printf("Unimplemented P1 read%zu @ %08X\n", 8 * sizeof(T), masked_addr);
        exit(1);
    } else if (addr < PrivilegedRegion::P3) {
        // P2, non-cacheable
        return hw::holly::bus::read<T>(masked_addr);
    } else if (addr < PrivilegedRegion::P4) {
        std::printf("Unimplemented P3 read%zu @ %08X\n", 8 * sizeof(T), masked_addr);
        exit(1);
    } else {
        return ocio::read<T>(masked_addr);
    }
}

static u16 fetch_instr() {
    // Save current PC
    CPC = PC;

    const u16 instr = read<u16>(PC);

    PC = NPC;
    NPC += sizeof(instr);

    return instr;
}

template<typename T>
static void write(const u32 addr, const T data) {
    assert(SR.is_privileged);
    
    u32 masked_addr = addr & PRIV_MASK;

    if (addr < PrivilegedRegion::P1) {
        masked_addr = addr & P0_MASK;

        std::printf("Unimplemented P0 write%zu @ %08X = %0*X\n", 8 * sizeof(T), masked_addr, (int)(2 * sizeof(T)), data);
        exit(1);
    } else if (addr < PrivilegedRegion::P2) {
        std::printf("Unimplemented P1 write%zu @ %08X = %0*X\n", 8 * sizeof(T), masked_addr, (int)(2 * sizeof(T)), data);
        exit(1);
    } else if (addr < PrivilegedRegion::P3) {
        // P2, non-cacheable
        return hw::holly::bus::write<T>(masked_addr, data);
    } else if (addr < PrivilegedRegion::P4) {
        std::printf("Unimplemented P3 write%zu @ %08X = %0*X\n", 8 * sizeof(T), masked_addr, (int)(2 * sizeof(T)), data);
        exit(1);
    } else {
        return ocio::write<T>(masked_addr, data);
    }
}

template<typename T>
static void fill_table_with_pattern(T table[], const char* pattern, T func) {
    usize mask = 0;
    usize value = 0;

    const usize length = std::strlen(pattern);

    for (usize i = 0; i < length; i++) {
        const usize shifted_bit = 1 << (length - i - 1);

        const char bit = pattern[i];
        if (bit == '0') {
            mask |= shifted_bit;
        } else if (bit == '1') {
            mask |= shifted_bit;
            value |= shifted_bit;
        }
    }

    for (usize i = 0; i < (1 << length); i++) {
        if ((i & mask) == value) {
            table[i] = func;
        }
    }
}

enum class Size {
    Byte,
    Word,
    Long,
};

static i64 i_movi(const u16 instr) {
    GPRS[N] = (i8)IMM;

    return 1;
}

template<Size size>
static i64 i_movl4(const u16 instr) {
    switch (size) {
        case Size::Byte:
            GPRS[0] = (i8)read<u8>(GPRS[M] + D);
            break;
        case Size::Word:
            GPRS[0] = (i16)read<u16>(GPRS[M] + (D << 1));
            break;
        case Size::Long:
            GPRS[N] = read<u32>(GPRS[M] + (D << 2));
            break;
    }

    return 2;
}

template<u32 amount>
static i64 i_shll(const u16 instr) {
    if constexpr (amount == 1) {
        // SHLL sets T flag
        SR.t = GPRS[N] >> 31;
    }

    GPRS[N] <<= amount;

    return 1;
}

template<u32 amount>
static i64 i_shlr(const u16 instr) {
    if constexpr (amount == 1) {
        // SHLR sets T flag
        SR.t = GPRS[N] & 1;
    }

    GPRS[N] >>= amount;

    return 1;
}

template<Size size>
static i64 i_swap(const u16 instr) {
    switch (size) {
        case Size::Word:
            // Swap 16-bit halves
            GPRS[N] = (GPRS[M] << 16) | (GPRS[M] >> 16);
            break;
    }

    return 1;
}

static i64 i_undefined(const u16 instr) {
    std::printf("Undefined SH-4 instruction %04X\n", instr);

    dump_registers();
    exit(1);
}

static void initialize_instr_table() {
    ctx.instr_table.fill(i_undefined);

    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000000", i_shll<1>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000001", i_shlr<1>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001000", i_shll<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001001", i_shlr<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011000", i_shll<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011001", i_shlr<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101000", i_shll<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101001", i_shlr<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0101xxxxxxxxxxxx", i_movl4<Size::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1001", i_swap<Size::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000100xxxxxxxx", i_movl4<Size::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000101xxxxxxxx", i_movl4<Size::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "1110xxxxxxxxxxxx", i_movi);
}

void initialize() {
    ocio::initialize();

    SR.interrupt_mask = 0xF;

    raise_exception(ExceptionEvent::Reset, ExceptionOffset::Reset);

    initialize_instr_table();
}

void reset() {
    ocio::reset();

    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    ocio::shutdown();
}

void step() {
    while (ctx.cycles > 0) {
        const u16 instr = fetch_instr();
        
        ctx.cycles -= ctx.instr_table[instr](instr);
    }
}

i64* get_cycles() {
    return &ctx.cycles;
}

}
