/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/cpu.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

#include <hw/cpu/ccn.hpp>
#include <hw/cpu/ocio.hpp>
#include <hw/cpu/tmu.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/intc.hpp>

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
#define FV          &ctx.fprs.fr
#define XR          ctx.banked_fprs.fr
#define FR_RAW      ctx.fprs.fr_raw
#define XR_RAW      ctx.banked_fprs.fr_raw

constexpr usize NUM_REGS = 16;
constexpr usize NUM_BANKED_REGS = 8;

constexpr usize NUM_FPRS = 16;

constexpr usize INSTR_TABLE_SIZE = 0x10000;

enum {
    STATE_RUNNING,
    STATE_SLEEPING,
};

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
    } fprs, banked_fprs;

    std::array<i64(*)(const u16), INSTR_TABLE_SIZE> instr_table;

    int state;

    u16 pending_interrupts;

    i64 cycles;
} ctx;

static void set_state(const int state) {
    ctx.state = state;
}

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

static f64 get_dr(const usize n) {
    assert(n < NUM_FPRS);

    const u32* fr = (n & 1) ? &XR_RAW[n & ~1] : &FR_RAW[n & ~1];

    f64 data;

    std::memcpy((u8*)&data + 4, fr + 0,sizeof(u32));
    std::memcpy((u8*)&data + 0, fr + 1,sizeof(u32));

    return data;
}

static u64 get_dr_move(const usize n) {
    const u32* fr = (n & 1) ? &XR_RAW[n & ~1] : &FR_RAW[n & ~1];

    u64 data;

    std::memcpy(&data, fr, sizeof(data));

    return data;
}

static u64 get_dr_raw(const usize n) {
    assert(n < NUM_FPRS);

    const u32* fr = (n & 1) ? &XR_RAW[n & ~1] : &FR_RAW[n & ~1];

    u64 data;

    std::memcpy((u8*)&data + 4, fr + 0,sizeof(u32));
    std::memcpy((u8*)&data + 0, fr + 1,sizeof(u32));

    return data;
}

static void set_dr(const usize n, const f64 data) {
    assert(n < NUM_FPRS);

    u32* fr = (n & 1) ? &XR_RAW[n & ~1] : &FR_RAW[n & ~1];

    std::memcpy(fr + 0, (u8*)&data + 4, sizeof(u32));
    std::memcpy(fr + 1, (u8*)&data + 0, sizeof(u32));
}

static void set_dr_raw(const usize n, const u64 data) {
    assert(n < NUM_FPRS);

    u32* fr = (n & 1) ? &XR_RAW[n & ~1] : &FR_RAW[n & ~1];

    std::memcpy(fr + 0, (u8*)&data + 4, sizeof(u32));
    std::memcpy(fr + 1, (u8*)&data + 0, sizeof(u32));
}

static void set_dr_move(const usize n, const u64 data) {
    u32* fr = (n & 1) ? &XR_RAW[n & ~1] : &FR_RAW[n & ~1];

    std::memcpy(fr, &data, sizeof(data));
}

[[maybe_unused]]
static void add_jump_target(const u32 addr) {
    static std::unordered_set<u32> jump_targets;

    if (jump_targets.find(addr) == jump_targets.end()) {
        std::printf("Jump @ %08X to %08X\n", CPC, addr);

        jump_targets.insert(addr);
    }
}

static void jump(const u32 addr) {
    // std::printf("SH-4 jump @ %08X to %08X\n", CPC, addr);

    PC = addr;
    NPC = addr + sizeof(u16);

    // add_jump_target(addr);
}

static void delayed_jump(const u32 addr) {
    // std::printf("SH-4 delayed jump @ %08X to %08X\n", CPC, addr);

    NPC = addr;

    // add_jump_target(addr);
}

enum class ControlRegister {
    Dbr,
    Gbr,
    Rbank,
    Spc,
    Sr,
    Ssr,
    Vbr,
};

