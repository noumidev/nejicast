/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/ccn.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hw::cpu::ocio::ccn {

#define PTEH   ctx.page_table.entry_hi
#define PTEL   ctx.page_table.entry_lo
#define TTB    ctx.translation_table_base
#define TEA    ctx.tlb_exception_address
#define MMUCR  ctx.mmu_control
#define CCR    ctx.cache_control
#define TRAPA  ctx.trapa_exception
#define EXPEVT ctx.exception_event
#define INTEVT ctx.interrupt_event
#define PTEA   ctx.page_table.assistance
#define QACR1  ctx.queue_address_control[STORE_QUEUE_1]
#define QACR2  ctx.queue_address_control[STORE_QUEUE_2]

struct {
    struct {
        // TODO: properly implement this
        u32 entry_hi, entry_lo;
        
        union {
            u32 raw;

            struct {
                u32 space_attribute :  3;
                u32 timing_control  :  1;
                u32                 : 28;
            };
        } assistance;
    } page_table;
    
    u32 translation_table_base;
    u32 tlb_exception_address;

    union {
        u32 raw;

        struct {
            u32 enable_translation    : 1;
            u32                       : 1;
            u32 invalidate_tlb        : 1;
            u32                       : 5;
            u32 single_virtual_mode   : 1;
            u32 store_queue_mode      : 1;
            u32 utlb_replace_counter  : 6;
            u32                       : 2;
            u32 utlb_replace_boundary : 6;
            u32                       : 2;
            u32 itlb_lru              : 6;
        };
    } mmu_control;

    union {
        u32 raw;

        struct {
            u32 enable_operand_cache           :  1;
            u32 enable_writethrough            :  1;
            u32 enable_copyback                :  1;
            u32 invalidate_operand_cache       :  1;
            u32                                :  1;
            u32 enable_operand_cache_ram       :  1;
            u32                                :  1;
            u32 enable_operand_cache_index     :  1;
            u32 enable_instruction_cache       :  1;
            u32                                :  2;
            u32 invalidate_instruction_cache   :  1;
            u32                                :  3;
            u32 enable_instruction_cache_index :  1;
            u32                                : 16;
        };
    } cache_control;

    u32 trapa_exception;
    u32 exception_event;
    u32 interrupt_event;

    union {
        u32 raw;
        
        struct {
            u32      :  2;
            u32 area :  3;
            u32      : 27;
        };
    } queue_address_control[NUM_STORE_QUEUES];
} ctx;

void initialize() {}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

u32 get_cache_control() {
    return CCR.raw;
}

u32 get_exception_event() {
    return EXPEVT;
}

u32 get_interrupt_event() {
    return INTEVT;
}

u32 get_store_queue_area(const int store_queue) {
    assert(store_queue < NUM_STORE_QUEUES);

    return ctx.queue_address_control[store_queue].area;
}

void set_page_table_entry_hi(const u32 data) {
    PTEH = data;
}

void set_page_table_entry_lo(const u32 data) {
    PTEL = data;
}

void set_translation_table_base(const u32 data) {
    TTB = data;
}

void set_tlb_exception_address(const u32 data) {
    TEA = data;
}

void set_mmu_control(const u32 data) {
    MMUCR.raw = data;

    if (MMUCR.enable_translation) {
        std::puts("SH-4 MMU enabled");
        exit(1);
    }
}

void set_cache_control(const u32 data) {
    CCR.raw = data;

    if (CCR.invalidate_operand_cache) {
        std::puts("SH-4 invalidate operand cache");

        CCR.invalidate_operand_cache = 0;
    }

    if (CCR.invalidate_instruction_cache) {
        std::puts("SH-4 invalidate instruction cache");

        CCR.invalidate_instruction_cache = 0;
    }
}

void set_trapa_exception(const u32 data) {
    TRAPA = data;
}

void set_exception_event(const u32 data) {
    EXPEVT = data;
}

void set_interrupt_event(const u32 data) {
    INTEVT = data;
}

void set_page_table_assistance(const u32 data) {
    PTEA.raw = data;
}

void set_queue_address_control(const int store_queue, const u32 data) {
    assert(store_queue < NUM_STORE_QUEUES);

    ctx.queue_address_control[store_queue].raw = data;
}

}
