/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/ocio.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hw/cpu/bsc.hpp>
#include <hw/cpu/ccn.hpp>
#include <hw/cpu/cpg.hpp>
#include <hw/cpu/dmac.hpp>
#include <hw/cpu/intc.hpp>
#include <hw/cpu/prfc.hpp>
#include <hw/cpu/rtc.hpp>
#include <hw/cpu/scif.hpp>
#include <hw/cpu/tmu.hpp>
#include <hw/cpu/ubc.hpp>
#include <hw/holly/bus.hpp>

namespace hw::cpu::ocio {

constexpr bool SILENT_SCIF = true;

enum : u32 {
    BASE_OPERAND_CACHE_TAG = 0x14000000,
};

enum : u32 {
    IO_PTEH    = 0x1F000000,
    IO_PTEL    = 0x1F000004,
    IO_TTB     = 0x1F000008,
    IO_TEA     = 0x1F00000C,
    IO_MMUCR   = 0x1F000010,
    IO_BASRA   = 0x1F000014,
    IO_BASRB   = 0x1F000018,
    IO_CCR     = 0x1F00001C,
    IO_TRAPA   = 0x1F000020,
    IO_EXPEVT  = 0x1F000024,
    IO_INTEVT  = 0x1F000028,
    IO_CPUVER  = 0x1F000030,
    IO_PTEA    = 0x1F000034,
    IO_QACR1   = 0x1F000038,
    IO_QACR2   = 0x1F00003C,
    IO_PMCR0   = 0x1F000084,
    IO_BARA    = 0x1F200000,
    IO_BAMRA   = 0x1F200004,
    IO_BBRA    = 0x1F200008,
    IO_BARB    = 0x1F20000C,
    IO_BAMRB   = 0x1F200010,
    IO_BBRB    = 0x1F200014,
    IO_BRCR    = 0x1F200020,
    IO_BCR1    = 0x1F800000,
    IO_BCR2    = 0x1F800004,
    IO_WCR1    = 0x1F800008,
    IO_WCR2    = 0x1F80000C,
    IO_WCR3    = 0x1F800010,
    IO_MCR     = 0x1F800014,
    IO_PCR     = 0x1F800018,
    IO_RTCSR   = 0x1F80001C,
    IO_RTCOR   = 0x1F800024,
    IO_RFCR    = 0x1F800028,
    IO_PCTRA   = 0x1F80002C,
    IO_PDTRA   = 0x1F800030,
    IO_PCTRB   = 0x1F800040,
    IO_PDTRB   = 0x1F800044,
    IO_GPIOIC  = 0x1F800048,
    IO_SDMR3   = 0x1F940000,
    IO_SAR0    = 0x1FA00000,
    IO_DAR0    = 0x1FA00004,
    IO_DMATCR0 = 0x1FA00008,
    IO_CHCR0   = 0x1FA0000C,
    IO_SAR1    = 0x1FA00010,
    IO_DAR1    = 0x1FA00014,
    IO_DMATCR1 = 0x1FA00018,
    IO_CHCR1   = 0x1FA0001C,
    IO_SAR2    = 0x1FA00020,
    IO_DAR2    = 0x1FA00024,
    IO_DMATCR2 = 0x1FA00028,
    IO_CHCR2   = 0x1FA0002C,
    IO_SAR3    = 0x1FA00030,
    IO_DAR3    = 0x1FA00034,
    IO_DMATCR3 = 0x1FA00038,
    IO_CHCR3   = 0x1FA0003C,
    IO_DMAOR   = 0x1FA00040,
    IO_STBCR   = 0x1FC00004,
    IO_WTCNT   = 0x1FC00008,
    IO_WTCSR   = 0x1FC0000C,
    IO_STBCR2  = 0x1FC00010,
    IO_RMONAR  = 0x1FC80034,
    IO_RCR1    = 0x1FC80038,
    IO_ICR     = 0x1FD00000,
    IO_IPRA    = 0x1FD00004,
    IO_IPRB    = 0x1FD00008,
    IO_IPRC    = 0x1FD0000C,
    IO_TOCR    = 0x1FD80000,
    IO_TSTR    = 0x1FD80004,
    IO_TCOR0   = 0x1FD80008,
    IO_TCNT0   = 0x1FD8000C,
    IO_TCR0    = 0x1FD80010,
    IO_TCOR1   = 0x1FD80014,
    IO_TCNT1   = 0x1FD80018,
    IO_TCR1    = 0x1FD8001C,
    IO_TCOR2   = 0x1FD80020,
    IO_TCNT2   = 0x1FD80024,
    IO_TCR2    = 0x1FD80028,
    IO_SCSMR2  = 0x1FE80000,
    IO_SCBRR2  = 0x1FE80004,
    IO_SCSCR2  = 0x1FE80008,
    IO_SCFTDR2 = 0x1FE8000C,
    IO_SCFSR2  = 0x1FE80010,
    IO_SCFCR2  = 0x1FE80018,
    IO_SCSPTR2 = 0x1FE80020,
    IO_SCLSR2  = 0x1FE80024,
};

enum {
    SIZE_STORE_QUEUE_AREA = 0x4000000,
};

struct {
    struct {
        u32 bytes[8];
    } store_queues[ccn::NUM_STORE_QUEUES];
} ctx;

void initialize() {
    bsc::initialize();
    ccn::initialize();
    cpg::initialize();
    dmac::initialize();
    intc::initialize();
    prfc::initialize();
    rtc::initialize();
    scif::initialize();
    tmu::initialize();
    ubc::initialize();
}

void reset() {
    bsc::reset();
    ccn::reset();
    cpg::reset();
    dmac::reset();
    intc::reset();
    prfc::reset();
    rtc::reset();
    scif::reset();
    tmu::reset();
    ubc::reset();

    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {
    bsc::shutdown();
    ccn::shutdown();
    cpg::shutdown();
    dmac::shutdown();
    intc::shutdown();
    prfc::shutdown();
    rtc::shutdown();
    scif::shutdown();
    tmu::shutdown();
    ubc::shutdown();
}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped SH-4 P4 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u8 read(const u32 addr) {
    switch (addr) {
        case IO_WTCSR:
            std::puts("WTCSR read8");

            return cpg::get_watchdog_timer_control();
        case IO_TSTR:
            std::puts("TSTR read8");

            return tmu::get_timer_start();
        default:
            std::printf("Unmapped SH-4 P4 read8 @ %02X\n", addr);
            exit(1);
    }
}

template<>
u16 read(const u32 addr) {
    switch (addr) {
        case IO_PMCR0:
            std::puts("PMCR0 read16");

            return prfc::get_control(prfc::CHANNEL_0);
        case IO_RFCR:
            std::puts("RFCR read16");

            return bsc::get_refresh_count();
        case IO_PDTRA:
            std::puts("PDTRA read16");

            return bsc::get_port_data(bsc::PORT_A);
        case IO_IPRA:
            std::puts("IPRA read16");

            return intc::get_priority(intc::PRIORITY_A);
        case IO_IPRB:
            std::puts("IPRB read16");

            return intc::get_priority(intc::PRIORITY_B);
        case IO_IPRC:
            std::puts("IPRC read16");

            return intc::get_priority(intc::PRIORITY_C);
        case IO_TCR0:
            std::puts("TCR0 read16");

            return tmu::get_control(tmu::CHANNEL_0);
        case IO_TCR2:
            std::puts("TCR2 read16");

            return tmu::get_control(tmu::CHANNEL_2);
        case IO_SCFSR2:
            if constexpr (!SILENT_SCIF) std::puts("SCFSR2 read16");

            return scif::get_serial_status();
        case IO_SCLSR2:
            if constexpr (!SILENT_SCIF) std::puts("SCLSR2 read16");

            return scif::get_line_status();
        default:
            std::printf("Unmapped SH-4 P4 read16 @ %08X\n", addr);
            exit(1);
    }
}

constexpr u32 CPUVER = 0x040205C1;

template<>
u32 read(const u32 addr) {
    switch (addr) {
        case IO_CCR:
            std::puts("CCR read32");

            return ccn::get_cache_control();
        case IO_EXPEVT:
            std::puts("EXPEVT read32");

            return ccn::get_exception_event();
        case IO_INTEVT:
            std::puts("INTEVT read32");

            return ccn::get_interrupt_event();
        case IO_CPUVER:
            std::puts("CPUVER read32");

            return CPUVER;
        case IO_PCTRA:
            std::puts("PCTRA read32");

            return bsc::get_port_control(bsc::PORT_A);
        case IO_CHCR2:
            std::puts("CHCR2 read32");
            
            return dmac::get_control(dmac::CHANNEL_2);
        case IO_TCNT0:
            // std::puts("TCNT0 read32");

            return tmu::get_counter(tmu::CHANNEL_0);
        case IO_TCNT2:
            // std::puts("TCNT2 read32");

            return tmu::get_counter(tmu::CHANNEL_2);
        default:
            std::printf("Unmapped SH-4 P4 read32 @ %08X\n", addr);
            exit(1);
    }
}

template u64 read(u32);

template<typename T>
void write(const u32 addr, const T data) {
    std::printf("Unmapped SH-4 P4 write%zu @ %08X = %0*llX\n", 8 * sizeof(T), addr, (int)(2 * sizeof(T)), (u64)data);
    exit(1);
}

template<>
void write(const u32 addr, const u8 data) {
    if ((addr & ~0xFFFF) == IO_SDMR3) {
        // TODO: handle 32-bit bus SDMR3 writes?
        const u16 sdram_mode = (data & 0x1FF8) >> 3;

        std::printf("SDMR3 write = %03X\n", sdram_mode);

        bsc::set_sdram_mode_3(sdram_mode);
        return;
    }

    switch (addr) {
        case IO_BASRA:
            std::printf("BASRA write8 = %02X\n", data);

            ubc::set_asid(ubc::CHANNEL_A, data);
            break;
        case IO_BASRB:
            std::printf("BASRB write8 = %02X\n", data);

            ubc::set_asid(ubc::CHANNEL_B, data);
            break;
        case IO_BAMRA:
            std::printf("BAMRA write8 = %02X\n", data);

            ubc::set_address_mask(ubc::CHANNEL_A, data);
            break;
        case IO_BAMRB:
            std::printf("BAMRB write8 = %02X\n", data);

            ubc::set_address_mask(ubc::CHANNEL_B, data);
            break;
        case IO_STBCR:
            std::printf("STBCR write8 = %02X\n", data);

            cpg::set_standby_control(data);
            break;
        case IO_STBCR2:
            std::printf("STBCR2 write8 = %02X\n", data);

            cpg::set_standby_control_2(data);
            break;
        case IO_RMONAR:
            std::printf("RMONAR write8 = %02X\n", data);

            rtc::set_rtc_month_alarm(data);
            break;
        case IO_RCR1:
            std::printf("RCR1 write8 = %02X\n", data);

            rtc::set_rtc_control_1(data);
            break;
        case IO_TOCR:
            std::printf("TOCR write8 = %02X\n", data);
            
            tmu::set_timer_output_control(data);
            break;
        case IO_TSTR:
            std::printf("TSTR write8 = %02X\n", data);
            
            tmu::set_timer_start(data);
            break;
        case IO_SCBRR2:
            if constexpr (!SILENT_SCIF) std::printf("SCBRR2 write8 = %02X\n", data);

            scif::set_bit_rate(data);
            break;
        case IO_SCFTDR2:
            if constexpr (!SILENT_SCIF) std::printf("SCFTDR2 write8 = %02X\n", data);

            scif::set_transmit_fifo_data(data);
            break;
        default:
            std::printf("Unmapped SH-4 P4 write8 @ %08X = %02X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case IO_PMCR0:
            std::printf("PMCR0 write16 = %04X\n", data);

            prfc::set_control(prfc::CHANNEL_0, data);
            break;
        case IO_BBRA:
            std::printf("BBRA write16 = %04X\n", data);

            ubc::set_bus_cycle(ubc::CHANNEL_A, data);
            break;
        case IO_BBRB:
            std::printf("BBRB write16 = %04X\n", data);

            ubc::set_bus_cycle(ubc::CHANNEL_B, data);
            break;
        case IO_BRCR:
            std::printf("BRCR write16 = %04X\n", data);
            
            ubc::set_break_control(data);
            break;
        case IO_BCR2:
            std::printf("BCR2 write16 = %04X\n", data);
            
            bsc::set_bus_control_2(data);
            break;
        case IO_PCR:
            std::printf("PCR write16 = %04X\n", data);
            break;
        case IO_RTCSR:
            std::printf("RTCSR write16 = %04X\n", data);

            bsc::set_refresh_timer_control(data);
            break;
        case IO_RTCOR:
            std::printf("RTCOR write16 = %04X\n", data);

            bsc::set_refresh_time_constant(data);
            break;
        case IO_RFCR:
            std::printf("RFCR write16 = %04X\n", data);

            bsc::set_refresh_count(data);
            break;
        case IO_PDTRA:
            std::printf("PDTRA write16 = %04X\n", data);
            
            bsc::set_port_data(bsc::PORT_A, data);
            break;
        case IO_PDTRB:
            std::printf("PDTRB write16 = %04X\n", data);
            
            bsc::set_port_data(bsc::PORT_A, data);
            break;
        case IO_GPIOIC:
            std::printf("GPIOIC write16 = %04X\n", data);
            
            bsc::set_gpio_interrupt_control(data);
            break;
        case IO_WTCNT:
            std::printf("WTCNT write16 = %04X\n", data);

            cpg::set_watchdog_timer_counter(data);
            break;
        case IO_WTCSR:
            std::printf("WTCSR write16 = %04X\n", data);

            cpg::set_watchdog_timer_control(data);
            break;
        case IO_ICR:
            std::printf("ICR write16 = %04X\n", data);
            
            intc::set_interrupt_control(data);
            break;
        case IO_IPRA:
            std::printf("IPRA write16 = %04X\n", data);
            
            intc::set_priority(intc::PRIORITY_A, data);
            break;
        case IO_IPRB:
            std::printf("IPRB write16 = %04X\n", data);
            
            intc::set_priority(intc::PRIORITY_B, data);
            break;
        case IO_IPRC:
            std::printf("IPRC write16 = %04X\n", data);
            
            intc::set_priority(intc::PRIORITY_C, data);
            break;
        case IO_TCR0:
            std::printf("TCR0 write16 = %04X\n", data);
            
            tmu::set_control(tmu::CHANNEL_0, data);
            break;
        case IO_TCR1:
            std::printf("TCR1 write16 = %04X\n", data);
            
            tmu::set_control(tmu::CHANNEL_1, data);
            break;
        case IO_TCR2:
            std::printf("TCR2 write16 = %04X\n", data);
            
            tmu::set_control(tmu::CHANNEL_2, data);
            break;
        case IO_SCSMR2:
            if constexpr (!SILENT_SCIF) std::printf("SCSMR2 write16 = %04X\n", data);

            scif::set_serial_mode(data);
            break;
        case IO_SCSCR2:
            if constexpr (!SILENT_SCIF) std::printf("SCSCR2 write16 = %04X\n", data);

            scif::set_serial_control(data);
            break;
        case IO_SCFSR2:
            if constexpr (!SILENT_SCIF) std::printf("SCFSR2 write16 = %04X\n", data);

            scif::set_serial_status(data);
            break;
        case IO_SCFCR2:
            if constexpr (!SILENT_SCIF) std::printf("SCFCR2 write16 = %04X\n", data);

            scif::set_fifo_control(data);
            break;
        case IO_SCSPTR2:
            if constexpr (!SILENT_SCIF) std::printf("SCSPTR2 write16 = %04X\n", data);

            scif::set_serial_port(data);
            break;
        case IO_SCLSR2:
            if constexpr (!SILENT_SCIF) std::printf("SCLSR2 write16 = %04X\n", data);

            scif::set_line_status(data);
            break;
        default:
            std::printf("Unmapped SH-4 P4 write16 @ %08X = %04X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u32 data) {
    if (addr < SIZE_STORE_QUEUE_AREA) {
        const bool is_second_queue = (addr >> 5) != 0;

        const usize select_longword = (addr >> 2) & 7;

        ctx.store_queues[is_second_queue].bytes[select_longword] = data;

        std::printf("SQ%d[%zu] write32 = %08X\n", is_second_queue, select_longword, data);
        return;
    }

    if ((addr & 0xFF000000) == BASE_OPERAND_CACHE_TAG) {
        std::printf("SH-4 operand cache tag write32 @ %08X = %08X\n", addr, data);
        return;
    }

    switch (addr) {
        case IO_PTEH:
            std::printf("PTEH write32 = %08X\n", data);

            ccn::set_page_table_entry_hi(data);
            break;
        case IO_PTEL:
            std::printf("PTEL write32 = %08X\n", data);

            ccn::set_page_table_entry_lo(data);
            break;
        case IO_TTB:
            std::printf("TTB write32 = %08X\n", data);
            
            ccn::set_translation_table_base(data);
            break;
        case IO_TEA:
            std::printf("TEA write32 = %08X\n", data);
            
            ccn::set_tlb_exception_address(data);
            break;
        case IO_MMUCR:
            std::printf("MMUCR write32 = %08X\n", data);

            ccn::set_mmu_control(data);
            break;
        case IO_CCR:
            std::printf("CCR write32 = %08X\n", data);
            
            ccn::set_cache_control(data);
            break;
        case IO_TRAPA:
            std::printf("TRAPA write32 = %08X\n", data);
            
            ccn::set_trapa_exception(data);
            break;
        case IO_EXPEVT:
            std::printf("EXPEVT write32 = %08X\n", data);
            
            ccn::set_exception_event(data);
            break;
        case IO_INTEVT:
            std::printf("INTEVT write32 = %08X\n", data);
            
            ccn::set_interrupt_event(data);
            break;
        case IO_PTEA:
            std::printf("PTEA write32 = %08X\n", data);
            
            ccn::set_page_table_assistance(data);
            break;
        case IO_QACR1:
            std::printf("QACR1 write32 = %08X\n", data);
            
            ccn::set_queue_address_control(ccn::STORE_QUEUE_1, data);
            break;
        case IO_QACR2:
            std::printf("QACR2 write32 = %08X\n", data);
            
            ccn::set_queue_address_control(ccn::STORE_QUEUE_2, data);
            break;
        case IO_BARA:
            std::printf("BARA write32 = %08X\n", data);
            
            ubc::set_address(ubc::CHANNEL_A, data);
            break;
        case IO_BARB:
            std::printf("BARB write32 = %08X\n", data);
            
            ubc::set_address(ubc::CHANNEL_B, data);
            break;
        case IO_BCR1:
            std::printf("BCR1 write32 = %08X\n", data);
            
            bsc::set_bus_control_1(data);
            break;
        case IO_WCR1:
            std::printf("WCR1 write32 = %08X\n", data);
            
            bsc::set_wait_control_1(data);
            break;
        case IO_WCR2:
            std::printf("WCR2 write32 = %08X\n", data);
            
            bsc::set_wait_control_2(data);
            break;
        case IO_WCR3:
            std::printf("WCR3 write32 = %08X\n", data);
            
            bsc::set_wait_control_3(data);
            break;
        case IO_MCR:
            std::printf("MCR write32 = %08X\n", data);
            
            bsc::set_memory_control(data);
            break;
        case IO_PCTRA:
            std::printf("PCTRA write32 = %08X\n", data);
            
            bsc::set_port_control(bsc::PORT_A, data);
            break;
        case IO_PCTRB:
            std::printf("PCTRB write32 = %08X\n", data);
            
            bsc::set_port_control(bsc::PORT_B, data);
            break;
        case IO_SAR1:
            std::printf("SAR1 write32 = %08X\n", data);
            
            dmac::set_source_address(dmac::CHANNEL_1, data);
            break;
        case IO_DAR1:
            std::printf("DAR1 write32 = %08X\n", data);
            
            dmac::set_destination_address(dmac::CHANNEL_1, data);
            break;
        case IO_DMATCR1:
            std::printf("DMATCR1 write32 = %08X\n", data);
            
            dmac::set_transfer_count(dmac::CHANNEL_1, data);
            break;
        case IO_CHCR1:
            std::printf("CHCR1 write32 = %08X\n", data);
            
            dmac::set_control(dmac::CHANNEL_1, data);
            break;
        case IO_SAR2:
            std::printf("SAR2 write32 = %08X\n", data);
            
            dmac::set_source_address(dmac::CHANNEL_2, data);
            break;
        case IO_DAR2:
            std::printf("DAR2 write32 = %08X\n", data);
            
            dmac::set_destination_address(dmac::CHANNEL_2, data);
            break;
        case IO_DMATCR2:
            std::printf("DMATCR2 write32 = %08X\n", data);
            
            dmac::set_transfer_count(dmac::CHANNEL_2, data);
            break;
        case IO_CHCR2:
            std::printf("CHCR2 write32 = %08X\n", data);
            
            dmac::set_control(dmac::CHANNEL_2, data);
            break;
        case IO_SAR3:
            std::printf("SAR3 write32 = %08X\n", data);
            
            dmac::set_source_address(dmac::CHANNEL_3, data);
            break;
        case IO_DAR3:
            std::printf("DAR2 write32 = %08X\n", data);
            
            dmac::set_destination_address(dmac::CHANNEL_3, data);
            break;
        case IO_DMATCR3:
            std::printf("DMATCR3 write32 = %08X\n", data);
            
            dmac::set_transfer_count(dmac::CHANNEL_3, data);
            break;
        case IO_CHCR3:
            std::printf("CHCR3 write32 = %08X\n", data);
            
            dmac::set_control(dmac::CHANNEL_3, data);
            break;
        case IO_DMAOR:
            std::printf("DMAOR write32 = %08X\n", data);
            
            dmac::set_dma_operation(data);
            break;
        case IO_TCOR0:
            std::printf("TCOR0 write32 = %08X\n", data);
            
            tmu::set_constant(tmu::CHANNEL_0, data);
            break;
        case IO_TCNT0:
            std::printf("TCNT0 write32 = %08X\n", data);
            
            tmu::set_counter(tmu::CHANNEL_0, data);
            break;
        case IO_TCOR1:
            std::printf("TCOR1 write32 = %08X\n", data);
            
            tmu::set_constant(tmu::CHANNEL_1, data);
            break;
        case IO_TCNT1:
            std::printf("TCNT1 write32 = %08X\n", data);
            
            tmu::set_counter(tmu::CHANNEL_1, data);
            break;
        case IO_TCOR2:
            std::printf("TCOR2 write32 = %08X\n", data);
            
            tmu::set_constant(tmu::CHANNEL_2, data);
            break;
        case IO_TCNT2:
            std::printf("TCNT2 write32 = %08X\n", data);
            
            tmu::set_counter(tmu::CHANNEL_2, data);
            break;
        default:
            std::printf("Unmapped SH-4 P4 write32 @ %08X = %08X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u64 data) {
    if (addr < SIZE_STORE_QUEUE_AREA) {
        const bool is_second_queue = (addr >> 5) != 0;

        const usize select_longword = (addr >> 2) & 6;

        ctx.store_queues[is_second_queue].bytes[select_longword + 0] = data;
        ctx.store_queues[is_second_queue].bytes[select_longword + 1] = data >> 32;

        std::printf("SQ%d[%zu] write64 = %016llX\n", is_second_queue, select_longword, data);
        return;
    }

    switch (addr) {
        default:
            std::printf("Unmapped SH-4 P4 write64 @ %08X = %016llX\n", addr, data);
            exit(1);
    }
}

void flush_store_queue(const u32 addr) {
    assert(addr < SIZE_STORE_QUEUE_AREA);

    const bool is_second_queue = (addr >> 5) != 0;

    std::printf("Flushing SQ%d\n", is_second_queue);

    hw::holly::bus::block_write(
    (addr & 0x03FFFFE0) | (ccn::get_store_queue_area(is_second_queue) << 26),
    (u8*)ctx.store_queues[is_second_queue].bytes
    );
}

}