template<ControlRegister control_register>
u32 get_control_register(const usize idx = 0) {
    switch (control_register) {
        case ControlRegister::Dbr:
            return DBR;
        case ControlRegister::Gbr:
            return GBR;
        case ControlRegister::Rbank:
            assert(idx < NUM_BANKED_REGS);

            return BANKED_GPRS[idx];
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
void set_control_register(const u32 data, const usize idx = 0) {
    switch (control_register) {
        case ControlRegister::Dbr:
            DBR = data;
            break;
        case ControlRegister::Gbr:
            GBR = data;
            break;
        case ControlRegister::Rbank:
            assert(idx < NUM_BANKED_REGS);

            BANKED_GPRS[idx] = data;
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
        ExternalInterrupt = 0x200,
    };
}

namespace ExceptionOffset {
    enum : u32 {
        Reset = 0,
        ExternalInterrupt = 0x600,
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

    ocio::ccn::set_exception_event(event);

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

static i64 i_addc(const u16 instr) {
    const u64 result = (u64)GPRS[N] + (u64)GPRS[M] + (u64)SR.t;

    SR.t = result >= UINT32_MAX;

    GPRS[N] = (u32)result;

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

static i64 i_clrs(const u16) {
    SR.saturate_mac = 0;

    return 1;
}

static i64 i_clrt(const u16) {
    SR.t = 0;

    return 1;
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

template<bool is_signed>
static i64 i_div0(const u16 instr) {
    if constexpr (is_signed) {
        SR.m = GPRS[N] >> 31;
        SR.q = GPRS[M] >> 31;
        SR.t = SR.m ^ SR.q;
    } else {
        SR.m = 0;
        SR.q = 0;
        SR.t = 0;
    }

    return 1;
}

static i64 i_div1(const u16 instr) {
    const u32 old_n = GPRS[N];
    const u32 old_q = SR.q;

    SR.q = GPRS[N] >> 31;

    GPRS[N] = (GPRS[N] << 1) | SR.t;

    u32 new_q;

    if (old_q == SR.m) {
        GPRS[N] -= GPRS[M];

        new_q = GPRS[N] > old_n;
    } else {
        GPRS[N] += GPRS[M];

        new_q = GPRS[N] < old_n;
    }

    SR.q ^= new_q;

    if (SR.m != 0) {
        SR.q ^= 1;
    }

    SR.t = SR.q == SR.m;

    return 1;
}

static i64 i_dmulu(const u16 instr) {
    const u64 result = (u64)GPRS[N] * (u64)GPRS[M];

    MACH = result >> 32;
    MACL = result;

    return 4;
}

static i64 i_dmuls(const u16 instr) {
    const u64 result = (i64)(i32)GPRS[N] * (i64)(i32)GPRS[M];

    MACH = result >> 32;
    MACL = result;

    return 4;
}

static i64 i_dt(const u16 instr) {
    GPRS[N] -= 1;

    SR.t = GPRS[N] == 0;

    return 1;
}

template<OperandSize size>
static i64 i_exts(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[N] = (i8)GPRS[M];
            break;
        case OperandSize::Word:
            GPRS[N] = (i16)GPRS[M];
            break;
    }

    return 1;
}

template<OperandSize size>
static i64 i_extu(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[N] = (u8)GPRS[M];
            break;
        case OperandSize::Word:
            GPRS[N] = (u16)GPRS[M];
            break;
    }

    return 1;
}

static i64 i_fabs(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert((N & 1) == 0);

        set_dr_raw(N, get_dr_raw(N) & ~(1LL << 63));
    } else {
        FR_RAW[N] &= ~(1 << 31);
    }

    return 1;
}

static i64 i_fadd(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert(((M & 1) == 0) && ((N & 1) == 0));

        set_dr(N, get_dr(N) + get_dr(M));
    } else {
        FR[N] += FR[M];
    }

    return 3 + (4 * FPSCR.precision_mode);
}

template<Comparison comparison>
static i64 i_fcmp(const u16 instr) {
    assert(!FPSCR.precision_mode || (((N & 1) == 0) && ((M & 1) == 0)));

    switch (comparison) {
        case Comparison::Equal:
            if (FPSCR.precision_mode) {
                SR.t = get_dr(N) == get_dr(M);
            } else {
                SR.t = FR[N] == FR[M];
            }
            break;
        case Comparison::GreaterThan:
            if (FPSCR.precision_mode) {
                SR.t = get_dr(N) > get_dr(M);
            } else {
                SR.t = FR[N] > FR[M];
            }
            break;
    }

    return 2 + FPSCR.precision_mode;
}

static i64 i_fcnvds(const u16 instr) {
    assert(FPSCR.precision_mode);
    assert((N & 1) == 0);

    FPUL = from_f32((f32)get_dr(N));

    return 4;
}

static i64 i_fcnvsd(const u16 instr) {
    assert(FPSCR.precision_mode);
    assert((N & 1) == 0);

    set_dr(N, (f64)to_f32(FPUL));

    return 3;
}

static i64 i_fdiv(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert(((M & 1) == 0) && ((N & 1) == 0));

        set_dr(N, get_dr(N) / get_dr(M));
    } else {
        FR[N] /= FR[M];
    }

    return 12 + (12 * FPSCR.precision_mode);
}

static f32 fipr_core(const f32* fvn, const f32* fvm) {
    return
        fvn[0] * fvm[0] +
        fvn[1] * fvm[1] +
        fvn[2] * fvm[2] +
        fvn[3] * fvm[3];
}

static i64 i_fipr(const u16 instr) {
    (FV[N & ~3])[3] = fipr_core(FV[N & ~3], FV[(N & 3) << 2]);

    return 4;
}

template<bool is_1>
static i64 i_fldi(const u16 instr) {
    if constexpr (is_1) {
        FR[N] = 1.0;
    } else {
        FR[N] = 0.0;
    }

    return 1;
}

static i64 i_flds(const u16 instr) {
    FPUL = FR_RAW[N];

    return 1;
}

static i64 i_float(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert((N & 1) == 0);

        set_dr(N, (f64)(i32)FPUL);
    } else {
        FR[N] = (f32)(i32)FPUL;
    }

    return 3 + FPSCR.precision_mode;
}

