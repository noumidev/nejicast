/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <hw/cpu/ocio.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <scheduler.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/intc.hpp>

namespace hw::cpu::ocio {

enum : u32 {
    BASE_OPERAND_CACHE_TAG = 0x14000000,
};

enum : u32 {
    IO_PTEH    = 0x1F000000,
    IO_PTEL    = 0x1F000004,
    IO_TTB     = 0x1F000008,
    IO_TEA     = 0x1F00000C,
    IO_MMUCR   = 0x1F000010,
    IO_CCR     = 0x1F00001C,
    IO_TRAPA   = 0x1F000020,
    IO_EXPEVT  = 0x1F000024,
    IO_INTEVT  = 0x1F000028,
    IO_CPUVER  = 0x1F000030,
    IO_PTEA    = 0x1F000034,
    IO_QACR1   = 0x1F000038,
    IO_QACR2   = 0x1F00003C,
    IO_BBRA    = 0x1F200008,
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
    IO_SCFCR2  = 0x1FE80018,
    IO_SCSPTR2 = 0x1FE80020,
};

enum {
    DMA_0,
    DMA_1,
    DMA_2,
    DMA_3,
    NUM_DMA_CHANNELS,
};

enum {
    TIMER_0,
    TIMER_1,
    TIMER_2,
    NUM_TIMERS,
};

enum {
    SIZE_STORE_QUEUE_AREA = 0x4000000,
};

#define PTEH    ctx.page_table_entry_hi
#define TTB     ctx.translation_table_base
#define TEA     ctx.tlb_exception_address
#define MMUCR   ctx.mmu_control
#define CCR     ctx.cache_control
#define TRAPA   ctx.trapa_exception
#define EXPEVT  ctx.exception_event
#define INTEVT  ctx.interrupt_event
#define PTEA    ctx.page_table_entry_assistance
#define QACR1   ctx.queue_address_control[0]
#define QACR2   ctx.queue_address_control[1]
#define BBRA    ctx.break_bus_cycle_a
#define BBRB    ctx.break_bus_cycle_b
#define BRCR    ctx.break_control
#define BCR1    ctx.bus_control_1
#define BCR2    ctx.bus_control_2
#define WCR1    ctx.wait_control_1
#define WCR2    ctx.wait_control_2
#define WCR3    ctx.wait_control_3
#define MCR     ctx.memory_control
#define RTCSR   ctx.refresh_timer_control
#define RTCNT   ctx.refresh_timer
#define RTCOR   ctx.refresh_time_constant
#define RFCR    ctx.refresh_count
#define PCTRA   ctx.port_a.control
#define PDTRA   ctx.port_a.latched_data
#define PCTRB   ctx.port_b.control
#define PDTRB   ctx.port_b.latched_data
#define GPIOIC  ctx.gpio_interrupt_control
#define SDMR3   ctx.sdram_mode_3
#define SAR0    ctx.dma_channels[DMA_0].source_address
#define DAR0    ctx.dma_channels[DMA_0].destination_address
#define DMATCR0 ctx.dma_channels[DMA_0].transfer_count
#define CHCR0   ctx.dma_channels[DMA_0].control
#define SAR1    ctx.dma_channels[DMA_1].source_address
#define DAR1    ctx.dma_channels[DMA_1].destination_address
#define DMATCR1 ctx.dma_channels[DMA_1].transfer_count
#define CHCR1   ctx.dma_channels[DMA_1].control
#define SAR2    ctx.dma_channels[DMA_2].source_address
#define DAR2    ctx.dma_channels[DMA_2].destination_address
#define DMATCR2 ctx.dma_channels[DMA_2].transfer_count
#define CHCR2   ctx.dma_channels[DMA_2].control
#define SAR3    ctx.dma_channels[DMA_3].source_address
#define DAR3    ctx.dma_channels[DMA_3].destination_address
#define DMATCR3 ctx.dma_channels[DMA_3].transfer_count
#define CHCR3   ctx.dma_channels[DMA_3].control
#define DMAOR   ctx.dma_operation
#define STBCR   ctx.standby_control
#define WTCNT   ctx.watchdog_timer_counter
#define WTCSR   ctx.watchdog_timer_control
#define STBCR2  ctx.standby_control_2
#define RMONAR  ctx.rtc_month_alarm
#define RCR1    ctx.rtc_control_1
#define ICR     ctx.interrupt_control
#define IPRA    ctx.interrupt_priority_a
#define IPRB    ctx.interrupt_priority_b
#define IPRC    ctx.interrupt_priority_c
#define TOCR    ctx.timer_output_control
#define TSTR    ctx.timer_start
#define TCOR0   ctx.timers[TIMER_0].constant
#define TCNT0   ctx.timers[TIMER_0].counter
#define TCR0    ctx.timers[TIMER_0].control
#define TCOR1   ctx.timers[TIMER_1].constant
#define TCNT1   ctx.timers[TIMER_1].counter
#define TCR1    ctx.timers[TIMER_1].control
#define TCOR2   ctx.timers[TIMER_2].constant
#define TCNT2   ctx.timers[TIMER_2].counter
#define TCR2    ctx.timers[TIMER_2].control
#define SCSMR2  ctx.serial_mode_2
#define SCBRR2  ctx.bit_rate_2
#define SCSCR2  ctx.serial_control_2
#define SCFCR2  ctx.fifo_control_2
#define SCSPTR2 ctx.serial_port_2

constexpr usize NUM_STORE_QUEUES = 2;

struct {
    struct {
        u32 bytes[8];
    } store_queues[NUM_STORE_QUEUES];

