/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/types.hpp>

// SuperH cache controller I/O
namespace hw::cpu::ocio::ccn {

enum {
    STORE_QUEUE_1,
    STORE_QUEUE_2,
    NUM_STORE_QUEUES,
};

void initialize();
void reset();
void shutdown();

u32 get_mmu_control();
u32 get_cache_control();
u32 get_exception_event();
u32 get_interrupt_event();

u32 get_store_queue_area(const int store_queue);

void set_page_table_entry_hi(const u32 data);
void set_page_table_entry_lo(const u32 data);
void set_translation_table_base(const u32 data);
void set_tlb_exception_address(const u32 data);
void set_mmu_control(const u32 data);
void set_cache_control(const u32 data);
void set_trapa_exception(const u32 data);
void set_exception_event(const u32 data);
void set_interrupt_event(const u32 data);
void set_page_table_assistance(const u32 data);
void set_queue_address_control(const int store_queue, const u32 data);

}
