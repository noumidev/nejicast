/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/cpu.hpp>

#include <array>
#include <bit>
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
#define MACH        ctx.mach
#define MACL        ctx.macl

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

    // Multiplication results
    u32 mach, macl;

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

    printf("[PC      ] %08X [SPC     ] %08X\n", CPC, SPC);
    printf("[SR      ] %08X [SSR     ] %08X [SGR     ] %08X\n", SR.raw, SSR.raw, SGR);
    printf("[GBR     ] %08X [VBR     ] %08X [DBR     ] %08X\n", GBR, VBR, DBR);
    printf("[MACH    ] %08X [MACL    ] %08X\n", MACH, MACL);
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
    std::printf("SH-4 jump @ %08X to %08X\n", CPC, addr);

    PC = addr;
    NPC = addr + sizeof(u16);
}

static void delayed_jump(const u32 addr) {
    std::printf("SH-4 delayed jump @ %08X to %08X\n", CPC, addr);

    NPC = addr;
}

enum class SystemRegister {
    Macl,
};

template<SystemRegister system_register>
u32 get_system_register() {
    switch (system_register) {
        case SystemRegister::Macl:
            return MACL;
    }
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
        // P1, cacheable
        // TODO: implement caches?
        return hw::holly::bus::read<T>(masked_addr);
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
        // P1, cacheable
        // TODO: implement caches?
        return hw::holly::bus::write<T>(masked_addr, data);
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

enum class AddressingMode {
    Immediate,
    RegisterDirect,
    RegisterIndirectGbr,
    RegisterIndirectPredecrement,
};

enum class OperandSize {
    Byte,
    Word,
    Long,
};

template<bool is_immediate>
static i64 i_add(const u16 instr) {
    if constexpr (is_immediate) {
        GPRS[N] += (i8)IMM;
    } else {
        GPRS[N] += GPRS[M];
    }

    return 1;
}

template<bool is_delayed>
static i64 i_bf(const u16 instr) {
    if (SR.t != 0) {
        return 1;
    }

    const u32 jump_target = PC + sizeof(u16) + ((u32)(i8)IMM << 1);

    if constexpr (is_delayed) {
        delayed_jump(jump_target);
    } else {
        jump(jump_target);
    }

    return 2;
}

enum class Comparison {
    Equal,
    EqualImmediate,
    GreaterThan,
    GreaterEqual,
    Higher,
    HigherSame,
    Positive,
    PositiveZero,
    String,
};

template<Comparison comparison>
static i64 i_cmp(const u16 instr) {
    switch (comparison) {
        case Comparison::Equal:
            SR.t = GPRS[N] == GPRS[M];
            break;
        case Comparison::EqualImmediate:
            SR.t = GPRS[0] == (u32)(i8)IMM;
            break;
        case Comparison::GreaterThan:
            SR.t = (i32)GPRS[N] > (i32)GPRS[M];
            break;
        case Comparison::GreaterEqual:
            SR.t = (i32)GPRS[N] >= (i32)GPRS[M];
            break;
        case Comparison::Higher:
            SR.t = GPRS[N] > GPRS[M];
            break;
        case Comparison::HigherSame:
            SR.t = GPRS[N] >= GPRS[M];
            break;
        case Comparison::Positive:
            SR.t = (i32)GPRS[N] > 0;
            break;
        case Comparison::PositiveZero:
            SR.t = (i32)GPRS[N] >= 0;
            break;
        case Comparison::String:
            {
                const u32 temp = GPRS[N] ^ GPRS[M];

                u32 result = temp & 0xFF;

                result &= (temp >>  8) & 0xFF;
                result &= (temp >> 16) & 0xFF;
                result &= (temp >> 24) & 0xFF;

                SR.t = result == 0;
                break;
            }
    }

    return 1;
}

static i64 i_jmp(const u16 instr) {
    delayed_jump(GPRS[N]);

    return 3;
}

static i64 i_mov(const u16 instr) {
    GPRS[N] = GPRS[M];

    return 1;
}

static i64 i_movi(const u16 instr) {
    GPRS[N] = (i8)IMM;

    return 1;
}

template<OperandSize size>
static i64 i_movl4(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[0] = (i8)read<u8>(GPRS[M] + D);
            break;
        case OperandSize::Word:
            GPRS[0] = (i16)read<u16>(GPRS[M] + (D << 1));
            break;
        case OperandSize::Long:
            GPRS[N] = read<u32>(GPRS[M] + (D << 2));
            break;
    }

    return 2;
}

template<OperandSize size>
static i64 i_movs(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            write<u8>(GPRS[N], GPRS[M]);
            break;
        case OperandSize::Word:
            write<u16>(GPRS[N], GPRS[M]);
            break;
        case OperandSize::Long:
            write<u32>(GPRS[N], GPRS[M]);
            break;
    }

    return 1;
}

template<OperandSize size>
static i64 i_movs4(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            write<u8>(GPRS[M] + D, GPRS[0]);
            break;
        case OperandSize::Word:
            write<u16>(GPRS[M] + (D << 1), GPRS[0]);
            break;
        case OperandSize::Long:
            // Different from MOVLL4
            write<u32>(GPRS[N] + (D << 2), GPRS[M]);
            break;
    }

    return 2;
}

