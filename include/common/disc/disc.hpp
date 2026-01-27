/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <vector>

#include <common/types.hpp>

namespace common::disc {

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

enum {
    DISC_FORMAT_CDROM_XA =  2,
    DISC_FORMAT_GDROM    =  8,
    DISC_FORMAT_NONE     = 15,
};

class Disc {
private:
public:
    Disc() {}
    virtual ~Disc() {}

    virtual u8 get_disc_format() = 0;

    virtual bool load(const char* path) = 0;
    virtual bool is_mounted() = 0;

    virtual Toc read_toc(const bool is_hd_region) = 0;
    // CDI only
    virtual SessionInfo request_session(const u8 num_session) = 0;

    virtual std::vector<u8> read_sectors(const u32 fad, const u32 num_sectors, const bool is_cdda = false) = 0;
};

Disc* mount_disc(const char* path);

}