static i64 i_fmac(const u16 instr) {
    assert(!FPSCR.precision_mode);

    FR[N] += FR[0] * FR[M];

    return 3;
}

static i64 i_fmov(const u16 instr) {
    if (FPSCR.pair_mode) {
        set_dr_raw(N, get_dr_raw(M));
    } else {
        FR_RAW[N] = FR_RAW[M];
    }

    return 1;
}

static i64 i_fmov_index_load(const u16 instr) {
    if (FPSCR.pair_mode) {
        set_dr_move(N, read<u64>(GPRS[0] + GPRS[M]));
    } else {
        FR_RAW[N] = read<u32>(GPRS[0] + GPRS[M]);
    }
    
    return 1;
}

static i64 i_fmov_index_store(const u16 instr) {
    if (FPSCR.pair_mode) {
        write<u64>(GPRS[0] + GPRS[N], get_dr_move(M));
    } else {
        write<u32>(GPRS[0] + GPRS[N], FR_RAW[M]);
    }
    
    return 1;
}

static i64 i_fmov_load(const u16 instr) {
    if (FPSCR.pair_mode) {
        set_dr_move(N, read<u64>(GPRS[M]));
    } else {
        FR_RAW[N] = read<u32>(GPRS[M]);
    }

    return 1;
}

static i64 i_fmov_restore(const u16 instr) {
    if (FPSCR.pair_mode) {
        set_dr_move(N, read<u64>(GPRS[M]));

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

        write<u64>(GPRS[N], get_dr_move(M));
    } else {
        GPRS[N] -= sizeof(u32);

        write<u32>(GPRS[N], FR_RAW[M]);
    }

    return 1;
}

static i64 i_fmov_store(const u16 instr) {
    if (FPSCR.pair_mode) {
        write<u64>(GPRS[N], get_dr_move(M));
    } else {
        write<u32>(GPRS[N], FR_RAW[M]);
    }

    return 1;
}

static i64 i_fmul(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert(((M & 1) == 0) && ((N & 1) == 0));

        set_dr(N, get_dr(N) * get_dr(M));
    } else {
        FR[N] *= FR[M];
    }

    return 3 + (4 * FPSCR.precision_mode);
}

