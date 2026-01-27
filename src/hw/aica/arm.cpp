/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#include <hw/aica/arm.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

#include <hw/aica/bus.hpp>

namespace hw::aica::arm {

constexpr bool SILENT_ARM = false;

// Instruction bit macros
#define OPCODE (((instr >> 4) & 0x000F) | ((instr >> 16) & 0x0FF0))
#define DP_OP  ((instr >> 21) & 0x000F)
#define COND   ((instr >> 28) & 0x000F)
#define RLIST  ((instr >>  0) & 0xFFFF)
#define IMM    ((instr >>  0) & 0x00FF)
#define OFS_8  (((instr >> 0) & 0x000F) | ((instr >> 4) & 0x00F0))
#define OFS_12 ((instr >>  0) & 0x0FFF)
#define SHIFT  ((instr >>  5) & 0x0003)
#define AMOUNT ((instr >>  7) & 0x001F)
#define ROTATE ((instr >>  8) & 0x000F)
#define RM     ((instr >>  0) & 0x000F)
#define RS     ((instr >>  8) & 0x000F)
#define RD     ((instr >> 12) & 0x000F)
#define RN     ((instr >> 16) & 0x000F)
#define MASK   ((instr >> 16) & 0x000F)
#define P      ((instr >> 24) & 0x0001)
#define U      ((instr >> 23) & 0x0001)
#define B      ((instr >> 22) & 0x0001)
#define W      ((instr >> 21) & 0x0001)

// Register file macros
#define SP          ctx.gprs[13]
#define SP_FIQ      ctx.fiq_sp
#define SP_IRQ      ctx.irq_sp
#define SP_ABT      ctx.abt_sp
#define SP_SVC      ctx.svc_sp
#define SP_UND      ctx.und_sp
#define LR          ctx.gprs[14]
#define LR_FIQ      ctx.fiq_lr
#define LR_IRQ      ctx.irq_lr
#define LR_ABT      ctx.abt_lr
#define LR_SVC      ctx.svc_lr
#define LR_UND      ctx.und_lr
#define PC          ctx.gprs[15]
#define PC_DELAY    (ctx.gprs[15] + sizeof(u32))
#define CPC         ctx.current_pc
#define GPRS        ctx.gprs
#define FIQ_GPRS    ctx.fiq_gprs
#define CPSR        ctx.cpsr
#define SPSR        ctx.spsr
#define SPSR_FIQ    ctx.fiq_spsr
#define SPSR_IRQ    ctx.irq_spsr
#define SPSR_ABT    ctx.abt_spsr
#define SPSR_SVC    ctx.svc_spsr
#define SPSR_UND    ctx.und_spsr

constexpr usize NUM_REGS = 16;
constexpr usize NUM_FIQ_REGS = 5;

constexpr usize INSTR_TABLE_SIZE = 4096;

enum {
    STATE_RUNNING,
    STATE_SLEEPING,
};

// Program status register
union ProgramStatus {
    u32 raw;

    struct {
        u32 mode        :  5;
        u32             :  1;
        u32 disable_fiq :  1;
        u32 disable_irq :  1;
        u32             : 20;
        u32 overflow    :  1;
        u32 carry       :  1;
        u32 zero        :  1;
        u32 negative    :  1;
    };
};

struct {
    u32 current_pc;

    u32 gprs[NUM_REGS], fiq_gprs[NUM_FIQ_REGS];

    // PSRs
    ProgramStatus cpsr;
    ProgramStatus* spsr;

    u32 carry_out;

    // Banked stack pointers/link registers
    u32 fiq_sp, svc_sp, abt_sp, irq_sp, und_sp;
    u32 fiq_lr, svc_lr, abt_lr, irq_lr, und_lr;

    // Banked PSRs
    ProgramStatus fiq_spsr, svc_spsr, abt_spsr, irq_spsr, und_spsr;

    int state;

    bool pending_interrupt;

