/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
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

constexpr u64 NUM_GPRS = 16;
constexpr u64 NUM_FIQ_ctx = 5;

constexpr u64 SIZE_ARM_TABLE = 4096;

constexpr u32 EXCEPTION_VECTOR_BASE = 0;

enum {
    STATE_RUNNING,
    STATE_SLEEPING,
};

union Instruction {
    u32 raw;

    struct {
        u32                : 28;
        u32 condition_code :  4;
    } none;

    struct {
        u32 rlist : 16;
        u32 rn    :  4;
        u32       :  1;
        u32 w     :  1;
        u32 s     :  1;
        u32 u     :  1;
        u32 p     :  1;
        u32       :  7;
    } bdt;

    struct {
        u32 offset : 24;
        u32 h      :  1;
        u32        :  7;
    } branch;

    struct {
        u32 immediate : 8;
        u32 rotate    : 4;
        u32 rd        : 4;
        u32 rn        : 4;
        u32 s         : 1;
        u32 opcode    : 4;
        u32           : 7;
    } dp_immediate;

    struct {
        u32 rm     : 4;
        u32        : 1;
        u32 shift  : 2;
        u32 amount : 5;
        u32 rd     : 4;
        u32 rn     : 4;
        u32 s      : 1;
        u32 opcode : 4;
        u32        : 7;
    } dp_shift_immediate;

    struct {
        u32 rm     : 4;
        u32        : 1;
        u32 shift  : 2;
        u32        : 1;
        u32 rs     : 4;
        u32 rd     : 4;
        u32 rn     : 4;
        u32 s      : 1;
        u32 opcode : 4;
        u32        : 7;
    } dp_shift_register;

    struct {
        u32 offsetlo : 4;
        u32          : 4;
        u32 offsethi : 4;
        u32 rd       : 4;
        u32 rn       : 4;
        u32          : 1;
        u32 w        : 1;
        u32          : 1;
        u32 u        : 1;
        u32 p        : 1;
        u32          : 7;
    } edt_immediate;

    struct {
        u32 rm : 4;
        u32    : 8;
        u32 rd : 4;
        u32 rn : 4;
        u32    : 1;
        u32 w  : 1;
        u32    : 1;
        u32 u  : 1;
        u32 p  : 1;
        u32    : 7;
    } edt_register;

    struct {
        u32 rm   :  4;
        u32      :  4;
        u32 rs   :  4;
        u32 rdlo :  4;
        u32 rdhi :  4;
        u32 s    :  1;
        u32      : 11;
    } multiply_long;

    struct {
        u32 immediate : 8;
        u32 rotate    : 4;
        u32           : 4;
        u32 mask      : 4;
        u32           : 2;
        u32 r         : 1;
        u32           : 9;
    } msr_immediate;

    struct {
        u32 immediate : 12;
        u32 rd        :  4;
        u32 rn        :  4;
        u32           :  1;
        u32 w         :  1;
        u32 b         :  1;
        u32 u         :  1;
        u32 p         :  1;
        u32           :  7;
    } sdt_immediate;
    
    u32 get_opcode() const {
        return ((raw >> 4) & 0xF) | ((raw >> 16) & 0xFF0);
    }
};

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

enum class AddExtendOpcode {
    SXTB,
};

enum class BranchOpcode {
    B,
    BL,
    BLX,
    Prefix,
};

namespace DataProcessingOpcode {
    enum : u32 {
        AND = 0x0,
        EOR = 0x1,
        SUB = 0x2,
        RSB = 0x3,
        ADD = 0x4,
        ADC = 0x5,
        TST = 0x8,
        TEQ = 0x9,
        CMP = 0xA,
        CMN = 0xB,
        ORR = 0xC,
        MOV = 0xD,
        BIC = 0xE,
        MVN = 0xF,
    };
}

namespace DataProcessingImmediateOpcode {
    enum : u32 {
        MOV = 0,
        CMP = 1,
        ADD = 2,
        SUB = 3,
    };
}

namespace DataProcessingOpcodeTHUMB {
    enum : u32 {
        AND = 0x0,
        EOR = 0x1,
        LSL = 0x2,
        LSR = 0x3,
        ASR = 0x4,
        ADC = 0x5,
        SBC = 0x6,
        ROR = 0x7,
        TST = 0x8,
        NEG = 0x9,
        CMP = 0xA,
        ORR = 0xC,
        MUL = 0xD,
        BIC = 0xE,
        MVN = 0xF,
    };
}

namespace SingleDataTransferOpcode {
    enum : u32 {
        STR   = 0,
        STRH  = 1,
        STRB  = 2,
        LDR   = 4,
        LDRH  = 5,
        LDRB  = 6,
        LDRSH = 7,
    };
}

namespace SpecialDataProcessingOpcode {
    enum : u32 {
        ADD = 0,
        CMP = 1,
        MOV = 2,
    };
}

namespace Mode {
    enum : u32 {
        User = 0x10,
        FastInterrupt = 0x11,
        Interrupt  = 0x12,
        Supervisor = 0x13,
        Abort      = 0x17,
        Undefined  = 0x1B,
        System     = 0x1F,
    };
}

namespace ShiftType {
    enum : u32 {
        LSL,
        LSR,
        ASR,
        ROR,
    };
}

namespace Register {
    enum {
        R0, R1, R2 , R3 , R4 , R5, R6, R7,
        R8, R9, R10, R11, R12, SP, LR, PC,
    };
}

// Program status register
union PSR {
    u32 raw;
    struct {
        u32 mode :  5;
        u32      :  1;
        u32 f    :  1; // FIQ disable
        u32 i    :  1; // IRQ disable
        u32      : 19;
        u32      :  1;
        u32 v    :  1; // Overflow
        u32 c    :  1; // Carry
        u32 z    :  1; // Zero
        u32 n    :  1; // Negative
    };
};

