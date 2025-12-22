/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <vector>

#include <common/types.hpp>

namespace common::cdi {

constexpr int NUM_TOC_ENTRIES = 99;

using Fad = u8[3];

struct SessionInfo {
    u8 start_track;
    Fad leadout_fad;
};

struct TocEntry {
    u8 adr_control;
    Fad fad;
} __attribute__((packed));

struct Toc {
    TocEntry track_entries[NUM_TOC_ENTRIES];

    TocEntry start_track_entry;
    TocEntry end_track_entry;
    TocEntry leadout_entry;
} __attribute__((packed));

void load(const char* path);

SessionInfo request_session(const u8 num_session);
Toc read_toc(const bool second_layer);

std::vector<u8> read_sectors(const u32 fad, const u32 num_sectors);

}
