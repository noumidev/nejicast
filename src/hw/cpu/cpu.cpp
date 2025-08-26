/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/cpu.hpp>

#include <algorithm>
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
#define IMM  get_bits(instr, 0,  7)
#define DISP get_bits(instr, 0, 11)
#define D    get_bits(instr, 0,  3)
#define M    get_bits(instr, 4,  7)
#define N    get_bits(instr, 8, 11)

static inline u32 get_mask(const u32 start, const u32 end) {
    assert(start <= end);

    return (UINT32_MAX << start) & (UINT32_MAX >> ((8 * sizeof(u32) - 1) - end));
}

static inline u32 get_bits(const u32 n, const u32 start, const u32 end) {
    return (n & get_mask(start, end)) >> start;
}

// Register file macros
#define PC          ctx.pc
#define PC_DELAY    (ctx.pc + sizeof(u16))
#define CPC         ctx.current_pc
#define NPC         ctx.next_pc
#define SPC         ctx.spc
#define PR          ctx.pr
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
#define FPSCR       ctx.fpscr
#define FPUL        ctx.fpul
#define FR          ctx.fprs.fr
#define DR          ctx.fprs.dr
#define XR          ctx.banked_fprs.fr
#define XD          ctx.banked_fprs.dr
#define FR_RAW      ctx.fprs.fr_raw
#define DR_RAW      ctx.fprs.dr_raw
#define XR_RAW      ctx.banked_fprs.fr_raw
#define XD_RAW      ctx.banked_fprs.dr_raw

constexpr usize NUM_REGS = 16;
constexpr usize NUM_BANKED_REGS = 8;

constexpr usize NUM_FPRS = 16;
constexpr usize NUM_DRS = 8;

constexpr usize INSTR_TABLE_SIZE = 0x10000;

struct {
    // PC and delay slot helpers
    u32 pc, current_pc, next_pc;
    u32 spc, pr;

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

    union {
        u32 raw;

        struct {
            u32 rounding_mode        :  2;
            u32 flag                 :  5;
            u32 enable               :  5;
            u32 cause                :  6;
            u32 denormalization_mode :  1;
            u32 precision_mode       :  1;
            u32 pair_mode            :  1;
            u32 select_bank          :  1;
            u32                      : 10;
        };
    } fpscr;

    u32 fpul;
    
    union {
        u32 fr_raw[NUM_FPRS];
        f32 fr[NUM_FPRS];
        u64 dr_raw[NUM_DRS];
        f64 dr[NUM_DRS];
    } fprs, banked_fprs;

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

    bank_0 = (FPSCR.select_bank) ? XR_RAW : FR_RAW; 
    bank_1 = (FPSCR.select_bank) ? FR_RAW : XR_RAW;

    for (usize i = 0; i < NUM_REGS; i++) {
        std::printf("[FR%zu%*c] %08X ", i, (i < 10) ? 5 : 4, ' ', bank_0[i]);

        if ((i % 4) == 3) {
            std::puts("");
        }
    }

    for (usize i = 0; i < NUM_REGS; i++) {
        std::printf("[XR%zu%*c] %08X ", i, (i < 10) ? 5 : 4, ' ', bank_1[i]);

        if ((i % 4) == 3) {
            std::puts("");
        }
    }

    printf("[PC      ] %08X [SPC     ] %08X [PR      ] %08X\n", CPC, SPC, PR);
    printf("[SR      ] %08X [SSR     ] %08X [SGR     ] %08X\n", SR.raw, SSR.raw, SGR);
    printf("[GBR     ] %08X [VBR     ] %08X [DBR     ] %08X\n", GBR, VBR, DBR);
    printf("[MACH    ] %08X [MACL    ] %08X\n", MACH, MACL);
    printf("[FPSCR   ] %08X [FPUL    ] %08X\n", FPSCR.raw, FPUL);
}

static void swap_banks() {
    u32 temp_bank[NUM_BANKED_REGS];

    const u32 size = sizeof(u32) * NUM_BANKED_REGS;

    std::memcpy(temp_bank, GPRS, size);
    std::memcpy(GPRS, BANKED_GPRS, size);
    std::memcpy(BANKED_GPRS, temp_bank, size);
}