struct {
    u32 r[NUM_GPRS];
    u32 cpc;

    // PSRs
    PSR  cpsr;
    PSR *spsr;

    u32 carryOut;

    // Banked registers
    u32 fiqctx[NUM_FIQ_ctx];

    // Banked stack pointers/link registers
    u32 spFIQ, spSVC, spABT, spIRQ, spUND;
    u32 lrFIQ, lrSVC, lrABT, lrIRQ, lrUND;

    PSR spsrFIQ, spsrSVC, spsrABT, spsrIRQ, spsrUND;

    int state;
    i64 cycles;

    bool isInterruptPending;
} ctx;

std::array<void (*)(Instruction), SIZE_ARM_TABLE> instructionTable;

void dumpctx() {
    std::printf("%08X %08X %08X %08X\n",   ctx.r[0 ], ctx.r[1 ], ctx.r[2 ], ctx.r[3 ]);
    std::printf("%08X %08X %08X %08X\n",   ctx.r[4 ], ctx.r[5 ], ctx.r[6 ], ctx.r[7 ]);
    std::printf("%08X %08X %08X %08X\n",   ctx.r[8 ], ctx.r[9 ], ctx.r[10], ctx.r[11]);
    std::printf("%08X %08X %08X %08X\n\n", ctx.r[12], ctx.r[13], ctx.r[14], ctx.r[15]);
}

// Thanks to https://github.com/PSI-Rockin for this helpful function
template<typename T>
void fillTableEntries(T table[], const char *pattern, T func) {
    u64 mask  = 0;
    u64 value = 0;

    const u64 length = std::strlen(pattern);

    for (u64 i = 0; i < length; i++) {
        const u64 shiftedBit = 1 << (length - i - 1);

        const char bit = pattern[i];
        if (bit == '0') {
            mask  |= shiftedBit;
        } else if (bit == '1') {
            mask  |= shiftedBit;
            value |= shiftedBit;
        }
    }

    for (u64 i = 0; i < (1 << length); i++) {
        if ((i & mask) == value) {
            table[i] = func;
        }
    }
}

static void set_state(const int state) {
    ctx.state = state;
}

void changeMode(const u32 mode) {
    PSR &cpsr = ctx.cpsr;

    if (cpsr.mode == mode) {
        return;
    }

    // Save old register bank
    switch (cpsr.mode) {
        case Mode::User:
        case Mode::System:
            break;
        case Mode::FastInterrupt:
            for (u64 i = 0; i < NUM_FIQ_ctx; i++) {
                std::swap(ctx.fiqctx[i], ctx.r[i + 8]);
            }

            std::swap(ctx.spFIQ, ctx.r[Register::SP]);
            std::swap(ctx.lrFIQ, ctx.r[Register::LR]);
            break;
        case Mode::Interrupt:
            std::swap(ctx.spIRQ, ctx.r[Register::SP]);
            std::swap(ctx.lrIRQ, ctx.r[Register::LR]);
            break;
        case Mode::Supervisor:
            std::swap(ctx.spSVC, ctx.r[Register::SP]);
            std::swap(ctx.lrSVC, ctx.r[Register::LR]);
            break;
        case Mode::Abort:
            std::swap(ctx.spABT, ctx.r[Register::SP]);
            std::swap(ctx.lrABT, ctx.r[Register::LR]);
            break;
        case Mode::Undefined:
            std::swap(ctx.spUND, ctx.r[Register::SP]);
            std::swap(ctx.lrUND, ctx.r[Register::LR]);
            break;
    }

    // Load new register bank
    switch (mode) {
        case Mode::User:
        case Mode::System:
            ctx.spsr = NULL;
            break;
        case Mode::FastInterrupt:
            for (u64 i = 0; i < NUM_FIQ_ctx; i++) {
                std::swap(ctx.fiqctx[i], ctx.r[i + 8]);
            }

            std::swap(ctx.spFIQ, ctx.r[Register::SP]);
            std::swap(ctx.lrFIQ, ctx.r[Register::LR]);

            ctx.spsr = &ctx.spsrFIQ;
            break;
        case Mode::Interrupt:
            std::swap(ctx.spIRQ, ctx.r[Register::SP]);
            std::swap(ctx.lrIRQ, ctx.r[Register::LR]);

            ctx.spsr = &ctx.spsrIRQ;
            break;
        case Mode::Supervisor:
            std::swap(ctx.spSVC, ctx.r[Register::SP]);
            std::swap(ctx.lrSVC, ctx.r[Register::LR]);

            ctx.spsr = &ctx.spsrSVC;
            break;
        case Mode::Abort:
            std::swap(ctx.spABT, ctx.r[Register::SP]);
            std::swap(ctx.lrABT, ctx.r[Register::LR]);

            ctx.spsr = &ctx.spsrABT;
            break;
        case Mode::Undefined:
            std::swap(ctx.spUND, ctx.r[Register::SP]);
            std::swap(ctx.lrUND, ctx.r[Register::LR]);

            ctx.spsr = &ctx.spsrUND;
            break;
        default:
            std::printf("[  ARM  ] Unrecognized CPU mode %02X\n", mode);

            exit(1);
    }

    cpsr.mode = mode;
}

void reloadCPSR() {
    if (ctx.spsr == NULL) {
        std::puts("[  ARM  ] Invalid SPSR\n");

        exit(1);
    }

    const u32 mode = ctx.spsr->mode;

    ctx.cpsr.raw &= 0x1F;
    ctx.cpsr.raw |= ctx.spsr->raw & ~0x1F;

    changeMode(mode);
}