static i64 i_fneg(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert((N & 1) == 0);

        set_dr(N, -get_dr(N));
    } else {
        FR_RAW[N] ^= 1 << 31;
    }

    return 1;
}

static i64 i_frchg(const u16) {
    assert(!FPSCR.precision_mode);

    FPSCR.select_bank ^= 1;

    swap_fpu_banks();

    return 1;
}

static i64 i_fsca(const u16 instr) {
    assert(!FPSCR.precision_mode);
    assert((N & 1) == 0);

    const f32 angle = 2.0 * M_PI * (f32)(FPUL & 0xFFFF) / 65536.0;

    FR[N + 0] = std::sin(angle);
    FR[N + 1] = std::cos(angle);

    return 3;
}

static i64 i_fschg(const u16) {
    assert(!FPSCR.precision_mode);

    FPSCR.pair_mode ^= 1;

    return 1;
}

static i64 i_fsqrt(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert((N & 1) == 0);

        set_dr(N, std::sqrt(get_dr(N)));
    } else {
        FR[N] = std::sqrt(FR[N]);
    }

    return 1;
}

static i64 i_fsrra(const u16 instr) {
    FR[N] = 1.0 / std::sqrt(FR[N]);

    return 1;
}

static i64 i_fsts(const u16 instr) {
    FR_RAW[N] = FPUL;

    return 1;
}

static i64 i_fsub(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert(((M & 1) == 0) && ((N & 1) == 0));

        set_dr(N, get_dr(N) - get_dr(M));
    } else {
        FR[N] -= FR[M];
    }

    return 3 + (4 * FPSCR.precision_mode);
}

static i64 i_ftrc(const u16 instr) {
    if (FPSCR.precision_mode) {
        assert((N & 1) == 0);

        FPUL = (i32)get_dr(N);
    } else {
        FPUL = (i32)FR[N];
    }

    return 3 + FPSCR.precision_mode;
}