static void swap_fpu_banks() {
    std::swap(ctx.fprs, ctx.banked_fprs);
}

static void set_sr(const u32 sr) {
    const u32 old_select_bank = SR.select_bank;

    SR.raw = sr;

    if (old_select_bank != SR.select_bank) {
        swap_banks();
    }
}

static void set_fpscr(const u32 fpscr) {
    const u32 old_select_bank = FPSCR.select_bank;

    FPSCR.raw = fpscr;

    if (old_select_bank != FPSCR.select_bank) {
        swap_fpu_banks();
    }
}

static void jump(const u32 addr) {
    // std::printf("SH-4 jump @ %08X to %08X\n", CPC, addr);

    PC = addr;
    NPC = addr + sizeof(u16);
}

static void delayed_jump(const u32 addr) {
    // std::printf("SH-4 delayed jump @ %08X to %08X\n", CPC, addr);

    NPC = addr;
}

enum class ControlRegister {
    Dbr,
    Gbr,
    Spc,
    Sr,
    Ssr,
    Vbr,
};

template<ControlRegister control_register>
u32 get_control_register() {
    switch (control_register) {
        case ControlRegister::Dbr:
            return DBR;
        case ControlRegister::Gbr:
            return GBR;
        case ControlRegister::Spc:
            return SPC;
        case ControlRegister::Sr:
            return SR.raw;
        case ControlRegister::Ssr:
            return SSR.raw;
        case ControlRegister::Vbr:
            return VBR;
    }
}

template<ControlRegister control_register>
void set_control_register(const u32 data) {
    switch (control_register) {
        case ControlRegister::Dbr:
            DBR = data;
            break;
        case ControlRegister::Gbr:
            GBR = data;
            break;
        case ControlRegister::Spc:
            SPC = data;
            break;
        case ControlRegister::Sr:
            set_sr(data);
            break;
        case ControlRegister::Ssr:
            SSR.raw = data;
            break;
        case ControlRegister::Vbr:
            VBR = data;
            break;
    }
}

enum class SystemRegister {
    Fpscr,
    Fpul,
    Mach,
    Macl,
    Pr,
};

template<SystemRegister system_register>
u32 get_system_register() {
    switch (system_register) {
        case SystemRegister::Fpscr:
            return FPSCR.raw;
        case SystemRegister::Fpul:
            return FPUL;
        case SystemRegister::Mach:
            return MACH;
        case SystemRegister::Macl:
            return MACL;
        case SystemRegister::Pr:
            return PR;
    }
}

