/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#pragma once

#include <cassert>
#include <cstdio>
#include <vector>

#include <common/types.hpp>
#include <common/disc/disc.hpp>

namespace common::disc::gdi {

struct Track {
    FILE* file;

    u32 track_length;
    u32 first_lba;
    u8 control;
    u32 sector_size;
};

class Gdi : public Disc {
private:
    std::vector<Track> tracks;

public:
    Gdi();
    ~Gdi();

    u8 get_disc_format() override {
        return DISC_FORMAT_GDROM;
    }

    bool load(const char* path) override;
    bool is_mounted() override;

    Toc read_toc(const bool is_hd_region) override;
    SessionInfo request_session(const u8 num_session) override;

    std::vector<u8> read_sectors(const u32 fad, const u32 num_sectors, const bool is_cdda = false) override;
};

}