    union {
        u32 raw;

        struct {
            u32 address_space_id    :  8;
            u32                     :  2;
            u32 virtual_page_number : 22;
        };
    } page_table_entry_hi;
    
    u32 translation_table_base, tlb_exception_address;

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

    u32 trapa_exception, exception_event, interrupt_event;

    union {
        u32 raw;

        struct {
            u32 space_attribute :  3;
            u32 timing_control  :  1;
            u32                 : 28;
        };
    } page_table_entry_assistance;

    union {
        u32 raw;
        
        struct {
            u32      :  2;
            u32 area :  3;
            u32      : 27;
        };
    } queue_address_control[NUM_STORE_QUEUES];

    union {
        u16 raw;

        struct {
            u16 select_size_01    : 2;
            u16 select_read_write : 2;
            u16 select_access     : 2;
            u16 select_size_2     : 1;
            u16                   : 9;
        };
    } break_bus_cycle_a, break_bus_cycle_b;

    union {
        u16 raw;

        struct {
            u16 enable_user_break_debug   : 1;
            u16                           : 2;
            u16 select_sequence_condition : 1;
            u16                           : 2;
            u16 select_pc_break_b         : 1;
            u16 enable_data_break_b       : 1;
            u16                           : 2;
            u16 select_pc_break_a         : 1;
            u16                           : 2;
            u16 condition_match_flag_b    : 1;
            u16 condition_match_flag_a    : 1;
        };
    } break_control;

    union {
        u32 raw;

        struct {
            u32 a56_pcmcia       : 1;
            u32                  : 1;
            u32 a23_memory_type  : 3;
            u32 enable_a6_burst  : 3;
            u32 enable_a5_burst  : 3;
            u32 a0_burst_control : 3;
            u32 high_z_control   : 2;
            u32 dma_burst        : 1;
            u32 enable_mpx       : 1;
            u32 partial_sharing  : 1;
            u32 enable_breq      : 1;
            u32 a4_byte_control  : 1;
            u32 a1_byte_control  : 1;
            u32                  : 2;
            u32 opup_control     : 1;
            u32 ipup_control     : 1;
            u32 dpup_control     : 1;
            u32                  : 2;
            u32 a0_mpx           : 1;
            u32 slave_mode       : 1;
            u32 little_endian    : 1;
        };
    } bus_control_1;

    union {
        u16 raw;

        struct {
            u16 enable_port  : 1;
            u16              : 1;
            u16 a1_bus_width : 2;
            u16 a2_bus_width : 2;
            u16 a3_bus_width : 2;
            u16 a4_bus_width : 2;
            u16 a5_bus_width : 2;
            u16 a6_bus_width : 2;
            u16 a0_bus_width : 2;
        };
    } bus_control_2;

    union {
        u32 raw;

