/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

// Thanks to washingtonDC for a lot of this code (https://github.com/washingtondc-emu/washingtondc/blob/master/src/libwashdc/cdi.c)

#include <common/cdi.hpp>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace common::cdi {

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

struct Cdi {
    FILE* file;

    std::vector<Session> sessions;
};

struct {
    Cdi cdi;
} ctx;

static void seek_stream(const long offset, const int whence = SEEK_CUR) {
    if (ctx.cdi.file == nullptr) {
        std::puts("Can't seek in unopened file");
        exit(1);
    }

    std::fseek(ctx.cdi.file, offset, whence);
}

static void read_stream(u8* bytes, const usize size, const long offset = 0, const int whence = SEEK_CUR) {
    if (ctx.cdi.file == nullptr) {
        std::puts("Can't read from unopened file");
        exit(1);
    }

    seek_stream(offset, whence);

    if (std::fread(bytes, sizeof(u8), size, ctx.cdi.file) != size) {
        std::printf("Couldn't read %zu bytes @ %ld from CDI\n", size, offset);
        exit(1);
    }
}

static long read_track(Track& track, long pos, const u32 cdi_version) {
    constexpr u8 TRACK_START[] = {
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF
    };

    u8 track_start[14];

    read_stream(track_start, sizeof(track_start));

    if (std::memcmp(track_start, TRACK_START, sizeof(track_start)) != 0) {
        std::printf("CDI Invalid track start pattern (");

        for (usize i = 0; i < sizeof(track_start); i++) {
            std::printf("%02X", track_start[i]);

            if (i == (sizeof(track_start) - 1)) {
                std::printf(")\n");
            } else {
                std::printf(" ");
            }
        }

        exit(1);
    }

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

    switch (select_sector_size) {
        case 1:
            track.sector_size = 2336;
            break;
        case 2:
            track.sector_size = 2352;
            break;
        default:
            std::printf("CDI Invalid sector size select byte %u\n", select_sector_size);
            exit(1);
    }

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

static long read_session(Session& session, long pos, const u32 cdi_version) {
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

void load(const char* path) {
    std::memset(&ctx, 0, sizeof(ctx));

    ctx.cdi.file = std::fopen(path, "rb");

    if (ctx.cdi.file == nullptr) {
        std::puts("Failed to open CDI");
        exit(1);
    }

    u32 version;
    int header_offset;

    read_stream((u8*)&version, sizeof(version), -8, SEEK_END);
    read_stream((u8*)&header_offset, sizeof(header_offset));

    std::printf("CDI version = %08X, header offset = %08X\n", version, header_offset);

    // Seek to header
    switch (version) {
        case 0x80000004:
            seek_stream(header_offset, SEEK_SET);
            break;
        case 0x80000006:
            // Header is at the end of the CDI file
            seek_stream(-header_offset, SEEK_END);
            break;
        default:
            std::printf("Unimplemented CDI version %08X\n", version);
            exit(1);
    }

    u16 num_sessions;

    read_stream((u8*)&num_sessions, sizeof(num_sessions));

    std::printf("Number of sessions = %u\n", num_sessions);

    ctx.cdi.sessions.resize(num_sessions);

    u32 num_tracks = 0;

    long pos = 0;

    for (Session& session : ctx.cdi.sessions) {
        pos = read_session(session, pos, version);

        seek_stream(3);

        session.first_track = num_tracks;

        num_tracks += session.tracks.size();
    }
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

SessionInfo request_session(const u8 num_session) {
    if (ctx.cdi.file == nullptr) {
        std::puts("CDI Failed to request session info (CDI not loaded)");
        exit(1);
    }

    if (num_session > ctx.cdi.sessions.size()) {
        std::puts("CDI Session number out of bounds");
        exit(1);
    }

    SessionInfo session_info;

    if (num_session == 0) {
        const Track& leadout_track = ctx.cdi.sessions.back().tracks.back();
    
        session_info.start_track = ctx.cdi.sessions.size();
        bswap24_to_buf(lba_to_fad(leadout_track.first_lba + leadout_track.track_length), session_info.leadout_fad);
    } else {
        const Session& session = ctx.cdi.sessions[num_session - 1];

        session_info.start_track = session.first_track + 1;
        bswap24_to_buf(lba_to_fad(session.tracks.front().first_lba), session_info.leadout_fad);
    }

    return session_info;
}

static inline u8 set_adr_control(const u8 adr, const u8 control) {
    return (adr << 4) | control;
}

Toc read_toc(const bool second_layer) {
    if (ctx.cdi.file == nullptr) {
        std::puts("CDI Failed to read TOC (CDI not loaded)");
        exit(1);
    }

    Toc toc;

    std::memset(&toc, 0, sizeof(toc));

    if (!second_layer) {
        u32 num_tracks = 0;

        for (const Session& session : ctx.cdi.sessions) {
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

std::vector<u8> read_sectors(const u32 fad, const u32 num_sectors) {
    constexpr u32 SECTOR_SIZE = 2048;

    if (ctx.cdi.file == nullptr) {
        std::puts("CDI Failed to read sectors (CDI not loaded)");
        exit(1);
    }

    // Prepare buffer for sector data
    std::vector<u8> sector_bytes;

    sector_bytes.resize(SECTOR_SIZE * num_sectors);

    const u32 lba = fad_to_lba(fad);

    // Find correct session/track
    for (const Session& session : ctx.cdi.sessions) {
        for (const Track& track : session.tracks) {
            const u32 first_lba = track.first_lba;
            const u32 track_length = track.track_length;

            if ((lba >= first_lba) && (lba < (first_lba + track_length))) {
                if ((lba + num_sectors) > (first_lba + track_length)) {
                    std::puts("CDI LBA out of bounds");
                    exit(1);
                }

                for (u32 sector = 0; sector < num_sectors; sector++) {
                    read_stream(
                        &sector_bytes[SECTOR_SIZE * sector],
                        SECTOR_SIZE,
                        track.start + track.sector_size * (track.pregap_length + lba - track.first_lba + sector) + 8,
                        SEEK_SET
                    );
                }

                return sector_bytes;
            }
        }
    }

    std::puts("CDI Failed to read sectors");
    exit(1);
}

}
