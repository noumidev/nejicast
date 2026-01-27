/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <cassert>
#include <cstdio>
#include <vector>

#include <common/types.hpp>
#include <common/disc/disc.hpp>

namespace common::disc::cdi {

struct Track {
    u32 start;
    u32 pregap_length;
    u32 track_length;
    u32 first_lba;
    u32 total_length;
    u32 sector_size;
    u8 control;
};

struct Session {
    u32 first_track;

    std::vector<Track> tracks;
};

class Cdi : public Disc {
private:
    FILE* file;

    std::vector<Session> sessions;

    void seek_stream(const long offset, const int whence = SEEK_CUR);
    void read_stream(u8* bytes, const usize size, const long offset = 0, const int whence = SEEK_CUR);

    long read_track(Track& track, long pos, const u32 cdi_version);
    long read_session(Session& session, long pos, const u32 cdi_version);

public:
    Cdi();
    ~Cdi();

    u8 get_disc_format() override {
        return DISC_FORMAT_CDROM_XA;
    }

    bool load(const char* path) override;
    bool is_mounted() override;

    Toc read_toc(const bool is_hd_region) override;
    SessionInfo request_session(const u8 num_session) override;

    std::vector<u8> read_sectors(const u32 fad, const u32 num_sectors, const bool is_cdda = false) override;
};

}