void raiseInterruptException() {
    PSR &cpsr = ctx.cpsr;

    const u32 lr = ctx.r[Register::PC] + 4;

    std::printf("[  ARM  ] IRQ exception (address = %08X)\n", ctx.cpc);

    // Save CPSR
    ctx.spsrIRQ.raw = cpsr.raw;

    cpsr.i = 1;

    changeMode(Mode::Interrupt);

    ctx.r[Register::LR] = lr;
    ctx.r[Register::PC] = EXCEPTION_VECTOR_BASE | 0x18;

    ctx.isInterruptPending = false;
}

void raiseSupervisorException() {
    PSR &cpsr = ctx.cpsr;

    const u32 lr = ctx.r[Register::PC];

    std::printf("[  ARM  ] SVC exception (address = %08X)\n", ctx.cpc);

    exit(1);

    // Save CPSR
    ctx.spsrSVC.raw = cpsr.raw;

    cpsr.i = 1;

    changeMode(Mode::Supervisor);

    ctx.r[Register::LR] = lr;
    ctx.r[Register::PC] = EXCEPTION_VECTOR_BASE | 8;
}

void raiseUndefinedException() {
    PSR &cpsr = ctx.cpsr;

    const u32 lr = ctx.r[Register::PC];

    std::printf("[  ARM  ] UND exception (address = %08X)\n", ctx.cpc);

    // Save CPSR
    ctx.spsrUND.raw = cpsr.raw;

    cpsr.i = 1;

    changeMode(Mode::Undefined);

    ctx.r[Register::LR] = lr;
    ctx.r[Register::PC] = EXCEPTION_VECTOR_BASE | 4;
}

void checkForInterrupts() {
    if (ctx.isInterruptPending && (ctx.cpsr.i == 0)) {
        raiseInterruptException();
    }
}

void setInterruptPending(const bool isInterruptPending) {
    ctx.isInterruptPending = isInterruptPending;

    if (isInterruptPending) {
        set_state(STATE_RUNNING);
    }
}

bool checkCondition(const u32 conditionCode) {
    const PSR &cpsr = ctx.cpsr;
    switch (conditionCode) {
        case CONDITION_CODE_EQ:
            return cpsr.z != 0;
        case CONDITION_CODE_NE:
            return cpsr.z == 0;
        case CONDITION_CODE_HS:
            return cpsr.c != 0;
        case CONDITION_CODE_LO:
            return cpsr.c == 0;
        case CONDITION_CODE_MI:
            return cpsr.n != 0;
        case CONDITION_CODE_PL:
            return cpsr.n == 0;
        case CONDITION_CODE_VS:
            return cpsr.v != 0;
        case CONDITION_CODE_VC:
            return cpsr.v == 0;
        case CONDITION_CODE_HI:
            return (cpsr.c != 0) && (cpsr.z == 0);
        case CONDITION_CODE_LS:
            return (cpsr.z != 0) || (cpsr.c == 0);
        case CONDITION_CODE_GE:
            return cpsr.n == cpsr.v;
        case CONDITION_CODE_LT:
            return cpsr.n != cpsr.v;
        case CONDITION_CODE_GT:
            return (cpsr.n == cpsr.v) && (cpsr.z == 0);
        case CONDITION_CODE_LE:
            return (cpsr.n != cpsr.v) || (cpsr.z != 0);
        case CONDITION_CODE_AL:
            return true;
        default:
            std::puts("ARM Invalid condition code NV");
            exit(1);
    }
}

void setBitFlags(const u32 n) {
    PSR &cpsr = ctx.cpsr;
    cpsr.c = ctx.carryOut;
    cpsr.z = n == 0;
    cpsr.n = (n >> 31) & 1;
}

void setAddFlags(const u32 a, const u32 b, const u32 n) {
    PSR &cpsr = ctx.cpsr;
    cpsr.v = (((a ^ b) >> 31) == 0) && (((a ^ n) >> 31) != 0);
    cpsr.c = (0xFFFFFFFF - a) < b;
    cpsr.z = n == 0;
    cpsr.n = (n >> 31) & 1;
}

void setAddFlagsWithCarry(const u32 a, const u32 b, const u64 n) {
    const u32 temp = a + b;
    const u32 n32 = (u32)n;

    PSR &cpsr = ctx.cpsr;
    cpsr.v = ((((a ^ b) >> 31) == 0) && (((a ^ temp) >> 31) != 0)) || ((((temp ^ ctx.cpsr.c) >> 31) == 0) && (((temp ^ n32) >> 31) != 0));
    cpsr.c = (n >> 32) & 1;
    cpsr.z = n32 == 0;
    cpsr.n = (n >> 31) & 1;
}

void setSubFlags(const u32 a, const u32 b, const u32 n) {
    PSR &cpsr = ctx.cpsr;
    cpsr.v = (((a ^ b) >> 31) != 0) && (((a ^ n) >> 31) != 0);
    cpsr.c = a >= b;
    cpsr.z = n == 0;
    cpsr.n = (n >> 31) & 1;
}

void setSubFlagsWithCarry(const u32 a, const u32 b, const u32 n) {
    const u32 carryIn = ctx.cpsr.c ^ 1;

    const u32 temp1 = a - b;
    const u32 temp2 = temp1 - carryIn;

    PSR &cpsr = ctx.cpsr;
    cpsr.v = ((((a ^ b) >> 31) != 0) && (((a ^ temp1) >> 31) != 0)) || ((((temp1 ^ carryIn) >> 31) != 0) && (((temp1 ^ temp2) >> 31) != 0));
    cpsr.c = (a >= b) || (temp1 >= carryIn);
    cpsr.z = n == 0;
    cpsr.n = (n >> 31) & 1;
}