static i64 i_ftrv(const u16 instr) {
    assert(!FPSCR.precision_mode);

    f32* fvn = FV[N & ~3];

    // Load XMTRX
    f32 xmtrx[4][4];

    for (int i = 0; i < 4; i++) {
        xmtrx[0][i] = XR[4 * i + 0];
        xmtrx[1][i] = XR[4 * i + 1];
        xmtrx[2][i] = XR[4 * i + 2];
        xmtrx[3][i] = XR[4 * i + 3];
    }

    f32 result[4];

    for (int i = 0; i < 4; i++) {
        result[i] = fipr_core(xmtrx[i], fvn);
    }

    std::memcpy(fvn, result, sizeof(result));

    return 5;
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
        set_control_register<control_register>(GPRS[N], M & 7);
    } else {
        set_control_register<control_register>(read<u32>(GPRS[N]), M & 7);

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

static i64 i_movca(const u16 instr) {
    // TODO: emulate operand cache?

    write<u32>(GPRS[N], GPRS[0]);

    return 3;
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
static i64 i_movlg(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            GPRS[0] = (i8)read<u8>(GBR + IMM);
            break;
        case OperandSize::Word:
            GPRS[0] = (i16)read<u16>(GBR + (IMM << 1));
            break;
        case OperandSize::Long:
            GPRS[0] = read<u32>(GBR + (IMM << 2));
            break;
    }

    return 1;
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
static i64 i_movs0(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            write<u8>(GPRS[0] + GPRS[N], GPRS[M]);
            break;
        case OperandSize::Word:
            write<u16>(GPRS[0] + GPRS[N], GPRS[M]);
            break;
        case OperandSize::Long:
            write<u32>(GPRS[0] + GPRS[N], GPRS[M]);
            break;
    }

    return 2;
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

template<OperandSize size>
static i64 i_movsg(const u16 instr) {
    switch (size) {
        case OperandSize::Byte:
            write<u8>(GBR + IMM, GPRS[0]);
            break;
        case OperandSize::Word:
            write<u16>(GBR + (IMM << 1), GPRS[0]);
            break;
        case OperandSize::Long:
            write<u32>(GBR + (IMM << 2), GPRS[0]);
            break;
    }

    return 1;
}

static i64 i_movt(const u16 instr) {
    GPRS[N] = SR.t;

    return 1;
}

static i64 i_mull(const u16 instr) {
    MACL = GPRS[N] * GPRS[M];

    return 4;
}

static i64 i_muls(const u16 instr) {
    MACL = (i32)(i16)GPRS[N] * (i32)(i16)GPRS[M];

    return 4;
}

static i64 i_mulu(const u16 instr) {
    MACL = (u32)(u16)GPRS[N] * (u32)(u16)GPRS[M];

    return 4;
}

static i64 i_neg(const u16 instr) {
    GPRS[N] = 0 - GPRS[M];

    return 1;
}

static i64 i_negc(const u16 instr) {
    const u64 result = 0ULL - (u64)GPRS[M] - (u64)SR.t;

    SR.t = (i64)result < 0;

    GPRS[N] = (u32)result;

    return 1;
}

static i64 i_nop(const u16) {
    return 1;
}

static i64 i_not(const u16 instr) {
    GPRS[N] = ~GPRS[M];

    return 1;
}

static i64 i_ocbi(const u16) {
    // TODO: implement operand cache?

    // std::printf("SH-4 operand cache block invalidate @ %08X\n", GPRS[N]);

    return 1;
}

static i64 i_ocbp(const u16) {
    // TODO: implement operand cache?

    // std::printf("SH-4 operand cache block purge @ %08X\n", GPRS[N]);

    return 1;
}

static i64 i_ocbwb(const u16) {
    // TODO: implement operand cache?

    // std::printf("SH-4 operand cache block write-back @ %08X\n", GPRS[N]);

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

    // std::printf("SH-4 operand cache prefetch @ %08X\n", GPRS[N]);

    if (GPRS[N] >= REGION_P4) {
        ocio::flush_store_queue(GPRS[N] & PRIV_MASK);
    }

    return 1;
}

static i64 i_rotcl(const u16 instr) {
    const u32 old_t = SR.t;

    SR.t = (GPRS[N] >> 31) & 1;

    GPRS[N] = (GPRS[N] << 1) | old_t;

    return 1;
}

static i64 i_rotr(const u16 instr) {
    SR.t = GPRS[N] & 1;

    GPRS[N] = std::rotr(GPRS[N], 1);

    return 1;
}

static i64 i_rotcr(const u16 instr) {
    const u32 old_t = SR.t;

    SR.t = GPRS[N]& 1;

    GPRS[N] = (GPRS[N] >> 1) | (old_t << 31);

    return 1;
}

static i64 i_rte(const u16) {
    set_sr(SSR.raw);

    delayed_jump(SPC);

    return 5;
}

static i64 i_rts(const u16) {
    delayed_jump(PR);

    return 3;
}

static i64 i_sett(const u16) {
    SR.t = 1;

    return 1;
}

static i64 i_shar(const u16 instr) {
    SR.t = GPRS[N] & 1;

    GPRS[N] = (i32)GPRS[N] >> 1;

    return 1;
}

static i64 i_shad(const u16 instr) {
    if ((i32)GPRS[M] >= 0) {
        // Left shift
        GPRS[N] <<= GPRS[M] & 0x1F;
    } else if ((GPRS[M] & 0x1F) == 0) {
        // Right shift by 32
        if ((i32)GPRS[N] < 0) {
            GPRS[N] = ~0;
        } else {
            GPRS[N] = 0;
        }
    } else {
        // Right shift
        GPRS[N] = (i32)GPRS[N] >> ((~GPRS[M] & 0x1F) + 1);
    }

    return 1;
}

static i64 i_shld(const u16 instr) {
    if ((i32)GPRS[M] >= 0) {
        // Left shift
        GPRS[N] <<= GPRS[M] & 0x1F;
    } else if ((GPRS[M] & 0x1F) == 0) {
        // Right shift by 32
        GPRS[N] = 0;
    } else {
        // Right shift
        GPRS[N] >>= (~GPRS[M] & 0x1F) + 1;
    }

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

static i64 i_sleep(const u16) {
    set_state(STATE_SLEEPING);

    // Make sure CPU actually sleeps after this
    ctx.cycles = 1;

    return 1;
}

template <ControlRegister control_register, AddressingMode mode>
static i64 i_stc(const u16 instr) {
    if constexpr (mode == AddressingMode::RegisterDirect) {
        GPRS[N] = get_control_register<control_register>(M & 7);
    } else {
        // Register indirect pre-decrement
        GPRS[N] -= sizeof(u32);

        write<u32>(GPRS[N], get_control_register<control_register>(M & 7));
    }

    // See sts
    return 2;
}

template <SystemRegister system_register, AddressingMode mode>
static i64 i_sts(const u16 instr) {
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

static i64 i_sub(const u16 instr) {
    GPRS[N] -= GPRS[M];

    return 1;
}

static i64 i_subc(const u16 instr) {
    const u64 result = (u64)GPRS[N] - (u64)GPRS[M] - (u64)SR.t;

    SR.t = (i64)result < 0;

    GPRS[N] = (u32)result;

    return 1;
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

static i64 i_tas(const u16 instr) {
    const u8 data = read<u8>(GPRS[N]);

    SR.t = data == 0;

    write<u8>(GPRS[N], data | (1 << 7));

    return 5;
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

static i64 i_xtrct(const u16 instr) {
    GPRS[N] = (GPRS[M] << 16) | (GPRS[N] >> 16);

    return 1;
}

static void initialize_instr_table() {
    ctx.instr_table.fill(i_undefined);

    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00000010", i_stc<ControlRegister::Sr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00000011", i_bra<true, false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx0100", i_movs0<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx0101", i_movs0<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx0110", i_movs0<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx0111", i_mull);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000001000", i_clrt);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000001001", i_nop);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00001010", i_sts<SystemRegister::Mach, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000001011", i_rts);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx1100", i_movl0<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx1101", i_movl0<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxxxxxx1110", i_movl0<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00010010", i_stc<ControlRegister::Gbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000011000", i_sett);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000011001", i_div0<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00011010", i_sts<SystemRegister::Macl, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000011011", i_sleep);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00100010", i_stc<ControlRegister::Vbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00100011", i_bra<false, false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00101001", i_movt);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00101010", i_sts<SystemRegister::Pr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000000101011", i_rte);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx00110010", i_stc<ControlRegister::Ssr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx01000010", i_stc<ControlRegister::Spc, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000000001001000", i_clrs);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx01011010", i_sts<SystemRegister::Fpul, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx01101010", i_sts<SystemRegister::Fpscr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx1xxx0010", i_stc<ControlRegister::Rbank, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx10000011", i_pref);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx10010011", i_ocbi);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx10100011", i_ocbp);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx10110011", i_ocbwb);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx11000011", i_movca);
    fill_table_with_pattern(ctx.instr_table.data(), "0000xxxx11111010", i_stc<ControlRegister::Dbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0001xxxxxxxxxxxx", i_movs4<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0000", i_movs<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0001", i_movs<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0010", i_movs<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0100", i_movm<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0101", i_movm<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0110", i_movm<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx0111", i_div0<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1000", i_tst<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1001", i_and<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1010", i_xor<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1011", i_or<AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1100", i_cmp<Comparison::String>);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1101", i_xtrct);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1110", i_mulu);
    fill_table_with_pattern(ctx.instr_table.data(), "0010xxxxxxxx1111", i_muls);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0000", i_cmp<Comparison::Equal>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0010", i_cmp<Comparison::HigherSame>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0011", i_cmp<Comparison::GreaterEqual>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0100", i_div1);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0101", i_dmulu);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0110", i_cmp<Comparison::Higher>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx0111", i_cmp<Comparison::GreaterThan>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx1000", i_sub);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx1010", i_subc);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx1100", i_add<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx1101", i_dmuls);
    fill_table_with_pattern(ctx.instr_table.data(), "0011xxxxxxxx1110", i_addc);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000000", i_shll<1>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000001", i_shlr<1>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000010", i_sts<SystemRegister::Mach, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000011", i_stc<ControlRegister::Sr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000101", i_rotr);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000110", i_lds<SystemRegister::Mach, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00000111", i_ldc<ControlRegister::Sr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001000", i_shll<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001001", i_shlr<2>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001010", i_lds<SystemRegister::Mach, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001011", i_jsr);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxxxxxx1100", i_shad);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxxxxxx1101", i_shld);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00001110", i_ldc<ControlRegister::Sr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010000", i_dt);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010001", i_cmp<Comparison::PositiveZero>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010010", i_sts<SystemRegister::Macl, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010011", i_stc<ControlRegister::Gbr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010101", i_cmp<Comparison::Positive>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010110", i_lds<SystemRegister::Macl, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00010111", i_ldc<ControlRegister::Gbr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011000", i_shll<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011001", i_shlr<8>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011010", i_lds<SystemRegister::Macl, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011011", i_tas);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00011110", i_ldc<ControlRegister::Gbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100001", i_shar);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100010", i_sts<SystemRegister::Pr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100011", i_stc<ControlRegister::Vbr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100100", i_rotcl);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100101", i_rotcr);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100110", i_lds<SystemRegister::Pr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00100111", i_ldc<ControlRegister::Vbr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101000", i_shll<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101001", i_shlr<16>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101010", i_lds<SystemRegister::Pr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101011", i_jmp);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00101110", i_ldc<ControlRegister::Vbr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00110011", i_stc<ControlRegister::Ssr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00110111", i_ldc<ControlRegister::Ssr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx00111110", i_ldc<ControlRegister::Ssr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01000011", i_stc<ControlRegister::Spc, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01000111", i_ldc<ControlRegister::Spc, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01001110", i_ldc<ControlRegister::Spc, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01010010", i_sts<SystemRegister::Fpul, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01010110", i_lds<SystemRegister::Fpul, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01011010", i_lds<SystemRegister::Fpul, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01100010", i_sts<SystemRegister::Fpscr, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01100110", i_lds<SystemRegister::Fpscr, AddressingMode::RegisterIndirectPostincrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx01101010", i_lds<SystemRegister::Fpscr, AddressingMode::RegisterDirect>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx1xxx0011", i_stc<ControlRegister::Rbank, AddressingMode::RegisterIndirectPredecrement>);
    fill_table_with_pattern(ctx.instr_table.data(), "0100xxxx1xxx0111", i_ldc<ControlRegister::Rbank, AddressingMode::RegisterIndirectPostincrement>);
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
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx0111", i_not);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1000", i_swap<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1001", i_swap<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1010", i_negc);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1011", i_neg);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1100", i_extu<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1101", i_extu<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1110", i_exts<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "0110xxxxxxxx1111", i_exts<OperandSize::Word>);
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
    fill_table_with_pattern(ctx.instr_table.data(), "11000000xxxxxxxx", i_movsg<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "11000001xxxxxxxx", i_movsg<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "11000010xxxxxxxx", i_movsg<OperandSize::Long>);
    fill_table_with_pattern(ctx.instr_table.data(), "11000100xxxxxxxx", i_movlg<OperandSize::Byte>);
    fill_table_with_pattern(ctx.instr_table.data(), "11000101xxxxxxxx", i_movlg<OperandSize::Word>);
    fill_table_with_pattern(ctx.instr_table.data(), "11000110xxxxxxxx", i_movlg<OperandSize::Long>);
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
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0000", i_fadd);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0001", i_fsub);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0010", i_fmul);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0011", i_fdiv);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0100", i_fcmp<Comparison::Equal>);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0101", i_fcmp<Comparison::GreaterThan>);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx00001101", i_fsts);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx00011101", i_flds);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx00101101", i_float);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx00111101", i_ftrc);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx01001101", i_fneg);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx01011101", i_fabs);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx01101101", i_fsqrt);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx01111101", i_fsrra);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx10001101", i_fldi<false>);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx10011101", i_fldi<true>);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx10101101", i_fcnvsd);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx10111101", i_fcnvds);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxx11101101", i_fipr);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxx011111101", i_fsca);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xx0111111101", i_ftrv);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0110", i_fmov_index_load);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx0111", i_fmov_index_store);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1000", i_fmov_load);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1001", i_fmov_restore);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1010", i_fmov_store);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1011", i_fmov_save);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1100", i_fmov);
    fill_table_with_pattern(ctx.instr_table.data(), "1111xxxxxxxx1110", i_fmac);
    fill_table_with_pattern(ctx.instr_table.data(), "1111001111111101", i_fschg);
    fill_table_with_pattern(ctx.instr_table.data(), "1111101111111101", i_frchg);
}