    i64 cycles;
} ctx;

static std::array<void (*)(const u32), INSTR_TABLE_SIZE> instr_table;

static void dump_registers() {
    for (usize i = 0; i < NUM_REGS; i++) {
        std::printf("[R%zu%*c] %08X ", i, (i < 10) ? 6 : 5, ' ', (i == 15) ? CPC : GPRS[i]);

        if ((i % 4) == 3) {
            std::puts("");
        }
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

static void set_state(const int state) {
    ctx.state = state;
}

enum {
    MODE_USR = 0x10,
    MODE_FIQ = 0x11,
    MODE_IRQ = 0x12,
    MODE_SVC = 0x13,
    MODE_ABT = 0x17,
    MODE_UND = 0x1B,
    MODE_SYS = 0x1F,
};

static void change_mode(const int mode) {
    if (CPSR.mode == mode) {
        return;
    }

    // Save old register bank
    switch (CPSR.mode) {
        case MODE_USR:
        case MODE_SYS:
            break;
        case MODE_FIQ:
            for (usize i = 0; i < NUM_FIQ_REGS; i++) {
                std::swap(FIQ_GPRS[i], GPRS[i + 8]);
            }

            std::swap(SP_FIQ, SP);
            std::swap(LR_FIQ, LR);
            break;
        case MODE_IRQ:
            std::swap(SP_IRQ, SP);
            std::swap(LR_IRQ, LR);
            break;
        case MODE_SVC:
            std::swap(SP_SVC, SP);
            std::swap(LR_SVC, LR);
            break;
        case MODE_ABT:
            std::swap(SP_ABT, SP);
            std::swap(LR_ABT, LR);
            break;
        case MODE_UND:
            std::swap(SP_UND, SP);
            std::swap(LR_UND, LR);
            break;
    }

    // Load new register bank
    switch (mode) {
        case MODE_USR:
        case MODE_SYS:
            SPSR = nullptr;
            break;
        case MODE_FIQ:
            for (usize i = 0; i < NUM_FIQ_REGS; i++) {
                std::swap(FIQ_GPRS[i], GPRS[i + 8]);
            }

            std::swap(SP_FIQ, SP);
            std::swap(LR_FIQ, LR);

            SPSR = &SPSR_FIQ;
            break;
        case MODE_IRQ:
            std::swap(SP_IRQ, SP);
            std::swap(LR_IRQ, LR);

            SPSR = &SPSR_IRQ;
            break;
        case MODE_SVC:
            std::swap(SP_SVC, SP);
            std::swap(LR_SVC, LR);

            SPSR = &SPSR_SVC;
            break;
        case MODE_ABT:
            std::swap(SP_ABT, SP);
            std::swap(LR_ABT, LR);

            SPSR = &SPSR_ABT;
            break;
        case MODE_UND:
            std::swap(SP_UND, SP);
            std::swap(LR_UND, LR);

            SPSR = &SPSR_UND;
            break;
        default:
            std::printf("ARM Invalid mode %02X\n", mode);
            exit(1);
    }

    CPSR.mode = mode;
}

static void restore_cpsr() {
    assert(SPSR != nullptr);

    const int mode = SPSR->mode;

    // Restore flags, preserve old mode for mode change
    CPSR.raw &= 0x1F;
    CPSR.raw |= SPSR->raw & ~0x1F;

    change_mode(mode);
}

static void raise_fast_interrupt() {
    constexpr u32 FIQ_VECTOR = 0x1C;

    const u32 lr = PC_DELAY;

    if constexpr (!SILENT_ARM) std::printf("ARM Fast interrupt @ %08X\n", CPC);

    // Save CPSR
    SPSR_FIQ = CPSR;

    CPSR.disable_fiq = 1;
    CPSR.disable_irq = 1;

    change_mode(MODE_FIQ);

    LR = lr;
    PC = FIQ_VECTOR;

    ctx.pending_interrupt = false;
}

static void check_pending_interrupts() {
    if (ctx.pending_interrupt && !CPSR.disable_fiq) {
        raise_fast_interrupt();
    }
}

enum {
    CONDITION_CODE_EQ,
    CONDITION_CODE_NE,
    CONDITION_CODE_HS,
    CONDITION_CODE_LO,
    CONDITION_CODE_MI,
    CONDITION_CODE_PL,
    CONDITION_CODE_VS,
    CONDITION_CODE_VC,
    CONDITION_CODE_HI,
    CONDITION_CODE_LS,
    CONDITION_CODE_GE,
    CONDITION_CODE_LT,
    CONDITION_CODE_GT,
    CONDITION_CODE_LE,
    CONDITION_CODE_AL,
    CONDITION_CODE_NV,
};

static bool check_condition(const int condition_code) {
    switch (condition_code) {
        case CONDITION_CODE_EQ:
            return CPSR.zero;
        case CONDITION_CODE_NE:
            return !CPSR.zero;
        case CONDITION_CODE_HS:
            return CPSR.carry;
        case CONDITION_CODE_LO:
            return !CPSR.carry;
        case CONDITION_CODE_MI:
            return CPSR.negative;
        case CONDITION_CODE_PL:
            return !CPSR.negative;
        case CONDITION_CODE_VS:
            return CPSR.overflow;
        case CONDITION_CODE_VC:
            return !CPSR.overflow;
        case CONDITION_CODE_HI:
            return CPSR.carry && !CPSR.zero;
        case CONDITION_CODE_LS:
            return CPSR.zero || !CPSR.carry;
        case CONDITION_CODE_GE:
            return CPSR.negative == CPSR.overflow;
        case CONDITION_CODE_LT:
            return CPSR.negative != CPSR.overflow;
        case CONDITION_CODE_GT:
            return (CPSR.negative == CPSR.overflow) && !CPSR.zero;
        case CONDITION_CODE_LE:
            return (CPSR.negative != CPSR.overflow) || CPSR.zero;
        case CONDITION_CODE_AL:
            return true;
        default:
            std::puts("ARM Invalid condition code NV");
            exit(1);
    }
}

static void set_bit_flags(const u32 n) {
    CPSR.carry = ctx.carry_out;
    CPSR.zero = n == 0;
    CPSR.negative = (n >> 31) & 1;
}

static void set_add_flags(const u32 a, const u32 b, const u32 n) {
    CPSR.overflow = (((a ^ b) >> 31) == 0) && (((a ^ n) >> 31) != 0);
    CPSR.carry = (0xFFFFFFFF - a) < b;
    CPSR.zero = n == 0;
    CPSR.negative = (n >> 31) & 1;
}

static void set_add_flags_with_carry(const u32 a, const u32 b, const u64 n) {
    const u32 temp = a + b;
    const u32 n32 = (u32)n;

    CPSR.overflow = ((((a ^ b) >> 31) == 0) && (((a ^ temp) >> 31) != 0)) || ((((temp ^ CPSR.carry) >> 31) == 0) && (((temp ^ n32) >> 31) != 0));
    CPSR.carry = (n >> 32) & 1;
    CPSR.zero = n32 == 0;
    CPSR.negative = (n >> 31) & 1;
}

static void set_sub_flags(const u32 a, const u32 b, const u32 n) {
    CPSR.overflow = (((a ^ b) >> 31) != 0) && (((a ^ n) >> 31) != 0);
    CPSR.carry = a >= b;
    CPSR.zero = n == 0;
    CPSR.negative = (n >> 31) & 1;
}

enum {
    SHIFT_TYPE_LSL,
    SHIFT_TYPE_LSR,
    SHIFT_TYPE_ASR,
    SHIFT_TYPE_ROR,
};

template<int shift_type, bool is_immediate>
static u32 shift(const u32 data, u32 amount) {
    amount &= 0xFF;

    switch (shift_type) {
        case SHIFT_TYPE_LSL:
            if (amount == 0) {
                // Don't set flags
                ctx.carry_out = CPSR.carry;

                return data;
            }

            if (amount >= 32) {
                if (amount == 32) {
                    ctx.carry_out = data & 1;
                } else {
                    ctx.carry_out = 0;
                }

                return 0;
            }

            ctx.carry_out = ((data << (amount - 1)) >> 31) & 1;

            return data << amount;
        case SHIFT_TYPE_LSR:
            if (amount == 0) {
                if constexpr (!is_immediate) {
                    ctx.carry_out = CPSR.carry;

                    return data;
                }

                amount = 32;
            }

            if (amount >= 32) {
                if (amount == 32) {
                    ctx.carry_out = (data >> 31) & 1;
                } else {
                    ctx.carry_out = 0;
                }

                return 0;
            }

            ctx.carry_out = (data >> (amount - 1)) & 1;

            return data >> amount;
        case SHIFT_TYPE_ASR:
            {
                if (amount == 0) {
                    if constexpr (!is_immediate) {
                        ctx.carry_out = CPSR.carry;

                        return data;
                    }

                    amount = 32;
                }

                if (amount >= 32) {
                    const u32 sign = data >> 31;

                    ctx.carry_out = sign;

                    return 0 - sign;
                }
                
                ctx.carry_out = (data >> (amount - 1)) & 1;

                return (u32)((i32)data >> amount);
            }
        case SHIFT_TYPE_ROR:
            {
                if (!is_immediate || (amount != 0)) {
                    if (amount == 0) {
                        ctx.carry_out = CPSR.carry;

                        return data;
                    }

                    amount &= 0x1F;

                    const u32 out = std::rotr(data, amount - 1);

                    ctx.carry_out = data & 1;

                    return std::rotr(out, 1);
                } else {
                    // RRX
                    ctx.carry_out = data & 1;

                    return (data >> 1) | (CPSR.carry << 31);
                }
            }
    }
}

template<typename T>
static T read(const u32 addr) {
    if (addr == 0x0D2D8800) {
        dump_registers();
    }

    if ((addr & (sizeof(T) - 1)) != 0) {
        std::printf("Unaligned ARM read%zu @ %08X\n", 8 * sizeof(T), addr);

        dump_registers();
        exit(1);
    }

    return bus::read<T>(addr);
}

static u32 fetch_instr() {
    const u32 instr = read<u32>(PC);

    PC += sizeof(instr);

    return instr;
}

template<typename T>
static void write(const u32 addr, const u32 data) {
    if ((addr & (sizeof(T) - 1)) != 0) {
        std::printf("Unaligned ARM write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
        exit(1);
    }

    return bus::write<T>(addr, data);
}

static u32 rotate_immediate(const u32 immediate, u32 amount) {
    if (amount == 0) {
        ctx.carry_out = CPSR.carry;

        return immediate;
    }

    amount <<= 1;

    ctx.carry_out = (immediate & (1 << (amount - 1))) != 0;

    return std::rotr(immediate, amount);
}

template<bool use_spsr>
static void i_mrs(const u32 instr) {
    assert(RD != 15);

    if constexpr (use_spsr) {
        assert(SPSR != nullptr);

        GPRS[RD] = SPSR->raw;
    } else {
        GPRS[RD] = CPSR.raw;
    }
}

template<bool is_load>
static void i_block_data_transfer(const u32 instr) {
    assert(RN != 15);
    assert(RLIST != 0);
    assert((RLIST & (1 << RN)) == 0);

    bool is_pre_indexed = P;

    u32 addr = GPRS[RN];

    if (!U) {
        // Transfers always start at the lowest address, even for "decrementing" block transfers
        addr -= 4 * std::popcount(RLIST);

        is_pre_indexed = !is_pre_indexed;
    }

    const int mode = CPSR.mode;

    if (B && (!is_load || ((RLIST & (1 << 15)) == 0))) {
        // All STM, LDM that don't load PC transfer USR mode registers here
        change_mode(MODE_USR);
    }

    for (u32 reglist = RLIST; reglist != 0; ) {
        const u32 i = std::countr_zero(reglist);

        if (is_pre_indexed) {
            addr += 4;
        }

        if constexpr (is_load) {
            // Address is force-aligned
            GPRS[i] = read<u32>(addr & ~3);
        } else {
            u32 data = GPRS[i];

            if (i == 15) {
                // PC is 12 bytes ahead during a store operation
                data += 8;
            }

            write<u32>(addr & ~3, data);
        }

        if (!is_pre_indexed) {
            addr += 4;
        }

        reglist ^= 1 << i;
    }

    if (W) {
        if (!U) {
            addr -= 4 * std::popcount(RLIST);
        }

        GPRS[RN] = addr;
    }

    if (B) {
        // Either restore the old mode (STM, LDM with PC not in RLIST) OR
        // restore the CPSR (LDM with PC in RLIST)
        if (!is_load || ((RLIST & (1 << 15)) == 0)) {
            change_mode(mode);
        } else {
            restore_cpsr();
        }
    }
}

template<bool is_link>
static void i_branch(const u32 instr) {
    const u32 offset = ((i32)instr << 8) >> 6;

    if constexpr (is_link) {
        LR = PC;
    }

    PC = PC_DELAY + offset;
}

enum {
    DP_OP_AND,
    DP_OP_EOR,
    DP_OP_SUB,
    DP_OP_RSB,
    DP_OP_ADD,
    DP_OP_ADC,
    DP_OP_SBC,
    DP_OP_RSC,
    DP_OP_TST,
    DP_OP_TEQ,
    DP_OP_CMP,
    DP_OP_CMN,
    DP_OP_ORR,
    DP_OP_MOV,
    DP_OP_BIC,
    DP_OP_MVN,
};

template<bool is_immediate, bool is_immediate_shift, bool set_flags>
static void i_data_processing(const u32 instr) {
    u32 op_1 = GPRS[RN];

    if (RN == 15) {
        // PC is at least 8 bytes ahead
        op_1 += 4;
    }

    // Decode second operand
    u32 op_2;

    if constexpr (is_immediate) {
        op_2 = rotate_immediate(IMM, ROTATE);
    } else {
        op_2 = GPRS[RM];
    
        if (RM == 15) {
            // ...
            op_2 += 4;
        }

        u32 amount = AMOUNT;

        if constexpr (!is_immediate_shift) {
            amount = GPRS[RS];

            // Shifting takes a cycle, causing PC to advance by 4 more bytes before the register
            // values are read
            if (RN == 15) {
                op_1 += 4;
            }

            if (RM == 15) {
                op_2 += 4;
            }
        }

        switch (SHIFT) {
            case SHIFT_TYPE_LSL:
                op_2 = shift<SHIFT_TYPE_LSL, is_immediate_shift>(op_2, amount);
                break;
            case SHIFT_TYPE_LSR:
                op_2 = shift<SHIFT_TYPE_LSR, is_immediate_shift>(op_2, amount);
                break;
            case SHIFT_TYPE_ASR:
                op_2 = shift<SHIFT_TYPE_ASR, is_immediate_shift>(op_2, amount);
                break;
            case SHIFT_TYPE_ROR:
                op_2 = shift<SHIFT_TYPE_ROR, is_immediate_shift>(op_2, amount);
                break;
        default:
            std::printf("ARM Invalid shift type %d\n", SHIFT);
            exit(1);
        }
    }

    switch (DP_OP) {
        case DP_OP_AND:
            GPRS[RD] = op_1 & op_2;

            if (set_flags) {
                set_bit_flags(GPRS[RD]);
            }
            break;
        case DP_OP_EOR:
            GPRS[RD] = op_1 ^ op_2;

            if (set_flags) {
                set_bit_flags(GPRS[RD]);
            }
            break;
        case DP_OP_SUB:
            GPRS[RD] = op_1 - op_2;

            if (set_flags) {
                set_sub_flags(op_1, op_2, GPRS[RD]);
            }
            break;
        case DP_OP_RSB:
            GPRS[RD] = op_2 - op_1;

            if (set_flags) {
                set_sub_flags(op_2, op_1, GPRS[RD]);
            }
            break;
        case DP_OP_ADD:
            GPRS[RD] = op_1 + op_2;

            if (set_flags) {
                set_add_flags(op_1, op_2, GPRS[RD]);
            }
            break;
        case DP_OP_ADC:
            {
                const u64 result = (u64)op_1 + (u64)op_2 + (u64)CPSR.carry;

                GPRS[RD] = (u32)result;

                if (set_flags) {
                    set_add_flags_with_carry(op_1, op_2, result);
                }
            }
            break;
        case DP_OP_TST:
            set_bit_flags(op_1 & op_2);
            break;
        case DP_OP_TEQ:
            set_bit_flags(op_1 ^ op_2);
            break;
        case DP_OP_CMP:
            set_sub_flags(op_1, op_2, op_1 - op_2);
            break;
        case DP_OP_CMN:
            set_add_flags(op_1, op_2, op_1 + op_2);
            break;
        case DP_OP_ORR:
            GPRS[RD] = op_1 | op_2;

            if (set_flags) {
                set_bit_flags(GPRS[RD]);
            }
            break;
        case DP_OP_MOV:
            GPRS[RD] = op_2;

            if (set_flags) {
                set_bit_flags(GPRS[RD]);
            }
            break;
        case DP_OP_BIC:
            GPRS[RD] = op_1 & ~op_2;

            if (set_flags) {
                set_bit_flags(GPRS[RD]);
            }
            break;
        case DP_OP_MVN:
            GPRS[RD] = ~op_2;

            if (set_flags) {
                set_bit_flags(GPRS[RD]);
            }
            break;
        default:
            std::printf("ARM Unimplemented data processing opcode %u\n", DP_OP);
            exit(1);
    }

    if (set_flags && (RD == 15)) {
        // This is used to return from an exception
        restore_cpsr();
    }
}

template<bool is_immediate, bool is_load>
static void i_halfword_data_transfer(const u32 instr) {
    assert(RD != 15);
    assert(P || !W);

    u32 addr = GPRS[RN];

    if (RN == 15) {
        // PC is at least 8 bytes ahead
        addr += 4;
    }

    u32 offset = OFS_8;

    if constexpr (!is_immediate) {
        assert(RM != 15);

        offset = GPRS[RM];
    }

    if (P) {
        if (U) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    if constexpr (is_load) {
        assert((addr & 1) == 0);

        GPRS[RD] = read<u16>(addr);
    } else {
        write<u16>(addr & ~1, GPRS[RD]);
    }

    if (!is_load || (RN != RD)) {
        if (!P) {
            // Post-indexing implies writeback
            assert(RN != 15);

            if (U) {
                addr += offset;
            } else {
                addr -= offset;
            }

            GPRS[RN] = addr;
        } else if (W) {
            GPRS[RN] = addr;
        }
    }
}

template<bool is_immediate, bool use_spsr>
static void i_msr(const u32 instr) {
    u32 op;

    if constexpr (is_immediate) {
        op = rotate_immediate(IMM, ROTATE);
    } else {
        op = GPRS[RM];
    }

    u32 old_mode;

    ProgramStatus* psr;

    if constexpr (use_spsr) {
        assert(SPSR != nullptr);

        psr = SPSR;
    } else {
        psr = &CPSR;

        if ((MASK & 1) != 0) {
            // Save mode for mode change
            old_mode = psr->mode;
        }
    }

    // Build PSR mask
    u32 mask = 0;

    for (int i = 0; i < 3; i++) {
        if ((MASK & (1 << i)) != 0) {
            mask |= 0xFF << (8 * i);
        }
    }

    if (CPSR.mode == MODE_USR) {
        mask &= 0xFF000000;
    }

    psr->raw &= ~mask;
    psr->raw |= op & mask;

    if (!use_spsr && ((MASK & 1) != 0)) {
        const int mode = op & 0x1F;

        psr->mode = old_mode;

        change_mode(mode);
    }
}

template<bool is_accumulate, bool set_flags>
static void i_multiply(const u32 instr) {
    assert((RD != 15) && (RM != 15) && (RD != 15) && (RS != 15));
    assert(RN != RM);

    u32 result = GPRS[RM] * GPRS[RS];

    if constexpr (is_accumulate) {
        // Actually RN
        result += GPRS[RD];
    }

    // Actually RD
    GPRS[RN] = result;

    if constexpr (set_flags) {
        set_bit_flags(result);
    }
}

template<bool is_immediate, bool is_load>
void i_single_data_transfer(const u32 instr) {
    assert(P || !W);

    u32 addr = GPRS[RN];

    if (RN == 15) {
        // PC is at least 8 bytes ahead
        addr += 4;
    }

    u32 offset = OFS_12;

    if constexpr (!is_immediate) {
        offset = GPRS[RM];
    
        if (RM == 15) {
            // ...
            offset += 4;
        }

        switch (SHIFT) {
            case SHIFT_TYPE_LSL:
                offset = shift<SHIFT_TYPE_LSL, true>(offset, AMOUNT);
                break;
            case SHIFT_TYPE_LSR:
                offset = shift<SHIFT_TYPE_LSR, true>(offset, AMOUNT);
                break;
            case SHIFT_TYPE_ASR:
                offset = shift<SHIFT_TYPE_ASR, true>(offset, AMOUNT);
                break;
            case SHIFT_TYPE_ROR:
                offset = shift<SHIFT_TYPE_ROR, true>(offset, AMOUNT);
                break;
        }
    }

    if (P) {
        if (U) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    if constexpr (is_load) {
        if (B) {
            GPRS[RD] = read<u8>(addr);
        } else {
            GPRS[RD] = std::rotr(read<u32>(addr & ~3), 8 * (addr & 3));
        }
    } else {
        u32 data = GPRS[RD];

        if (RD == 15) {
            data += 8;
        }

        if (B) {
            write<u8>(addr, data);
        } else {
            write<u32>(addr & ~3, data);
        }
    }

    if (!is_load || (RN != RD)) {
        if (!P) {
            // Post-indexing implies writeback
            assert(RN != 15);

            if (U) {
                addr += offset;
            } else {
                addr -= offset;
            }

            GPRS[RN] = addr;
        } else if (W) {
            GPRS[RN] = addr;
        }
    }
}

static void i_undefined(const u32 instr) {
    std::printf("Undefined ARM instruction %08X\n", instr);

    dump_registers();
    exit(1);
}

std::unordered_set<u32> jump_targets;

static void add_jump_target(const u32 addr) {
    static std::unordered_set<u32> jump_targets;

    if (jump_targets.find(addr) == jump_targets.end()) {
        std::printf("Jump @ %08X to %08X\n", CPC, addr);

        jump_targets.insert(addr);
    }
}

static void run_instr() {
    // Initialize carry-out for instructions that don't use the barrel shifter
    ctx.carry_out = CPSR.carry;

    assert((PC & 3) == 0);

    CPC = PC;

    const u32 instr = fetch_instr();

    if (check_condition(COND)) {
        instr_table[OPCODE](instr);
    }
    
    if (PC != (CPC + sizeof(u32))) {
        add_jump_target(PC);
    }
}

static void initialize_instr_table() {
    instr_table.fill(i_undefined);

    fill_table_with_pattern(instr_table.data(), "000xxxx0xxx0", i_data_processing<0, 1, 0>);
    fill_table_with_pattern(instr_table.data(), "000xxxx00xx1", i_data_processing<0, 0, 0>);
    fill_table_with_pattern(instr_table.data(), "000xxxx1xxx0", i_data_processing<0, 1, 1>);
    fill_table_with_pattern(instr_table.data(), "000xxxx10xx1", i_data_processing<0, 0, 1>);
    fill_table_with_pattern(instr_table.data(), "000000001001", i_multiply<0, 0>);
    fill_table_with_pattern(instr_table.data(), "000000011001", i_multiply<0, 1>);
    fill_table_with_pattern(instr_table.data(), "000000101001", i_multiply<1, 0>);
    fill_table_with_pattern(instr_table.data(), "000000111001", i_multiply<1, 1>);
    fill_table_with_pattern(instr_table.data(), "00010x001001", i_undefined); // TODO: SWP
    fill_table_with_pattern(instr_table.data(), "000xx0x01011", i_halfword_data_transfer<0, 0>);
    fill_table_with_pattern(instr_table.data(), "000xx1x01011", i_halfword_data_transfer<1, 0>);
    fill_table_with_pattern(instr_table.data(), "000xx0x11011", i_halfword_data_transfer<0, 1>);
    fill_table_with_pattern(instr_table.data(), "000xx1x11011", i_halfword_data_transfer<1, 1>);
    fill_table_with_pattern(instr_table.data(), "000xx0x111x1", i_undefined); // TODO: LDRSB/LDRSH
    fill_table_with_pattern(instr_table.data(), "000xx1x111x1", i_undefined); // TODO: LDRSB/LDRSH
    fill_table_with_pattern(instr_table.data(), "000100000000", i_mrs<0>);
    fill_table_with_pattern(instr_table.data(), "000101000000", i_mrs<1>);
    fill_table_with_pattern(instr_table.data(), "000100100000", i_msr<0, 0>);
    fill_table_with_pattern(instr_table.data(), "000101100000", i_msr<0, 1>);
    fill_table_with_pattern(instr_table.data(), "001xxxx0xxxx", i_data_processing<1, 0, 0>);
    fill_table_with_pattern(instr_table.data(), "001xxxx1xxxx", i_data_processing<1, 0, 1>);
    fill_table_with_pattern(instr_table.data(), "00110x00xxxx", i_undefined);
    fill_table_with_pattern(instr_table.data(), "00110010xxxx", i_msr<1, 0>);
    fill_table_with_pattern(instr_table.data(), "00110110xxxx", i_msr<1, 1>);
    fill_table_with_pattern(instr_table.data(), "010xxxx0xxxx", i_single_data_transfer<1, 0>);
    fill_table_with_pattern(instr_table.data(), "010xxxx1xxxx", i_single_data_transfer<1, 1>);
    fill_table_with_pattern(instr_table.data(), "011xxxx0xxx0", i_single_data_transfer<0, 0>);
    fill_table_with_pattern(instr_table.data(), "011xxxx1xxx0", i_single_data_transfer<0, 1>);
    fill_table_with_pattern(instr_table.data(), "100xxxx0xxxx", i_block_data_transfer<0>);
    fill_table_with_pattern(instr_table.data(), "100xxxx1xxxx", i_block_data_transfer<1>);
    fill_table_with_pattern(instr_table.data(), "1010xxxxxxxx", i_branch<0>);
    fill_table_with_pattern(instr_table.data(), "1011xxxxxxxx", i_branch<1>);
}

void initialize() {
    initialize_instr_table();
}

void reset() {
    constexpr u32 RESET_VECTOR = 0;

    if constexpr (!SILENT_ARM) std::puts("ARM Reset");

    std::memset(&ctx, 0, sizeof(ctx));

    // Set initial CPSR
    CPSR.mode = MODE_USR;
    CPSR.disable_fiq = 1;
    CPSR.disable_irq = 1;

    change_mode(MODE_SVC);

    PC = RESET_VECTOR;
}

void shutdown() {}

void assert_fast_interrupt() {
    ctx.pending_interrupt = true;
}

void clear_fast_interrupt() {
    ctx.pending_interrupt = false;
}

void assert_reset(const bool is_reset) {
    if (is_reset) {
        reset();

        set_state(STATE_SLEEPING);
    } else {
        set_state(STATE_RUNNING);
    }
}

void step() {
    if (ctx.state == STATE_SLEEPING) {
        // Zzz...
        ctx.cycles = 0;

        // TODO: check for interrupts
        return;
    }

    for (; ctx.cycles > 0; ctx.cycles -= 2) {
        run_instr();

        check_pending_interrupts();
    }
}

i64* get_cycles() {
    return &ctx.cycles;
}

}