static i64 i_mulu(const u16 instr) {
    MACL = (u32)(u16)GPRS[N] * (u32)(u16)GPRS[M];

    return 4;
}

static i64 i_nop(const u16) {
    return 1;
}

template<AddressingMode mode>
static i64 i_or(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        GPRS[N] |= GPRS[M];
    } else if constexpr (mode == AddressingMode::Immediate) {
        GPRS[0] |= IMM;
    } else {
        write<u8>(GPRS[0] + GBR, read<u8>(GPRS[0] + GBR) | IMM);

        return 4;
    }

    return 1;
}

static i64 i_pref(const u16 instr) {
    // TODO: implement operand cache?

    std::printf("SH-4 operand cache prefetch @ %08X\n", GPRS[N]);

    return 1;
}

static i64 i_rotr(const u16 instr) {
    SR.t = GPRS[N] & 1;

    GPRS[N] = std::rotr(GPRS[N], 1);

    return 1;
}

static i64 i_shar(const u16 instr) {
    SR.t = GPRS[N] & 1;

    GPRS[N] = (i32)GPRS[N] >> 1;

    return 1;
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

template <SystemRegister system_register, AddressingMode mode>
i64 i_sts(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        GPRS[N] = get_system_register<system_register>();
    } else {
        // Register indirect pre-decrement
        GPRS[N] -= sizeof(u32);

        write<u32>(GPRS[N], get_system_register<system_register>());
    }

    // Depends, but is good enough for now
    return 2;
}

template<OperandSize size>
static i64 i_swap(const u16 instr) {
    switch (size) {
        case OperandSize::Word:
            // Swap 16-bit halves
            GPRS[N] = (GPRS[M] << 16) | (GPRS[M] >> 16);
            break;
    }

    return 1;
}

template<AddressingMode mode>
static i64 i_tst(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        SR.t = (GPRS[N] & GPRS[M]) == 0;
    } else if constexpr (mode == AddressingMode::Immediate) {
        SR.t = (GPRS[0] & IMM) == 0;
    } else {
        SR.t = (read<u8>(GPRS[0] + GBR) & IMM) == 0;

        return 3;
    }

    return 1;
}

static i64 i_undefined(const u16 instr) {
    std::printf("Undefined SH-4 instruction %04X\n", instr);

    dump_registers();
    exit(1);
}

template<AddressingMode mode>
static i64 i_xor(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        GPRS[N] ^= GPRS[M];
    } else if constexpr (mode == AddressingMode::Immediate) {
        GPRS[0] ^= IMM;
    } else {
        write<u8>(GPRS[0] + GBR, read<u8>(GPRS[0] + GBR) ^ IMM);

        return 4;
    }

    return 1;
}

static void initialize_instr_table() {
    ctx.instr_table.fill(i_undefined);

    fill_table_with_pattern(ctx.instr_table.data(), "0000000000001001", i_nop);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00011010", i_sts<SystemRegister::Macl, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx10000011", i_pref);
    fill_table_with_pattern(ctx.instr_table.data(), "0001xxxxxxxxxxxx", i_movs4<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0000", i_movs<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0001", i_movs<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0010", i_movs<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1000", i_tst<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1010", i_xor<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1011", i_or<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1100", i_cmp<Comparison::String>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1110", i_mulu);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0000", i_cmp<Comparison::Equal>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0010", i_cmp<Comparison::HigherSame>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0011", i_cmp<Comparison::GreaterEqual>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0110", i_cmp<Comparison::Higher>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0111", i_cmp<Comparison::GreaterThan>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx1100", i_add<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000000", i_shll<1>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000001", i_shlr<1>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000101", i_rotr);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001000", i_shll<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001001", i_shlr<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010001", i_cmp<Comparison::PositiveZero>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010010", i_sts<SystemRegister::Macl, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010101", i_cmp<Comparison::Positive>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011000", i_shll<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011001", i_shlr<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100001", i_shar);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101000", i_shll<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101001", i_shlr<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101011", i_jmp);
    fill_table_with_pattern(ctx.instr_table.data(), "0101xxxxxxxxxxxx", i_movl4<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0011", i_mov);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1001", i_swap<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0111xxxxxxxxxxxx", i_add<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000000xxxxxxxx", i_movs4<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000100xxxxxxxx", i_movl4<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000001xxxxxxxx", i_movs4<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000101xxxxxxxx", i_movl4<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001000xxxxxxxx", i_cmp<Comparison::EqualImmediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001011xxxxxxxx", i_bf<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001111xxxxxxxx", i_bf<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001000xxxxxxxx", i_tst<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001010xxxxxxxx", i_xor<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001011xxxxxxxx", i_or<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001100xxxxxxxx", i_tst<AddressingMode::RegisterIndirectGbr>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001110xxxxxxxx", i_xor<AddressingMode::RegisterIndirectGbr>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001111xxxxxxxx", i_or<AddressingMode::RegisterIndirectGbr>);
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