        struct {
            u32 a0_idle_cycles  : 3;
            u32                 : 1;
            u32 a1_idle_cycles  : 3;
            u32                 : 1;
            u32 a2_idle_cycles  : 3;
            u32                 : 1;
            u32 a3_idle_cycles  : 3;
            u32                 : 1;
            u32 a4_idle_cycles  : 3;
            u32                 : 1;
            u32 a5_idle_cycles  : 3;
            u32                 : 1;
            u32 a6_idle_cycles  : 3;
            u32                 : 1;
            u32 dma_idle_cycles : 3;
            u32                 : 1;
        };
    } wait_control_1;

    union {
        u32 raw;

        struct {
            u32 a0_burst_pitch : 3;
            u32 a0_wait_states : 3;
            u32 a1_wait_states : 3;
            u32 a2_wait_states : 3;
            u32                : 1;
            u32 a3_wait_states : 3;
            u32                : 1;
            u32 a4_wait_states : 3;
            u32 a5_burst_pitch : 3;
            u32 a5_wait_states : 3;
            u32 a6_burst_pitch : 3;
            u32 a6_wait_states : 3;
        };
    } wait_control_2;

    union {
        u32 raw;

        struct {
            u32 a0_hold_time     : 2;
            u32 a0_strobe_time   : 1;
            u32                  : 1;
            u32 a1_hold_time     : 2;
            u32 a1_strobe_time   : 1;
            u32 a1_negate_timing : 1;
            u32 a2_hold_time     : 2;
            u32 a2_strobe_time   : 1;
            u32                  : 1;
            u32 a3_hold_time     : 2;
            u32 a3_strobe_time   : 1;
            u32                  : 1;
            u32 a4_hold_time     : 2;
            u32 a4_strobe_time   : 1;
            u32 a4_negate_timing : 1;
            u32 a5_hold_time     : 2;
            u32 a5_strobe_time   : 1;
            u32                  : 1;
            u32 a6_hold_time     : 2;
            u32 a6_strobe_time   : 1;
            u32                  : 5;
        };
    } wait_control_3;

    union {
        u32 raw;

        struct {
            u32 edo_mode              : 1;
            u32 refresh_mode          : 1;
            u32 refresh_control       : 1;
            u32 address_multiplexing  : 4;
            u32 dram_width            : 2;
            u32 enable_burst          : 1;
            u32 refresh_period        : 3;
            u32 write_precharge_delay : 3;
            u32 ras_cas_delay         : 2;
            u32                       : 1;
            u32 ras_precharge_period  : 3;
            u32                       : 1;
            u32 cas_negation_period   : 1;
            u32                       : 3;
            u32 ras_precharge_time    : 3;
            u32 mode_set              : 1;
            u32 ras_down              : 1;
        };
    } memory_control;

    union {
        u16 raw;

        struct {
            u16 select_limit              : 1;
            u16 enable_overflow_interrupt : 1;
            u16 overflow_flag             : 1;
            u16 select_clock              : 3;
            u16 enable_match_interrupt    : 1;
            u16 match_flag                : 1;
            u16                           : 8;
        };
    } refresh_timer_control;

    u16 refresh_timer, refresh_time_constant, refresh_count;

    struct {
        u32 control;
        u16 latched_data;
    } port_a, port_b;

    u16 gpio_interrupt_control;

    u16 sdram_mode_3;

    struct {
        u32 source_address;
        u32 destination_address;
        u32 transfer_count;

        union {
            u32 raw;

            struct {
                u32 enable_dmac              : 1;
                u32 transfer_end             : 1;
                u32 enable_interrupt         : 1;
                u32                          : 1;
                u32 transmit_size            : 3;
                u32 burst_mode               : 1;
                u32 select_resource          : 4;
                u32 source_mode              : 2;
                u32 destination_mode         : 2;
                u32 acknowledge_level        : 1;
                u32 acknowledge_mode         : 1;
                u32 request_check_level      : 1;
                u32 select_dreq              : 1;
                u32                          : 4;
                u32 destination_wait_control : 1;
                u32 destination_attribute    : 3;
                u32 source_wait_control      : 1;
                u32 source_attribute         : 3;
            };
        } control;
    } dma_channels[NUM_DMA_CHANNELS];

    union {
        u32 raw;