void initialize() {
    ocio::initialize();

    SR.interrupt_mask = 0xF;

    raise_exception(ExceptionEvent::Reset, ExceptionOffset::Reset);

    initialize_instr_table();

    set_state(STATE_RUNNING);
}

void reset() {
    ocio::reset();

    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    ocio::shutdown();
}

void setup_for_sideload(const u32 entry) {
    set_sr(0x600000F0);
    set_fpscr(0x00040001);

    GPRS[0] = 0x8C010000;
    GPRS[1] = 0x00000808;
    GPRS[2] = 0x8C00E070;
    GPRS[3] = 0x8C010000;
    GPRS[4] = 0x8C010000;
    GPRS[5] = 0xF4000000;
    GPRS[6] = 0xF4002000;
    GPRS[6] = 0x00000044;
    GPRS[15] = 0x8C00F400;

    BANKED_GPRS[0] = 0x600000F0;
    BANKED_GPRS[1] = 0x00000808;
    BANKED_GPRS[2] = 0x8C00E070;

    FR_RAW[4] = 0x3F266666;
    FR_RAW[5] = 0x3FE66666;
    FR_RAW[6] = 0x41840000;
    FR_RAW[7] = 0x3F800000;
    FR_RAW[8] = 0x80000000;
    FR_RAW[9] = 0x80000000;
    FR_RAW[11] = 0x3F800000;

    GBR = 0x8C000000;
    SSR.raw = 0x40000001;
    SPC = 0x8C000776;
    SGR = 0x8D000000;
    DBR = 0x8C000010;
    VBR = 0x8C000000;
    PR = 0x8C00E09C;
    FPUL = 0x00000000;

    jump(entry);
}