template<u32 shiftType, bool isImmediate>
u32 shift(const u32 data, u32 amount) {
    amount &= 0xFF;

    switch (shiftType) {
        case ShiftType::LSL:
            if (amount == 0) {
                // Don't set flags
                ctx.carryOut = ctx.cpsr.c;

                return data;
            }

            if (amount >= 32) {
                if (amount == 32) {
                    ctx.carryOut = data & 1;
                } else {
                    ctx.carryOut = 0;
                }

                return 0;
            }

            ctx.carryOut = ((data << (amount - 1)) >> 31) & 1;

            return data << amount;
        case ShiftType::LSR:
            if (amount == 0) {
                if constexpr (!isImmediate) {
                    ctx.carryOut = ctx.cpsr.c;

                    return data;
                }

                amount = 32;
            }

            if (amount >= 32) {
                if (amount == 32) {
                    ctx.carryOut = (data >> 31) & 1;
                } else {
                    ctx.carryOut = 0;
                }

                return 0;
            }

            ctx.carryOut = (data >> (amount - 1)) & 1;

            return data >> amount;
        case ShiftType::ASR:
            {
                if (amount == 0) {
                    if constexpr (!isImmediate) {
                        ctx.carryOut = ctx.cpsr.c;

                        return data;
                    }

                    amount = 32;
                }

                if (amount >= 32) {
                    const u32 sign = data >> 31;

                    ctx.carryOut = sign;

                    return 0 - sign;
                }
                
                ctx.carryOut = (data >> (amount - 1)) & 1;

                return (u32)((i32)data >> amount);
            }
        case ShiftType::ROR:
            {
                if (!isImmediate || (amount != 0)) {
                    if (amount == 0) {
                        ctx.carryOut = ctx.cpsr.c;

                        return data;
                    }

                    amount &= 0x1F;

                    const u32 out = std::rotr(data, amount - 1);

                    ctx.carryOut = data & 1;

                    return std::rotr(out, 1);
                } else {
                    // RRX
                    ctx.carryOut = data & 1;

                    return (data >> 1) | (ctx.cpsr.c << 31);
                }
            }
    }
}

template<typename T>
static T read(const u32 addr) {
    if ((addr & (sizeof(T) - 1)) != 0) {
        std::printf("Unaligned ARM read%zu @ %08X\n", 8 * sizeof(T), addr);
        exit(1);
    }

    return bus::read<T>(addr);
}