        struct {
            u32 master_enable_dmac :  1;
            u32 nmi_flag           :  1;
            u32 address_error_flag :  1;
            u32                    :  5;
            u32 priority_mode      :  2;
            u32                    :  6;
            u32 on_demand_mode     :  1;
            u32                    : 16;
        };
    } dma_operation;

    union {
        u8 raw;

        struct {
            u8 stop_sci_clock  : 1;
            u8 stop_rtc_clock  : 1;
            u8 stop_tmu_clock  : 1;
            u8 stop_scif_clock : 1;
            u8 stop_dmac_clock : 1;
            u8 ppu_pup_control : 1;
            u8 ppu_hiz_control : 1;
            u8 standby_mode    : 1;
        };
    } standby_control;

    u8 watchdog_timer_counter;

    union {
        u8 raw;

        struct {
            u8 select_clock           : 3;
            u8 interval_overflow_flag : 1;
            u8 watchdog_overflow_flag : 1;
            u8 select_reset           : 1;
            u8 select_mode            : 1;
            u8 enable_timer           : 1;
        };
    } watchdog_timer_control;

    union {
        u8 raw;

        struct {
            u8                 : 7;
            u8 deep_sleep_mode : 1;
        };
    } standby_control_2;

    union {
        u8 raw;

        struct {
            u8 ones   : 4;
            u8 tens   : 1;
            u8        : 2;
            u8 enable : 1;
        };
    } rtc_month_alarm;

    union {
        u8 raw;

        struct {
            u8 alarm_flag             : 1;
            u8                        : 2;
            u8 alarm_interrupt_enable : 1;
            u8 carry_interrupt_enable : 1;
            u8                        : 2;
            u8 carry_flag             : 1;
        };
    } rtc_control_1;

    union {
        u16 raw;

        struct {
            u16                    : 7;
            u16 pin_mode           : 1;
            u16 select_nmi_edge    : 1;
            u16 nmi_block_mode     : 1;
            u16                    : 4;
            u16 nmi_interrupt_mask : 1;
            u16 nmi_input_level    : 1;
        };
    } interrupt_control;

    u16 interrupt_priority_a, interrupt_priority_b, interrupt_priority_c;

    union {
        u8 raw;

        struct {
            u8 timer_clock_control : 1;
            u8                     : 7;
        };
    } timer_output_control;

    union {
        u8 raw;

        struct {
            u8 start_counter : 3;
            u8               : 5;
        };
    } timer_start;

    struct {
        u32 constant;
        u32 counter;

        union {
            u16 raw;

            struct {
                u16 prescaler                  : 3;
                u16 clock_edge                 : 2;
                u16 enable_underflow_interrupt : 1;
                u16 enable_input_capture       : 2;
                u16 underflow_flag             : 1;
                u16 input_capture_flag         : 1;
                u16                            : 6;
            };
        } control;
    } timers[NUM_TIMERS];

    union {
        u16 raw;

        struct {
            u16 select_clock     : 2;
            u16                  : 1;
            u16 stop_length      : 1;
            u16 parity_mode      : 1;
            u16 enable_parity    : 1;
            u16 character_length : 1;
            u16                  : 9;
        };
    } serial_mode_2;

    u8 bit_rate_2;

    union {
        u16 raw;

        struct {
            u16                                : 1;
            u16 enable_clock_1                 : 1;
            u16                                : 1;
            u16 enable_receive_error_interrupt : 1;
            u16 enable_receive                 : 1;
            u16 enable_transmit                : 1;
            u16 enable_receive_interrupt       : 1;
            u16 enable_transmit_interrupt      : 1;
            u16                                : 8;
        };
    } serial_control_2;

    union {
        u16 raw;

        struct {
            u16 test_loopback              : 1;
            u16 reset_receive_fifo         : 1;
            u16 reset_transmit_fifo        : 1;
            u16 enable_modem_control       : 1;
            u16 transmit_fifo_data_trigger : 2;
            u16 receive_fifo_data_trigger  : 2;
            u16                            : 8;
        };
    } fifo_control_2;

    union {
        u16 raw;