template<SystemRegister system_register>
void set_system_register(const u32 data) {
    switch (system_register) {
        case SystemRegister::Fpscr:
            set_fpscr(data);
            break;
        case SystemRegister::Fpul:
            FPUL = data;
            break;
        case SystemRegister::Mach:
            MACH = data;
            break;
        case SystemRegister::Macl:
            MACL = data;
            break;
        case SystemRegister::Pr:
            PR = data;
            break;
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

enum : u32 {
    REGION_P0 = 0x00000000,
    REGION_P1 = 0x80000000,
    REGION_P2 = 0xA0000000,
    REGION_P3 = 0xC0000000,
    REGION_P4 = 0xE0000000,
};

constexpr u32 P0_MASK = 0x7FFFFFFF;
constexpr u32 PRIV_MASK = 0x1FFFFFFF;

template<typename T>
static T read(const u32 addr) {
    assert(SR.is_privileged);
    
    u32 masked_addr = addr & PRIV_MASK;

    if (addr < REGION_P1) {
        masked_addr = addr & P0_MASK;

        std::printf("Unimplemented P0 read%zu @ %08X\n", 8 * sizeof(T), masked_addr);
        exit(1);
    } else if (addr < REGION_P2) {
        // P1, cacheable
        // TODO: implement caches?
        return hw::holly::bus::read<T>(masked_addr);
    } else if (addr < REGION_P3) {
        // P2, non-cacheable
        return hw::holly::bus::read<T>(masked_addr);
    } else if (addr < REGION_P4) {
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

    if (addr < REGION_P1) {
        masked_addr = addr & P0_MASK;

        std::printf("Unimplemented P0 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), masked_addr, (int)(2 * sizeof(T)), (u64)data);
        exit(1);
    } else if (addr < REGION_P2) {
        // P1, cacheable
        // TODO: implement caches?
        return hw::holly::bus::write<T>(masked_addr, data);
    } else if (addr < REGION_P3) {
        // P2, non-cacheable
        return hw::holly::bus::write<T>(masked_addr, data);
    } else if (addr < REGION_P4) {
        std::printf("Unimplemented P3 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), masked_addr, (int)(2 * sizeof(T)), (u64)data);
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
    RegisterIndirectPostincrement,
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

template<AddressingMode mode>
static i64 i_and(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        GPRS[N] &= GPRS[M];
    } else if constexpr (mode == AddressingMode::Immediate) {
        GPRS[0] &= IMM;
    } else {
        write<u8>(GPRS[0] + GBR, read<u8>(GPRS[0] + GBR) & IMM);

        return 4;
    }

    return 1;
}

template<bool is_delayed>
static i64 i_bf(const u16 instr) {
    if (SR.t != 0) {
        return 1;
    }

    const u32 jump_target = PC_DELAY + ((u32)(i8)IMM << 1);

    if constexpr (is_delayed) {
        delayed_jump(jump_target);
    } else {
        jump(jump_target);
    }

    return 2;
}

template<bool is_linked, bool is_displacement>
static i64 i_bra(const u16 instr) {
    if constexpr (is_linked) {
        PR = PC_DELAY;
    }

    u32 offset;

    if constexpr (is_displacement) {
        offset = ((i32)(DISP << 20) >> 19);
    } else {
        offset = GPRS[N];
    }

    delayed_jump(PC_DELAY + offset);

    return (is_displacement) ? 2 : 3;
}

template<bool is_delayed>
static i64 i_bt(const u16 instr) {
    if (SR.t == 0) {
        return 1;
    }

    const u32 jump_target = PC_DELAY + ((u32)(i8)IMM << 1);

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

static i64 i_dt(const u16 instr) {
    GPRS[N] -= 1;

    SR.t = GPRS[N] == 0;

    return 1;
}

static i64 i_fmov(const u16 instr) {
    if (FPSCR.pair_mode) {
        u64 data = DR_RAW[M >> 1];

        if ((M & 1) != 0) {
            data = XD_RAW[M >> 1];
        }

        if ((N & 1) != 0) {
            XD_RAW[N >> 1] = data;
        } else {
            DR_RAW[N >> 1] = data;
        }
    } else {
        FR_RAW[N] = FR_RAW[M];
    }

    return 1;
}

static i64 i_fmov_restore(const u16 instr) {
    if (FPSCR.pair_mode) {
        if ((N & 1) != 0) {
            XD_RAW[N >> 1] = read<u64>(GPRS[M]);
        } else {
            DR_RAW[N >> 1] = read<u64>(GPRS[M]);
        }

        GPRS[M] += sizeof(u64);
    } else {
        FR_RAW[N] = read<u32>(GPRS[M]);

        GPRS[M] += sizeof(u32);
    }

    return 1;
}

static i64 i_fmov_save(const u16 instr) {
    if (FPSCR.pair_mode) {
        GPRS[N] -= sizeof(u64);

        if ((M & 1) != 0) {
            write<u64>(GPRS[N], XD_RAW[M >> 1]);
        } else {
            write<u64>(GPRS[N], DR_RAW[M >> 1]);
        }
    } else {
        GPRS[N] -= sizeof(u32);

        write<u32>(GPRS[N], FR_RAW[M]);
    }

    return 1;
}

static i64 i_frchg(const u16) {
    assert(!FPSCR.precision_mode);

    FPSCR.select_bank ^= 1;

    swap_fpu_banks();

    return 1;
}

static i64 i_jmp(const u16 instr) {
    delayed_jump(GPRS[N]);

    return 3;
}

static i64 i_jsr(const u16 instr) {
    PR = PC_DELAY;

    delayed_jump(GPRS[N]);

    return 3;
}

template<ControlRegister control_register, AddressingMode mode>
static i64 i_ldc(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        set_control_register<control_register>(GPRS[N]);
    } else {
        set_control_register<control_register>(read<u32>(GPRS[N]));

        GPRS[N] += sizeof(u32);
    }

    // See sts
    return 2;
}

template<SystemRegister system_register, AddressingMode mode>
static i64 i_lds(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        set_system_register<system_register>(GPRS[N]);
    } else {
        set_system_register<system_register>(read<u32>(GPRS[N]));

        GPRS[N] += sizeof(u32);
    }

    // See sts
    return 2;
}

static i64 i_mov(const u16 instr) {
    GPRS[N] = GPRS[M];

    return 1;
}

static i64 i_mova(const u16 instr) {
    GPRS[0] = (PC_DELAY & ~3) + (IMM << 2);

    return 1;
}

template<OperandSize size>
static i64 i_movi(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[N] = (i8)IMM;
            return 1;
        case OperandSize::Word:
            GPRS[N] = (i16)read<u16>(PC_DELAY + (IMM << 1));
            break;
        case OperandSize::Long:
            GPRS[N] = read<u32>((PC_DELAY & ~3) + (IMM << 2));
            break;
    }

    return 2;
}

template<OperandSize size>
static i64 i_movl(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[N] = (i8)read<u8>(GPRS[M]);
            break;
        case OperandSize::Word:
            GPRS[N] = (i16)read<u16>(GPRS[M]);
            break;
        case OperandSize::Long:
            GPRS[N] = read<u32>(GPRS[M]);
            break;
    }

    return 1;
}

template<OperandSize size>
static i64 i_movl0(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[N] = (i8)read<u8>(GPRS[0] + GPRS[M]);
            break;
        case OperandSize::Word:
            GPRS[N] = (i16)read<u16>(GPRS[0] + GPRS[M]);
            break;
        case OperandSize::Long:
            GPRS[N] = read<u32>(GPRS[0] + GPRS[M]);
            break;
    }

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
static i64 i_movm(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            write<u8>(GPRS[N] - sizeof(u8), GPRS[M]);

            GPRS[N]--;
            break;
        case OperandSize::Word:
            write<u16>(GPRS[N] - sizeof(u16), GPRS[M]);

            GPRS[N] -= sizeof(u16);
            break;
        case OperandSize::Long:
            write<u32>(GPRS[N] - sizeof(u32), GPRS[M]);

            GPRS[N] -= sizeof(u32);
            break;
    }

    return 1;
}

template<OperandSize size>
static i64 i_movp(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[N] = (i8)read<u8>(GPRS[M]);

            if (N != M) {
                GPRS[M]++;
            }
            break;
        case OperandSize::Word:
            GPRS[N] = (i16)read<u16>(GPRS[M]);

            if (N != M) {
                GPRS[M] += sizeof(u16);
            }
            break;
        case OperandSize::Long:
            GPRS[N] = read<u32>(GPRS[M]);

            if (N != M) {
                GPRS[M] += sizeof(u32);
            }
            break;
    }

    return 1;
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

static i64 i_rts(const u16) {
    delayed_jump(PR);

    return 3;
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

template <ControlRegister control_register, AddressingMode mode>
i64 i_stc(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        GPRS[N] = get_control_register<control_register>();
    } else {
        // Register indirect pre-decrement
        GPRS[N] -= sizeof(u32);

        write<u32>(GPRS[N], get_control_register<control_register>());
    }

    // See sts
    return 2;
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
        case OperandSize::Byte:
            // Swap low 8-bit halves
            GPRS[N] = (GPRS[M] & ~0xFFFF) | ((GPRS[M] >> 8) & 0xFF) | ((GPRS[M] << 8) & 0xFF00);
            break;
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

    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00000011", i_bra<true, false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000001001", i_nop);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000001011", i_rts);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx1100", i_movl0<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx1101", i_movl0<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx1110", i_movl0<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00011010", i_sts<SystemRegister::Macl, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00100011", i_bra<false, false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00101010", i_sts<SystemRegister::Pr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx10000011", i_pref);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx11111010", i_stc<ControlRegister::Dbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0001xxxxxxxxxxxx", i_movs4<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0000", i_movs<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0001", i_movs<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0010", i_movs<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0100", i_movm<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0101", i_movm<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0110", i_movm<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1000", i_tst<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1001", i_and<AddressingMode::RegisterDirect>);
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
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000110", i_lds<SystemRegister::Mach, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000111", i_ldc<ControlRegister::Sr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001000", i_shll<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001001", i_shlr<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001010", i_lds<SystemRegister::Mach, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001011", i_jsr);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001110", i_ldc<ControlRegister::Sr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010000", i_dt);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010001", i_cmp<Comparison::PositiveZero>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010010", i_sts<SystemRegister::Macl, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010101", i_cmp<Comparison::Positive>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010110", i_lds<SystemRegister::Macl, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010111", i_ldc<ControlRegister::Gbr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011000", i_shll<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011001", i_shlr<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011010", i_lds<SystemRegister::Macl, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011110", i_ldc<ControlRegister::Gbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100001", i_shar);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100010", i_sts<SystemRegister::Pr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100110", i_lds<SystemRegister::Pr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100111", i_ldc<ControlRegister::Vbr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101000", i_shll<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101001", i_shlr<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101010", i_lds<SystemRegister::Pr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101011", i_jmp);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101110", i_ldc<ControlRegister::Vbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00110111", i_ldc<ControlRegister::Ssr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00111110", i_ldc<ControlRegister::Ssr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01000111", i_ldc<ControlRegister::Spc, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01001110", i_ldc<ControlRegister::Spc, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01010110", i_lds<SystemRegister::Fpul, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01011010", i_lds<SystemRegister::Fpul, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01100110", i_lds<SystemRegister::Fpscr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01101010", i_lds<SystemRegister::Fpscr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx11110010", i_stc<ControlRegister::Dbr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx11110110", i_ldc<ControlRegister::Dbr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx11111010", i_ldc<ControlRegister::Dbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0101xxxxxxxxxxxx", i_movl4<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0000", i_movl<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0001", i_movl<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0010", i_movl<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0011", i_mov);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0100", i_movp<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0101", i_movp<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0110", i_movp<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1000", i_swap<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1001", i_swap<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0111xxxxxxxxxxxx", i_add<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000000xxxxxxxx", i_movs4<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000100xxxxxxxx", i_movl4<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000001xxxxxxxx", i_movs4<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "10000101xxxxxxxx", i_movl4<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001000xxxxxxxx", i_cmp<Comparison::EqualImmediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001001xxxxxxxx", i_bt<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001011xxxxxxxx", i_bf<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001101xxxxxxxx", i_bt<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "10001111xxxxxxxx", i_bf<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "1001xxxxxxxxxxxx", i_movi<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "1010xxxxxxxxxxxx", i_bra<false, true>);
    fill_table_with_pattern(ctx.instr_table.data(), "1011xxxxxxxxxxxx", i_bra<true, true>);
    fill_table_with_pattern(ctx.instr_table.data(), "11000111xxxxxxxx", i_mova);
    fill_table_with_pattern(ctx.instr_table.data(), "11001000xxxxxxxx", i_tst<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001001xxxxxxxx", i_and<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001010xxxxxxxx", i_xor<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001011xxxxxxxx", i_or<AddressingMode::Immediate>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001100xxxxxxxx", i_tst<AddressingMode::RegisterIndirectGbr>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001101xxxxxxxx", i_and<AddressingMode::RegisterIndirectGbr>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001110xxxxxxxx", i_xor<AddressingMode::RegisterIndirectGbr>);
    fill_table_with_pattern(ctx.instr_table.data(), "11001111xxxxxxxx", i_or<AddressingMode::RegisterIndirectGbr>);
    fill_table_with_pattern(ctx.instr_table.data(), "1101xxxxxxxxxxxx", i_movi<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "1110xxxxxxxxxxxx", i_movi<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1001", i_fmov_restore);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1011", i_fmov_save);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1100", i_fmov);
    fill_table_with_pattern(ctx.instr_table.data(), "1111101111111101", i_frchg);
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