static bool in_delay_slot() {
    return NPC != (CPC + 2 * sizeof(u16));
}

static void raise_interrupt(const u32 level) {
    std::printf("SH-4 interrupt @ %08X (level = %u)\n", CPC, level);

    // Save exception context
    SPC = PC;
    SSR = SR;
    SGR = GPRS[15];

    auto new_sr = SR;

    new_sr.block_exception = 1;
    new_sr.is_privileged = 1;
    new_sr.select_bank = 1;

    set_sr(new_sr.raw);

    ocio::ccn::set_interrupt_event(ExceptionEvent::ExternalInterrupt + 0x20 * (15 - level));

    jump(VBR + ExceptionOffset::ExternalInterrupt);

    set_state(STATE_RUNNING);
}

static void check_pending_interrupts() {
    if (SR.block_exception || in_delay_slot() || (ctx.pending_interrupts == 0)) {
        // Interrupts can't happen in delay slots
        return;
    }

    // Find highest level interrupt
    const u16 level = (8 * sizeof(u16) - 1) - std::countl_zero(ctx.pending_interrupts);

    if (level > SR.interrupt_mask) {
        raise_interrupt(level);
    }
}

void assert_interrupt(const int interrupt_level) {
    if ((ctx.pending_interrupts & (1 << interrupt_level)) == 0) {
        ctx.pending_interrupts |= 1 << interrupt_level;

        std::printf("SH-4 level %d interrupt pending\n", interrupt_level);
    }
}

void clear_interrupt(const int interrupt_level) {
    if ((ctx.pending_interrupts & (1 << interrupt_level)) != 0) {
        ctx.pending_interrupts &= ~(1 << interrupt_level);

        std::printf("SH-4 level %d interrupt cleared\n", interrupt_level);
    }
}

void step() {
    ocio::tmu::step(ctx.cycles);

    if (ctx.state == STATE_SLEEPING) {
        // Zzz...
        ctx.cycles = 0;

        check_pending_interrupts();

        return;
    }

    while (ctx.cycles > 0) {
        const u16 instr = fetch_instr();
        
        ctx.cycles -= ctx.instr_table[instr](instr);

        check_pending_interrupts();
    }
}

i64* get_cycles() {
    return &ctx.cycles;
}

}
