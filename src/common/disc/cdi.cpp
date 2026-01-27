/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

// Thanks to washingtonDC for a lot of this code (https://github.com/washingtondc-emu/washingtondc/blob/master/src/libwashdc/cdi.c)

#include <common/disc/cdi.hpp>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace common::disc::cdi {

Cdi::Cdi() {}

Cdi::~Cdi() {}

void Cdi::seek_stream(const long offset, const int whence) {
    assert(this->file != nullptr);

    std::fseek(this->file, offset, whence);
}

void Cdi::read_stream(u8* bytes, const usize size, const long offset, const int whence) {
    assert(this->file != nullptr);

    seek_stream(offset, whence);

    const usize read_size = std::fread(bytes, sizeof(u8), size, this->file);

    assert(read_size == size);
}

long Cdi::read_track(Track& track, long pos, const u32 cdi_version) {
    constexpr u8 TRACK_START[] = {
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF
    };

    u8 track_start[14];

    read_stream(track_start, sizeof(track_start));

    assert(std::memcmp(track_start, TRACK_START, sizeof(track_start)) == 0);

    track.start = pos;

    u8 path_length;

    read_stream(&path_length, sizeof(path_length), 4);
    read_stream(
        (u8*)&track.pregap_length,
        sizeof(track.pregap_length),
        path_length + ((cdi_version == 0x80000006) ? 33 : 25)
    );
    read_stream((u8*)&track.track_length, sizeof(track.track_length));
    read_stream((u8*)&track.first_lba, sizeof(track.first_lba), 22);
    read_stream((u8*)&track.total_length, sizeof(track.total_length));

    u8 select_sector_size;

    read_stream((u8*)&select_sector_size, sizeof(select_sector_size), 16);

    assert((select_sector_size == 1) || (select_sector_size == 2));

    track.sector_size = (select_sector_size == 1) ? 2336 : 2352;

    read_stream(&track.control, sizeof(track.control), 3);

    std::printf(
        "CDI Track (start = %08X, pregap = %u, length = %u, first LBA = %u, num sectors = %u, sector size = %u, control = %u)\n",
        track.start,
        track.pregap_length,
        track.track_length,
        track.first_lba,
        track.total_length,
        track.sector_size,
        track.control
    );

    seek_stream((cdi_version == 0x80000004) ? 35 : 125);

    return pos + track.total_length * track.sector_size;
}

long Cdi::read_session(Session& session, long pos, const u32 cdi_version) {
    u16 num_tracks;

    read_stream((u8*)&num_tracks, sizeof(num_tracks));

    session.tracks.resize(num_tracks);

    std::printf("CDI Session with %u tracks\n", num_tracks);
    
    // Skip session header
    seek_stream((cdi_version == 0x80000005) ? 18 : 10);

    for (Track& track : session.tracks) {
        pos = read_track(track, pos, cdi_version);
    }

    return pos;
}

bool Cdi::load(const char* path) {
    this->file = std::fopen(path, "rb");

    if (!is_mounted()) {
        return false;
    }

    u32 version;
    int header_offset;

    read_stream((u8*)&version, sizeof(version), -8, SEEK_END);
    read_stream((u8*)&header_offset, sizeof(header_offset));

    if ((version != 0x80000004) && (version != 0x80000005) && (version != 0x80000006)) {
        return false;
    }

    std::printf("CDI version = %08X, header offset = %08X\n", version, header_offset);

    // Seek to header
    switch (version) {
        case 0x80000004:
        case 0x80000005:
            seek_stream(header_offset, SEEK_SET);
            break;
        case 0x80000006:
            // Header is at the end of the CDI file
            seek_stream(-header_offset, SEEK_END);
            break;
    }

    u16 num_sessions;

    read_stream((u8*)&num_sessions, sizeof(num_sessions));

    std::printf("CDI Number of sessions = %u\n", num_sessions);

    this->sessions.resize(num_sessions);

    u32 num_tracks = 0;

    long pos = 0;

    for (u16 i = 0; i < num_sessions; i++) {
        Session& session = this->sessions[i];

        if ((i != 0) && (version == 0x80000004)) {
            seek_stream(2);
        }

        pos = read_session(session, pos, version);

        seek_stream(3);

        session.first_track = num_tracks;

        num_tracks += session.tracks.size();
    }

    return true;
}