template<typename T>
static void write(const u32 addr, const u32 data) {
    if ((addr & (sizeof(T) - 1)) != 0) {
        std::printf("Unaligned ARM write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
        exit(1);
    }

    return bus::write<T>(addr, data);
}

u32 rotateImmediate(const u32 immediate, u32 amount) {
    if (amount == 0) {
        ctx.carryOut = ctx.cpsr.c;

        return immediate;
    }

    amount <<= 1;

    ctx.carryOut = (immediate & (1 << (amount - 1))) != 0;

    return std::rotr(immediate, amount);
}

void BLX_Imm(const Instruction instr) {
    const u32 offset = (u32)(((i32)instr.branch.offset << 8) >> 6) | (instr.branch.h << 1);

    const u32 pc = ctx.r[Register::PC];

    ctx.r[Register::LR] = pc;
    ctx.r[Register::PC] = pc + offset + 4;

    // Switch to THUMB unconditionally
}

void BLX_Reg(const Instruction instr) {
    const u32 rm = instr.dp_shift_immediate.rm;
    if (rm == Register::PC) {
        std::puts("[  ARM  ] Invalid BLX source register");

        exit(1);
    }

    ctx.r[Register::LR] = ctx.r[Register::PC];

    const u32 target = ctx.r[rm];

    ctx.r[Register::PC] = target & ~1;
}

void BX(const Instruction instr) {
    const u32 rm = instr.dp_shift_immediate.rm;
    if (rm == Register::PC) {
        std::puts("[  ARM  ] Invalid BX source register");

        exit(1);
    }

    const u32 target = ctx.r[rm];

    ctx.r[Register::PC] = target & ~1;
}

void CLZ(const Instruction instr) {
    const u32 rd = instr.dp_shift_immediate.rd;
    const u32 rm = instr.dp_shift_immediate.rm;
    if (rm == Register::PC) {
        std::puts("[  ARM  ] Invalid CLZ source register");

        exit(1);
    }
    if (rd == Register::PC) {
        std::puts("[  ARM  ] Invalid CLZ destination register");

        exit(1);
    }

    ctx.r[rd] = std::countl_zero(ctx.r[rm]);
}

void MRS(const Instruction instr) {
    const u32 rd = instr.dp_immediate.rd;
    if (rd == Register::PC) {
        std::puts("[  ARM  ] Invalid MRS destination register");

        exit(1);
    }

    const bool isSaved = instr.msr_immediate.r != 0;
    if (isSaved) {
        if (ctx.spsr == NULL) {
            std::puts("[  ARM  ] Invalid SPSR");

            exit(1);
        }

        ctx.r[rd] = ctx.spsr->raw;
    } else {
        ctx.r[rd] = ctx.cpsr.raw;
    }
}

void SVC(const Instruction instr) {
    (void)instr;

    raiseSupervisorException();
}

void UDF(const Instruction instr) {
    (void)instr;

    raiseUndefinedException();
}

template<AddExtendOpcode op>
void doAddExtend(const Instruction instr) {
    const u32 rd = instr.dp_shift_register.rd;
    const u32 rm = instr.dp_shift_register.rm;

    const u32 rotate = 8 * (instr.dp_shift_register.rs >> 2);

    switch (op) {
        case AddExtendOpcode::SXTB:
            ctx.r[rd] = (u32)(i8)(std::rotr(ctx.r[rm], rotate));
            break;
    }
}

template<bool isLoad>
void doBlockDataTransfer(const Instruction instr) {
    const u32 rn = instr.bdt.rn;
    if (rn == Register::PC) {
        std::puts("[  ARM  ] Invalid base address register");

        exit(1);
    }

    const bool isWriteBack = instr.bdt.w != 0;
    const bool isSpecial = instr.bdt.s != 0;
    const bool isUp = instr.bdt.u != 0;

    bool isPreIndex = instr.bdt.p != 0;

    const u32 rlist = instr.bdt.rlist;
    if (rlist == 0) {
        std::puts("[  ARM  ] Invalid rlist");

        exit(1);
    }

    u32 addr = ctx.r[rn];
    if (!isUp) {
        addr -= 4 * std::popcount(rlist);

        isPreIndex = !isPreIndex;
    }

    u32 mode = ctx.cpsr.mode;
    if (isSpecial && (!isLoad || ((rlist & (1 << Register::PC)) == 0))) {
        changeMode(Mode::User);
    }

    for (u32 reglist = rlist; reglist != 0;) {
        const u32 i = std::countr_zero(reglist);

        if (isPreIndex) {
            addr += 4;
        }

        if constexpr (isLoad) {
            ctx.r[i] = read<u32>(addr & ~3);

            if (i == Register::PC) {
                // State change
                ctx.r[i] &= ~1;
            }
        } else {
            u32 data = ctx.r[i];
            if (i == Register::PC) {
                data += 8;
            }

            write<u32>(addr & ~3, data);
        }

        if (!isPreIndex) {
            addr += 4;
        }

        reglist ^= 1 << i;
    }

    if (isWriteBack) {
        if (!isUp) {
            addr -= 4 * std::popcount(rlist);
        }

        if (!isLoad || ((rlist & (1 << rn)) == 0) || ((31U - std::countl_zero(rlist)) != rn)) {
            ctx.r[rn] = addr;
        }
    }

    if (isSpecial) {
        if (!isLoad || ((rlist & (1 << Register::PC)) == 0)) {
            changeMode(mode);
        } else {
            reloadCPSR();
        }
    }
}

template<bool isLink>
void doBranch(const Instruction instr) {
    const u32 offset = (u32)(((i32)instr.branch.offset << 8) >> 6);

    const u32 pc = ctx.r[Register::PC];
    if constexpr (isLink) {
        ctx.r[Register::LR] = pc;
    }

    ctx.r[Register::PC] = pc + offset + 4;
}

template<bool isImmediate, bool isImmediateShift>
void doDataProcessing(const Instruction instr) {
    const u32 rd = instr.dp_immediate.rd;
    const u32 rn = instr.dp_immediate.rn;

    const u32 opcode = instr.dp_immediate.opcode;

    const bool isSpecial = instr.dp_immediate.s != 0;
    const bool setFlags = isSpecial && (rd != Register::PC);

    u32 op1 = ctx.r[rn];
    if (rn == Register::PC) {
        op1 += 4;
    }

    // Decode op2
    u32 op2;
    if constexpr (isImmediate) {
        op2 = rotateImmediate(instr.dp_immediate.immediate, instr.dp_immediate.rotate);
    } else {
        const u32 rm = instr.dp_shift_immediate.rm;

        op2 = ctx.r[rm];
        if (rm == Register::PC) {
            op2 += 4;
        }

        u32 amount;
        if constexpr (isImmediateShift) {
            amount = instr.dp_shift_immediate.amount;
        } else {
            amount = ctx.r[instr.dp_shift_register.rs];

            if (rn == Register::PC) {
                op1 += 4;
            }

            if (rm == Register::PC) {
                op2 += 4;
            }
        }

        switch (instr.dp_shift_immediate.shift) {
            case ShiftType::LSL:
                op2 = shift<ShiftType::LSL, isImmediateShift>(op2, amount);
                break;
            case ShiftType::LSR:
                op2 = shift<ShiftType::LSR, isImmediateShift>(op2, amount);
                break;
            case ShiftType::ASR:
                op2 = shift<ShiftType::ASR, isImmediateShift>(op2, amount);
                break;
            case ShiftType::ROR:
                op2 = shift<ShiftType::ROR, isImmediateShift>(op2, amount);
                break;
        }
    }

    switch (opcode) {
        case DataProcessingOpcode::AND:
            ctx.r[rd] = op1 & op2;

            if (setFlags) {
                setBitFlags(ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::EOR:
            ctx.r[rd] = op1 ^ op2;

            if (setFlags) {
                setBitFlags(ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::SUB:
            ctx.r[rd] = op1 - op2;

            if (setFlags) {
                setSubFlags(op1, op2, ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::RSB:
            ctx.r[rd] = op2 - op1;

            if (setFlags) {
                setSubFlags(op2, op1, ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::ADD:
            ctx.r[rd] = op1 + op2;

            if (setFlags) {
                setAddFlags(op1, op2, ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::ADC:
            {
                const u64 n = (u64)op1 + (u64)op2 + (u64)ctx.cpsr.c;

                if (setFlags) {
                    setAddFlagsWithCarry(op1, op2, n);
                }

                ctx.r[rd] = (u32)n;
            }
            break;
        case DataProcessingOpcode::TST:
            setBitFlags(op1 & op2);
            break;
        case DataProcessingOpcode::TEQ:
            setBitFlags(op1 ^ op2);
            break;
        case DataProcessingOpcode::CMP:
            setSubFlags(op1, op2, op1 - op2);
            break;
        case DataProcessingOpcode::CMN:
            setAddFlags(op1, op2, op1 + op2);
            break;
        case DataProcessingOpcode::ORR:
            ctx.r[rd] = op1 | op2;

            if (setFlags) {
                setBitFlags(ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::MOV:
            if (setFlags) {
                setBitFlags(op2);
            }

            ctx.r[rd] = op2;

            if (!isSpecial && (rd == Register::PC)) {
                ctx.r[rd] &= ~1;
            }
            break;
        case DataProcessingOpcode::BIC:
            ctx.r[rd] = op1 & ~op2;

            if (setFlags) {
                setBitFlags(ctx.r[rd]);
            }
            break;
        case DataProcessingOpcode::MVN:
            ctx.r[rd] = ~op2;

            if (setFlags) {
                setBitFlags(ctx.r[rd]);
            }
            break;
        default:
            std::printf("[  ARM  ] Unrecognized Data Processing opcode %X\n", opcode);

            exit(1);
    }

    if (isSpecial && (rd == Register::PC)) {
        reloadCPSR();
    }
}

template<bool isImmediate, bool isLoad>
void doDoublewordDataTransfer(const Instruction instr) {
    const u32 rd = instr.edt_immediate.rd;
    const u32 rn = instr.edt_immediate.rn;

    const bool isWriteBack = instr.edt_immediate.w != 0;
    const bool isUp = instr.edt_immediate.u != 0;
    const bool isPreIndex = instr.edt_immediate.p != 0;

    if (((rd & 1) != 0) || (rd >= Register::LR)) {
        std::puts("[  ARM  ] Invalid DDT destination register");

        exit(1);
    }

    if (!isPreIndex && isWriteBack) {
        std::puts("[  ARM  ] Unimplemented unprivileged memory access");

        exit(1);
    }

    u32 addr = ctx.r[rn];

    u32 offset;
    if constexpr (isImmediate) {
        offset = (instr.edt_immediate.offsethi << 4) | instr.edt_immediate.offsetlo;
    } else {
        const u32 rm = instr.edt_register.rm;
        if (rm == Register::PC) {
            std::puts("[  ARM  ] Invalid offset register");

            exit(1);
        }

        offset = ctx.r[rm];
    }

    if (isPreIndex) {
        if (isUp) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    if constexpr (isLoad) {
        ctx.r[rd + 0] = read<u32>(addr + 0);
        ctx.r[rd + 4] = read<u32>(addr + 4);
    } else {
        write<u32>(addr + 0, ctx.r[rd + 0]);
        write<u32>(addr + 4, ctx.r[rd + 1]);
    }

    if (!isLoad || (rn != rd)) {
        if (!isPreIndex) {
            // Post-indexing implies writeback
            if ((rn == rd) || (rn == (rd + 1)) || (rn == Register::PC)) {
                std::puts("[  ARM  ] Invalid writeback register");

                exit(1);
            }

            if (isUp) {
                addr += offset;
            } else {
                addr -= offset;
            }

            ctx.r[rn] = addr;
        } else if (isWriteBack) {
            ctx.r[rn] = addr;
        }
    }
}

template<bool isImmediate, bool isLoad>
void doHalfwordDataTransfer(const Instruction instr) {
    const u32 rd = instr.edt_immediate.rd;
    const u32 rn = instr.edt_immediate.rn;

    const bool isWriteBack = instr.edt_immediate.w != 0;
    const bool isUp = instr.edt_immediate.u != 0;
    const bool isPreIndex = instr.edt_immediate.p != 0;

    if (rd == Register::PC) {
        std::puts("[  ARM  ] Invalid HDT destination register");

        exit(1);
    }

    if (!isPreIndex && isWriteBack) {
        std::puts("[  ARM  ] Unimplemented unprivileged memory access");

        exit(1);
    }

    u32 addr = ctx.r[rn];
    if (rn == Register::PC) {
        addr += 4;
    }

    u32 offset;
    if constexpr (isImmediate) {
        offset = (instr.edt_immediate.offsethi << 4) | instr.edt_immediate.offsetlo;
    } else {
        const u32 rm = instr.edt_register.rm;
        if (rm == Register::PC) {
            std::puts("[  ARM  ] Invalid offset register");

            exit(1);
        }

        offset = ctx.r[rm];
    }

    if (isPreIndex) {
        if (isUp) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    if constexpr (isLoad) {
        if ((addr & 1) != 0) {
            std::printf("[  ARM  ] Unaligned LDRH address %08X\n", addr);

            exit(1);
        }

        ctx.r[rd] = read<u16>(addr);
    } else {
        write<u16>(addr & ~1, ctx.r[rd]);
    }

    if (!isLoad || (rn != rd)) {
        if (!isPreIndex) {
            // Post-indexing implies writeback
            if (rn == Register::PC) {
                std::puts("[  ARM  ] Invalid writeback register");

                exit(1);
            }

            if (isUp) {
                addr += offset;
            } else {
                addr -= offset;
            }

            ctx.r[rn] = addr;
        } else if (isWriteBack) {
            ctx.r[rn] = addr;
        }
    }
}

template<bool isImmediate>
void doMoveToStatusRegister(const Instruction instr) {
    u32 op;
    if constexpr (isImmediate) {
        op = rotateImmediate(instr.dp_immediate.immediate, instr.dp_immediate.rotate);
    } else {
        op = ctx.r[instr.dp_shift_immediate.rm];
    }

    u32 mask = instr.msr_immediate.mask;
    if (ctx.cpsr.mode == Mode::User) {
        mask &= 8;
    }

    const bool isSaved = instr.msr_immediate.r != 0;

    u32 mode;

    PSR *psr;
    if (isSaved) {
        psr = ctx.spsr;
    } else {
        psr = &ctx.cpsr;

        if ((mask & 1) != 0) {
            mode = psr->mode;
        }
    }

    if (psr == NULL) {
        std::puts("[  ARM  ] Invalid SPSR");

        exit(1);
    }

    u32 psrMask = 0;
    if ((mask & 1) != 0) {
        psrMask |= 0xFF;
    }

    if ((mask & 2) != 0) {
        psrMask |= 0xFF00;
    }

    if ((mask & 4) != 0) {
        psrMask |= 0xFF0000;
    }

    if ((mask & 8) != 0) {
        psrMask |= 0xFF000000;
    }

    psr->raw &= ~psrMask;
    psr->raw |= op & psrMask;

    if (!isSaved && ((mask & 1) != 0)) {
        const u32 newMode = op & 0x1F;

        psr->mode = mode;

        changeMode(newMode);
    }
}

template<bool isAccumulate>
void doMultiply(const Instruction instr) {
    const u32 rd = instr.multiply_long.rdhi;
    const u32 rm = instr.multiply_long.rm;
    const u32 rn = instr.multiply_long.rdlo;
    const u32 rs = instr.multiply_long.rs;

    if ((rm == Register::PC) || (rn == Register::PC) || (rs == Register::PC)) {
        std::puts("[  ARM  ] Invalid MUL source register");

        exit(1);
    }

    if ((rd == Register::PC)) {
        std::puts("[  ARM  ] Invalid MUL destination register");

        exit(1);
    }

    const bool setFlags = instr.multiply_long.s != 0;

    u32 n = ctx.r[rm] * ctx.r[rs];
    if constexpr (isAccumulate) {
        n += ctx.r[rn];
    }

    ctx.r[rd] = n;

    if (setFlags) {
        setBitFlags(n);
    }
}

template<bool isSigned, bool isAccumulate>
void domultiply_long(const Instruction instr) {
    const u32 rdhi = instr.multiply_long.rdhi;
    const u32 rdlo = instr.multiply_long.rdlo;

    const u32 rm = instr.multiply_long.rm;
    const u32 rs = instr.multiply_long.rs;

    if ((rm == Register::PC) || (rs == Register::PC)) {
        std::puts("[  ARM  ] Invalid MULL source register");

        exit(1);
    }

    if ((rdhi == Register::PC) || (rdlo == Register::PC) || (rdhi == rdlo)) {
        std::puts("[  ARM  ] Invalid MULL destination register");

        exit(1);
    }

    const bool setFlags = instr.multiply_long.s != 0;

    const u64 accumulator = ((u64)ctx.r[rdhi] << 32) | (u64)ctx.r[rdlo];

    u64 n;
    if constexpr (isSigned) {
        n = (i64)(i32)ctx.r[rm] * (i64)(i32)ctx.r[rs];
    } else {
        n = (u64)ctx.r[rm] * (u64)ctx.r[rs];
    }

    if constexpr (isAccumulate) {
        n += accumulator;
    }

    ctx.r[rdlo] = (u32)n;
    ctx.r[rdhi] = (u32)(n >> 32);

    if (setFlags) {
        ctx.cpsr.z = n == 0;
        ctx.cpsr.n = (n >> 63) & 1;
    }
}

template<bool isImmediate, bool isLoad>
void doSingleDataTransfer(const Instruction instr) {
    const u32 rd = instr.sdt_immediate.rd;
    const u32 rn = instr.sdt_immediate.rn;

    const bool isWriteBack = instr.sdt_immediate.w != 0;
    const bool isByte = instr.sdt_immediate.b != 0;
    const bool isUp = instr.sdt_immediate.u != 0;
    const bool isPreIndex = instr.sdt_immediate.p != 0;

    if (!isPreIndex && isWriteBack) {
        std::puts("[  ARM  ] Unimplemented unprivileged memory access");

        exit(1);
    }

    u32 addr = ctx.r[rn];
    if (rn == Register::PC) {
        addr += 4;
    }

    u32 offset;
    if constexpr (isImmediate) {
        offset = instr.sdt_immediate.immediate;
    } else {
        const u32 rm = instr.dp_shift_immediate.rm;

        offset = ctx.r[rm];
        if (rm == Register::PC) {
            offset += 4;
        }

        const u32 amount = instr.dp_shift_immediate.amount;

        switch (instr.dp_shift_immediate.shift) {
            case ShiftType::LSL:
                offset = shift<ShiftType::LSL, true>(offset, amount);
                break;
            case ShiftType::LSR:
                offset = shift<ShiftType::LSR, true>(offset, amount);
                break;
            case ShiftType::ASR:
                offset = shift<ShiftType::ASR, true>(offset, amount);
                break;
            case ShiftType::ROR:
                offset = shift<ShiftType::ROR, true>(offset, amount);
                break;
        }
    }

    if (isPreIndex) {
        if (isUp) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    if constexpr (isLoad) {
        if (isByte) {
            ctx.r[rd] = read<u8>(addr);
        } else {
            ctx.r[rd] = read<u32>(addr);

            if (rd == Register::PC) {
                ctx.r[rd] &= ~1;
            }
        }
    } else {
        u32 data = ctx.r[rd];
        if (rd == Register::PC) {
            addr += 8;
        }

        if (isByte) {
            write<u8>(addr, (u8)data);
        } else {
            write<u32>(addr & ~3, data);
        }
    }

    if (!isLoad || (rn != rd)) {
        if (!isPreIndex) {
            // Post-indexing implies writeback
            if (rn == Register::PC) {
                std::puts("[  ARM  ] Invalid writeback register");

                exit(1);
            }

            if (isUp) {
                addr += offset;
            } else {
                addr -= offset;
            }

            ctx.r[rn] = addr;
        } else if (isWriteBack) {
            ctx.r[rn] = addr;
        }
    }
}

void undefinedInstruction(const Instruction instr) {
    std::printf("[  ARM  ] Unrecognized instruction %08X (opcode = %03X, PC = %08X)\n", instr.raw, instr.get_opcode(), ctx.cpc);

    exit(1);
}

std::unordered_set<u32> jump_targets;

static void add_jump_target(const u32 addr) {
    static std::unordered_set<u32> jump_targets;

    if (jump_targets.find(addr) == jump_targets.end()) {
        std::printf("Jump @ %08X to %08X\n", ctx.cpc, addr);

        jump_targets.insert(addr);
    }
}

void decodeARM() {
    u32 &pc = ctx.r[Register::PC];
    pc &= ~3;

    ctx.cpc = pc;

    if ((pc & 3) != 0) {
        std::printf("[  ARM  ] Unaligned PC (address = %08X)\n", pc);

        exit(1);
    }

    const Instruction instr{.raw = read<u32>(ctx.cpc)};

    pc += sizeof(Instruction);

    const u32 conditionCode = instr.none.condition_code;

    if (checkCondition(conditionCode)) {
        instructionTable[instr.get_opcode()](instr);
    }
    
    if (pc != (ctx.cpc + 4)) {
        add_jump_target(pc);
    }
}

static void initTables() {
    instructionTable.fill(undefinedInstruction);

    fillTableEntries(instructionTable.data(), "000xxxxxxxx0", doDataProcessing<0, 1>);
    fillTableEntries(instructionTable.data(), "000xxxxx0xx1", doDataProcessing<0, 0>);
    fillTableEntries(instructionTable.data(), "0000000x1001", doMultiply<0>);
    fillTableEntries(instructionTable.data(), "0000001x1001", doMultiply<1>);
    fillTableEntries(instructionTable.data(), "0000100x1001", domultiply_long<0, 0>);
    fillTableEntries(instructionTable.data(), "0000101x1001", domultiply_long<0, 1>);
    fillTableEntries(instructionTable.data(), "0000110x1001", domultiply_long<1, 0>);
    fillTableEntries(instructionTable.data(), "0000111x1001", domultiply_long<1, 1>);
    fillTableEntries(instructionTable.data(), "00010x001001", undefinedInstruction); // TODO: SWP
    fillTableEntries(instructionTable.data(), "000xx0x01011", doHalfwordDataTransfer<0, 0>);
    fillTableEntries(instructionTable.data(), "000xx1x01011", doHalfwordDataTransfer<1, 0>);
    fillTableEntries(instructionTable.data(), "000xx0x11011", doHalfwordDataTransfer<0, 1>);
    fillTableEntries(instructionTable.data(), "000xx1x11011", doHalfwordDataTransfer<1, 1>);
    fillTableEntries(instructionTable.data(), "000xx0x01101", doDoublewordDataTransfer<0, 1>);
    fillTableEntries(instructionTable.data(), "000xx1x01101", doDoublewordDataTransfer<1, 1>);
    fillTableEntries(instructionTable.data(), "000xx0x111x1", undefinedInstruction); // TODO: LDRSB/LDRSH
    fillTableEntries(instructionTable.data(), "000xx0x01111", doDoublewordDataTransfer<0, 0>);
    fillTableEntries(instructionTable.data(), "000xx1x01111", doDoublewordDataTransfer<1, 0>);
    fillTableEntries(instructionTable.data(), "000xx1x111x1", undefinedInstruction); // TODO: LDRSB/LDRSH
    fillTableEntries(instructionTable.data(), "00010x000000", MRS); // TODO: MRS
    fillTableEntries(instructionTable.data(), "00010x100000", doMoveToStatusRegister<0>); // TODO: MSR
    fillTableEntries(instructionTable.data(), "000100100001", BX);
    fillTableEntries(instructionTable.data(), "000100100011", BLX_Reg);
    fillTableEntries(instructionTable.data(), "000101100001", CLZ);
    fillTableEntries(instructionTable.data(), "00010xx00101", undefinedInstruction); // TODO: DSP add/subtract
    fillTableEntries(instructionTable.data(), "000100100111", undefinedInstruction); // TODO: breakpoint
    fillTableEntries(instructionTable.data(), "00010xx01xx0", undefinedInstruction); // TODO: DSP multiply
    fillTableEntries(instructionTable.data(), "001xxxxxxxxx", doDataProcessing<1, 0>);
    fillTableEntries(instructionTable.data(), "00110x00xxxx", undefinedInstruction);
    fillTableEntries(instructionTable.data(), "00110x10xxxx", doMoveToStatusRegister<1>);
    fillTableEntries(instructionTable.data(), "010xxxx0xxxx", doSingleDataTransfer<1, 0>);
    fillTableEntries(instructionTable.data(), "010xxxx1xxxx", doSingleDataTransfer<1, 1>);
    fillTableEntries(instructionTable.data(), "011xxxx0xxx0", doSingleDataTransfer<0, 0>);
    fillTableEntries(instructionTable.data(), "011xxxx1xxx0", doSingleDataTransfer<0, 1>);
    fillTableEntries(instructionTable.data(), "011xxxxxxxx1", UDF);
    fillTableEntries(instructionTable.data(), "011010100111", doAddExtend<AddExtendOpcode::SXTB>);
    fillTableEntries(instructionTable.data(), "100xxxx0xxxx", doBlockDataTransfer<0>);
    fillTableEntries(instructionTable.data(), "100xxxx1xxxx", doBlockDataTransfer<1>);
    fillTableEntries(instructionTable.data(), "1010xxxxxxxx", doBranch<0>);
    fillTableEntries(instructionTable.data(), "1011xxxxxxxx", doBranch<1>);
    fillTableEntries(instructionTable.data(), "1111xxxxxxxx", SVC);
}

void initialize() {
    initTables();
}

void reset() {
    std::puts("ARM Reset");

    std::memset(&ctx, 0, sizeof(ctx));

    // Set initial CPSR
    PSR &cpsr = ctx.cpsr;
    cpsr.mode = Mode::User;
    cpsr.f = 1;
    cpsr.i = 1;

    changeMode(Mode::Supervisor);

    ctx.r[Register::PC] = EXCEPTION_VECTOR_BASE;
}

void shutdown() {}

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

    for (; ctx.cycles > 0; ctx.cycles--) {
        ctx.carryOut = ctx.cpsr.c;

        decodeARM();

        checkForInterrupts();
    }
}

i64* get_cycles() {
    return &ctx.cycles;
}

}