        struct {
            u16 port_break_data : 1;
            u16 port_break_io   : 1;
            u16                 : 2;
            u16 cts_port_data   : 1;
            u16 cts_port_io     : 1;
            u16 rts_port_data   : 1;
            u16 rts_port_io     : 1;
            u16                 : 8;
        };
    } serial_port_2;
} ctx;

// Simulates DRAM refresh
static void refresh_dram() {
    constexpr u16 COUNT_LIMIT[2] = {1024, 512};

    RTCNT++;

    if (RTCNT >= RTCOR) {
        RTCNT = 0;

        RTCSR.match_flag = 1;

        if (RTCSR.enable_match_interrupt) {
            std::puts("Unimplemented SH-4 refresh timer match interrupt");
            exit(1);
        }

        RFCR++;

        if (RFCR >= COUNT_LIMIT[RTCSR.select_limit]) {
            RFCR = 0;

            RTCSR.overflow_flag = 1;

            if (RTCSR.enable_overflow_interrupt) {
                std::puts("Unimplemented SH-4 refresh count overflow interrupt");
                exit(1);
            }
        }
    }
}

constexpr int NUM_PINS = 16;

enum {
    PIN_0             = 0,
    PIN_1             = 1,
    PIN_VIDEO_MODE_LO = 8,
    PIN_VIDEO_MODE_HI = 9,
};

enum {
    VIDEO_MODE_VGA,
};

static u16 read_port_a() {
    u16 port_data = 0;

    for (int i = 0; i < NUM_PINS; i++) {
        const bool is_output = ((PCTRA >> (2 * i + 0)) & 1) != 0;
        const bool is_pull_up = ((PCTRA >> (2 * i + 1)) & 1) == 0;

        if (is_output) {
            // Return latched value
            port_data |= PDTRA & (1 << i);
        } else {
            switch (i) {
                case PIN_0:
                case PIN_1:
                    port_data |= 1 << i;
                    break;
                case PIN_VIDEO_MODE_LO:
                    port_data |= (VIDEO_MODE_VGA & 1) << 8;
                    break;
                case PIN_VIDEO_MODE_HI:
                    port_data |= (VIDEO_MODE_VGA & 2) << 8;
                    break;
                default:
                    if (is_pull_up) {
                        port_data |= 1 << i;
                    }
                    break;
            }
        }
        
        std::printf("Pin %d read (output: %d, pull-up: %d)\n", i, is_output, is_pull_up);
    }

    // On Dreamcast, these two pins are shorted
    if ((port_data & 3) != 3) {
        port_data &= ~3;
    }

    std::printf("Port A data = %04X\n", port_data);

    return port_data;
}

static void write_port_a(const u16 data) {
    for (int i = 0; i < NUM_PINS; i++) {
        const bool is_output = ((PCTRA >> (2 * i)) & 1) != 0;

        if (is_output) {
            const u16 bit = (data >> i) & 1;

            std::printf("Pin %d write = %u\n", i, bit);
        }
    }

    // Update latched value
    PDTRA = data;
}

// TODO: merge this with Port A write handler?
static void write_port_b(const u16 data) {
    for (int i = 0; i < NUM_PINS; i++) {
        const bool is_output = ((PCTRB >> (2 * i)) & 1) != 0;

        if (is_output) {
            const u16 bit = (data >> i) & 1;

            std::printf("Pin %d write = %u\n", i, bit);
        }
    }

    // Update latched value
    PDTRB = data;
}

void initialize() {
    BCR2.raw = 0x3FFC;
    WCR1.raw = 0x77777777;
    WCR2.raw = 0xFFFEEFFF;
    WCR3.raw = 0x07777777;

    for (auto& timer : ctx.timers) {
        timer.constant = 0xFFFFFFFF;
        timer.counter = 0xFFFFFFFF;
    }

    SCBRR2 = 0xFF;
}

void reset() {
    std::memset(&ctx, 0, sizeof(ctx));
}

void shutdown() {}

template<typename T>
T read(const u32 addr) {
    std::printf("Unmapped SH-4 P4 read%zu @ %08X\n", 8 * sizeof(T), addr);
    exit(1);
}

template<>
u8 read(const u32 addr) {
    switch (addr) {
        case IO_TSTR:
            std::puts("TSTR read8");

            return TSTR.raw;
        default:
            std::printf("Unmapped SH-4 P4 read8 @ %02X\n", addr);
            exit(1);
    }
}

template<>
u16 read(const u32 addr) {
    switch (addr) {
        case IO_RFCR:
            std::puts("RFCR read16");

            // HACK
            refresh_dram();

            return RFCR;
        case IO_PDTRA:
            std::puts("PDTRA read16");

            return read_port_a();
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

            return CCR.raw;
        case IO_EXPEVT:
            std::puts("EXPEVT read32");

            return EXPEVT;
        case IO_INTEVT:
            std::puts("INTEVT read32");

            return INTEVT;
        case IO_CPUVER:
            std::puts("CPUVER read32");

            return CPUVER;
        case IO_PCTRA:
            std::puts("PCTRA read32");

            return PCTRA;
        case IO_CHCR2:
            std::puts("CHCR2 read32");

            return CHCR2.raw;
        case IO_TCNT0:
            // std::puts("TCNT0 read32");

            // HACK
            return TCNT0--;
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
        // TODO: handle 32-bis bus SDMR3 writes?
        SDMR3 = (data & 0x1FF8) >> 3;

        std::printf("SDMR3 write = %03X\n", SDMR3);
        return;
    }

    switch (addr) {
        case IO_STBCR:
            std::printf("STBCR write8 = %02X\n", data);

            STBCR.raw = data;
            break;
        case IO_RMONAR:
            std::printf("RMONAR write8 = %02X\n", data);

            RMONAR.raw = data;
            break;
        case IO_RCR1:
            std::printf("RCR1 write8 = %02X\n", data);

            RCR1.raw = data;
            break;
        case IO_STBCR2:
            std::printf("STBCR2 write8 = %02X\n", data);

            STBCR2.raw = data;
            break;
        case IO_TOCR:
            std::printf("TOCR write8 = %02X\n", data);

            TOCR.raw = data;
            break;
        case IO_TSTR:
            std::printf("TSTR write8 = %02X\n", data);

            TSTR.raw = data;
            break;
        case IO_SCBRR2:
            std::printf("SCBRR2 write8 = %02X\n", data);

            SCBRR2 = data;
            break;
        default:
            std::printf("Unmapped SH-4 P4 write8 @ %08X = %02X\n", addr, data);
            exit(1);
    }
}

template<>
void write(const u32 addr, const u16 data) {
    switch (addr) {
        case IO_BBRA:
            std::printf("BBRA write16 = %04X\n", data);

            BBRA.raw = data;
            break;
        case IO_BBRB:
            std::printf("BBRB write16 = %04X\n", data);

            BBRB.raw = data;
            break;
        case IO_BRCR:
            std::printf("BRCR write16 = %04X\n", data);

            BRCR.raw = data;
            break;
        case IO_BCR2:
            std::printf("BCR2 write16 = %04X\n", data);

            BCR2.raw = data;
            break;
        case IO_PCR:
            std::printf("PCR write16 = %04X\n", data);
            break;
        case IO_RTCSR:
            std::printf("RTCSR write16 = %04X\n", data);

            if ((data & 0xFF00) == 0xA500) {
                RTCSR.raw = data & 0xFF;
            }
            break;
        case IO_RTCOR:
            std::printf("RTCOR write16 = %04X\n", data);

            if ((data & 0xFF00) == 0xA500) {
                RTCOR = data & 0xFF;
            }
            break;
        case IO_RFCR:
            std::printf("RFCR write16 = %04X\n", data);

            if ((data & 0xFC00) == 0xA400) {
                RFCR = data & 0x3FF;
            }
            break;
        case IO_PDTRA:
            std::printf("PDTRA write16 = %04X\n", data);

            write_port_a(data);
            break;
        case IO_PDTRB:
            std::printf("PDTRB write16 = %04X\n", data);

            write_port_b(data);
            break;
        case IO_GPIOIC:
            std::printf("GPIOIC write16 = %04X\n", data);

            GPIOIC = data;
            break;
        case IO_WTCNT:
            std::printf("WTCNT write16 = %04X\n", data);

            if ((data & 0xFF00) == 0x5A00) {
                WTCNT = (u8)data;
            }
            break;
        case IO_WTCSR:
            std::printf("WTCSR write16 = %04X\n", data);

            if ((data & 0xFF00) == 0x5A00) {
                WTCSR.raw = (u8)data;
            }
            break;
        case IO_ICR:
            std::printf("ICR write16 = %04X\n", data);

            ICR.raw = data;
            break;
        case IO_IPRA:
            std::printf("IPRA write16 = %04X\n", data);

            IPRA = data;
            break;
        case IO_IPRB:
            std::printf("IPRB write16 = %04X\n", data);

            IPRB = data;
            break;
        case IO_IPRC:
            std::printf("IPRC write16 = %04X\n", data);

            IPRC = data;
            break;
        case IO_TCR0:
            std::printf("TCR0 write16 = %04X\n", data);

            TCR0.raw = data;
            break;
        case IO_TCR1:
            std::printf("TCR1 write16 = %04X\n", data);

            TCR1.raw = data;
            break;
        case IO_TCR2:
            std::printf("TCR2 write16 = %04X\n", data);

            TCR2.raw = data;
            break;
        case IO_SCSMR2:
            std::printf("SCSMR2 write16 = %04X\n", data);

            SCSMR2.raw = data;
            break;
        case IO_SCSCR2:
            std::printf("SCSCR2 write16 = %04X\n", data);

            SCSCR2.raw = data;
            break;
        case IO_SCFCR2:
            std::printf("SCFCR2 write16 = %04X\n", data);

            SCFCR2.raw = data;
            break;
        case IO_SCSPTR2:
            std::printf("SCSPTR2 write16 = %04X\n", data);

            SCSPTR2.raw = data;
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

            PTEH.raw = data;
            break;
        case IO_PTEL:
            std::printf("PTEL write32 = %08X\n", data);

            // TODO
            break;
        case IO_TTB:
            std::printf("TTB write32 = %08X\n", data);

            TTB = data;
            break;
        case IO_TEA:
            std::printf("TEA write32 = %08X\n", data);

            TEA = data;
            break;
        case IO_MMUCR:
            std::printf("MMUCR write32 = %08X\n", data);

            MMUCR.raw = data;

            assert(!MMUCR.enable_translation);
            break;
        case IO_CCR:
            std::printf("CCR write32 = %08X\n", data);

            CCR.raw = data;

            if (CCR.invalidate_operand_cache) {
                std::puts("SH-4 invalidate operand cache");

                CCR.invalidate_operand_cache = 0;
            }

            if (CCR.invalidate_instruction_cache) {
                std::puts("SH-4 invalidate instruction cache");

                CCR.invalidate_instruction_cache = 0;
            }
            break;
        case IO_TRAPA:
            std::printf("TRAPA write32 = %08X\n", data);

            TRAPA = data;
            break;
        case IO_EXPEVT:
            std::printf("EXPEVT write32 = %08X\n", data);

            EXPEVT = data;
            break;
        case IO_INTEVT:
            std::printf("INTEVT write32 = %08X\n", data);

            INTEVT = data;
            break;
        case IO_PTEA:
            std::printf("PTEA write32 = %08X\n", data);

            PTEA.raw = data;
            break;
        case IO_QACR1:
            std::printf("QACR1 write32 = %08X\n", data);

            QACR1.raw = data;
            break;
        case IO_QACR2:
            std::printf("QACR2 write32 = %08X\n", data);

            QACR2.raw = data;
            break;
        case IO_BCR1:
            std::printf("BCR1 write32 = %08X\n", data);

            BCR1.raw = data;
            break;
        case IO_WCR1:
            std::printf("WCR1 write32 = %08X\n", data);

            WCR1.raw = data;
            break;
        case IO_WCR2:
            std::printf("WCR2 write32 = %08X\n", data);

            WCR2.raw = data;
            break;
        case IO_WCR3:
            std::printf("WCR3 write32 = %08X\n", data);

            WCR3.raw = data;
            break;
        case IO_MCR:
            std::printf("MCR write32 = %08X\n", data);

            MCR.raw = data;
            break;
        case IO_PCTRA:
            std::printf("PCTRA write32 = %08X\n", data);

            PCTRA = data;
            break;
        case IO_PCTRB:
            std::printf("PCTRB write32 = %08X\n", data);

            PCTRB = data;
            break;
        case IO_SAR1:
            std::printf("SAR1 write32 = %08X\n", data);

            SAR1 = data;
            break;
        case IO_DAR1:
            std::printf("DAR1 write32 = %08X\n", data);

            DAR1 = data;
            break;
        case IO_DMATCR1:
            std::printf("DMATCR1 write32 = %08X\n", data);

            DMATCR1 = data;
            break;
        case IO_CHCR1:
            std::printf("CHCR1 write32 = %08X\n", data);

            CHCR1.raw = data;
            break;
        case IO_SAR2:
            std::printf("SAR2 write32 = %08X\n", data);

            SAR2 = data;
            break;
        case IO_DAR2:
            std::printf("DAR2 write32 = %08X\n", data);

            DAR2 = data;
            break;
        case IO_DMATCR2:
            std::printf("DMATCR2 write32 = %08X\n", data);

            DMATCR2 = data;
            break;
        case IO_CHCR2:
            std::printf("CHCR2 write32 = %08X\n", data);

            CHCR2.raw = data;
            break;
        case IO_SAR3:
            std::printf("SAR3 write32 = %08X\n", data);

            SAR3 = data;
            break;
        case IO_DAR3:
            std::printf("DAR2 write32 = %08X\n", data);

            DAR3 = data;
            break;
        case IO_DMATCR3:
            std::printf("DMATCR3 write32 = %08X\n", data);

            DMATCR3 = data;
            break;
        case IO_CHCR3:
            std::printf("CHCR3 write32 = %08X\n", data);

            CHCR3.raw = data;
            break;
        case IO_DMAOR:
            std::printf("DMAOR write32 = %08X\n", data);

            DMAOR.raw = data;
            break;
        case IO_TCOR0:
            std::printf("TCOR0 write32 = %08X\n", data);

            TCOR0 = data;
            break;
        case IO_TCNT0:
            std::printf("TCNT0 write32 = %08X\n", data);

            TCNT0 = data;
            break;
        case IO_TCOR1:
            std::printf("TCOR1 write32 = %08X\n", data);

            TCOR1 = data;
            break;
        case IO_TCNT1:
            std::printf("TCNT1 write32 = %08X\n", data);

            TCNT1 = data;
            break;
        case IO_TCOR2:
            std::printf("TCOR2 write32 = %08X\n", data);

            TCOR2 = data;
            break;
        case IO_TCNT2:
            std::printf("TCNT2 write32 = %08X\n", data);

            TCNT2 = data;
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

void set_exception_event(const u32 event) {
    EXPEVT = event;
}

void set_interrupt_event(const u32 event) {
    INTEVT = event;
}

constexpr int CHANNEL_2_INTERRUPT = 19;

void flush_store_queue(const u32 addr) {
    assert(addr < SIZE_STORE_QUEUE_AREA);

    const bool is_second_queue = (addr >> 5) != 0;

    std::printf("Flushing SQ%d\n", is_second_queue);

    hw::holly::bus::block_write(
    (addr & 0x03FFFFE0) |( ctx.queue_address_control[is_second_queue].area << 26),
    (u8*)ctx.store_queues[is_second_queue].bytes
    );
}

void execute_channel_2_dma(u32 &start_address, u32 &length, bool &start) {
    assert(CHCR2.enable_dmac);
    assert(CHCR2.transmit_size == 4); // 32-byte

    assert((start_address % 32) == 0);
    assert((length % 32) == 0);

    if (CHCR2.enable_interrupt) {
        scheduler::schedule_event(
            "CH2_IRQ",
            hw::holly::intc::assert_normal_interrupt,
            CHANNEL_2_INTERRUPT,
            8 * length
        );
    }

    // TODO: mask this through CPU
    SAR2 &= 0x1FFFFFFF;
    
    u8 dma_bytes[32];

    while (length > 0) {
        hw::holly::bus::block_read(SAR2, dma_bytes);
        hw::holly::bus::block_write(start_address, dma_bytes);

        switch (CHCR2.source_mode) {
            case 0: // Fixed
            case 3: // ??
                break;
            case 1:
                SAR2 += sizeof(dma_bytes);
                break;
            case 2:
                SAR2 -= sizeof(dma_bytes);
                break;
        }

        switch (CHCR2.destination_mode) {
            case 0: // Fixed
            case 3: // ??
                break;
            case 1:
                start_address += sizeof(dma_bytes);
                break;
            case 2:
                start_address -= sizeof(dma_bytes);
                break;
        }

        length -= sizeof(dma_bytes);
    }

    start = false;
}

}