bool Cdi::is_mounted() {
    return this->file != nullptr;
}

enum {
    SUB_Q_NONE,
    SUB_Q_POSITION,
};

inline u32 fad_to_lba(const u32 fad) {
    constexpr u32 PREGAP = 150;

    return fad - PREGAP;
}

inline u32 lba_to_fad(const u32 lba) {
    constexpr u32 PREGAP = 150;

    return lba + PREGAP;
}

SessionInfo Cdi::request_session(const u8 num_session) {
    assert(is_mounted());
    assert(num_session <= this->sessions.size());

    SessionInfo session_info;

    if (num_session == 0) {
        const Track& leadout_track = this->sessions.back().tracks.back();
    
        session_info.start_track = this->sessions.size();
        bswap24_to_buf(lba_to_fad(leadout_track.first_lba + leadout_track.track_length), session_info.leadout_fad);
    } else {
        const Session& session = this->sessions[num_session - 1];

        session_info.start_track = session.first_track + 1;
        bswap24_to_buf(lba_to_fad(session.tracks.front().first_lba), session_info.leadout_fad);
    }

    return session_info;
}

static inline u8 set_adr_control(const u8 adr, const u8 control) {
    return (adr << 4) | control;
}

Toc Cdi::read_toc(const bool is_hd_region) {
    assert(is_mounted());

    Toc toc;

    std::memset(&toc, 0, sizeof(toc));

    if (!is_hd_region) {
        u32 num_tracks = 0;

        for (const Session& session : this->sessions) {
            for (const Track& track : session.tracks) {
                TocEntry& entry = toc.track_entries[num_tracks];

                entry.adr_control = set_adr_control(SUB_Q_POSITION, track.control);
                bswap24_to_buf(lba_to_fad(track.first_lba), entry.fad);

                num_tracks++;
            }

            toc.start_track_entry.adr_control = set_adr_control(SUB_Q_NONE, 0);
            bswap24_to_buf(1, toc.start_track_entry.fad);

            toc.end_track_entry.adr_control = set_adr_control(SUB_Q_NONE, 0);
            bswap24_to_buf(num_tracks, toc.end_track_entry.fad);

            toc.leadout_entry.adr_control = set_adr_control(SUB_Q_NONE, 0);
            bswap24_to_buf(1, toc.leadout_entry.fad);
        }
    }
    
    return toc;
}

std::vector<u8> Cdi::read_sectors(const u32 fad, const u32 num_sectors, const bool is_cdda) {
    const u32 sector_size = is_cdda ? 2352 : 2048;

    assert(is_mounted());

    // Prepare buffer for sector data
    std::vector<u8> sector_bytes;

    sector_bytes.resize(sector_size * num_sectors);

    const u32 lba = fad_to_lba(fad);

    // Find correct session/track
    for (const Session& session : this->sessions) {
        for (const Track& track : session.tracks) {
            const u32 first_lba = track.first_lba;
            const u32 track_length = track.track_length;

            if ((lba >= first_lba) && (lba < (first_lba + track_length))) {
                assert((lba + num_sectors) < (first_lba + track_length));

                for (u32 sector = 0; sector < num_sectors; sector++) {
                    read_stream(
                        &sector_bytes[sector_size * sector],
                        sector_size,
                        track.start + track.sector_size * (track.pregap_length + lba - track.first_lba + sector) + !is_cdda * 8,
                        SEEK_SET
                    );
                }

                return sector_bytes;
            }
        }
    }

    assert(false);
}

}
