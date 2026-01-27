/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <common/disc/gdi.hpp>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/_types/_seek_set.h>
#include <unistd.h>
#include <vector>

namespace common::disc::gdi {

constexpr int MAX_BUFFER_SIZE = 256;

static const char* get_file_extension(const char* path) {
    const char* dot = std::strrchr(path, '.');

    if (dot) {
        return dot;
    }

    return "";
}

static void replace_file_from_path(const char* new_file, const char* path, char* buf) {
    std::strcpy(buf, path);

    int i;
    
    for (i = std::strlen(buf) - 1; i >= 0; i--) {
        if (buf[i] == '/') {
            break;
        }
    }

    assert(i > 0);

    buf[i + 1] = 0;

    std::strncat(buf, new_file, MAX_BUFFER_SIZE);
}

Gdi::Gdi() {}

Gdi::~Gdi() {}

bool Gdi::load(const char* path) {
    if (std::strcmp(".gdi", get_file_extension(path)) != 0) {
        return false;
    }

    FILE* file = std::fopen(path, "r");

    if (file == nullptr) {
        return false;
    }

    // Line buffer
    char line[MAX_BUFFER_SIZE];

    // Read track number
    if (std::fgets(line, sizeof(line), file) == nullptr) {
        return false;
    }

    const int num_tracks = std::atoi(line);

    if (num_tracks < 3) {
        return false;
    }

    std::printf("GDI Number of tracks = %d\n", num_tracks);

    this->tracks.resize(num_tracks);

    while (std::fgets(line, sizeof(line), file) != nullptr) {
        u32 track_num;
        int first_lba;
        int control;
        int sector_size;
        char track_path[MAX_BUFFER_SIZE];
        int offset;
    
        std::sscanf(
            line,
            "%u %d %d %d %s %d\n",
            &track_num,
            &first_lba,
            &control,
            &sector_size,
            track_path,
            &offset
        );

        assert((track_num > 0) && (track_num <= tracks.size()));
        assert(offset == 0);

        char real_path[MAX_BUFFER_SIZE];

        replace_file_from_path(track_path, path, real_path);

        std::printf(
            "GDI Track %d (first LBA = %d, control = %d, sector size = %d, path = %s, offset = %d)\n",
            track_num,
            first_lba,
            control,
            sector_size,
            real_path,
            offset
        );

        Track& track = tracks[track_num - 1];

        track.first_lba = first_lba;
        track.control = control;
        track.sector_size = sector_size;
        track.file = std::fopen(real_path, "rb");

        if (track.file == nullptr) {
            return false;
        }

        std::fseek(track.file, 0, SEEK_END);

        track.track_length = std::ftell(track.file);

        assert((track.track_length % track.sector_size) == 0);

        track.track_length /= track.sector_size;

        std::fseek(track.file, 0, SEEK_SET);
    }

    return true;
}

bool Gdi::is_mounted() {
    return true;
}

SessionInfo Gdi::request_session(const u8) {
    assert(false);
}

inline u32 fad_to_lba(const u32 fad) {
    constexpr u32 PREGAP = 150;

    return fad - PREGAP;
}

inline u32 lba_to_fad(const u32 lba) {
    constexpr u32 PREGAP = 150;

    return lba + PREGAP;
}

static inline u8 set_adr_control(const u8 adr, const u8 control) {
    return (adr << 4) | control;
}

Toc Gdi::read_toc(const bool is_hd_region) {
    Toc toc;

    std::memset(&toc, 0, sizeof(toc));

    const auto& tracks = this->tracks;

    if (is_hd_region) {
        // This contains all tracks but 0 and 1
        for (usize i = 2; i < tracks.size(); i++) {
            bswap24_to_buf(lba_to_fad(tracks[i].first_lba), toc.track_entries[i].fad);
            toc.track_entries[i].adr_control = set_adr_control(1, tracks[i].control);
        }

        bswap24_to_buf(3, toc.start_track_entry.fad);
        bswap24_to_buf((u32)tracks.size(), toc.end_track_entry.fad);
    } else {
        bswap24_to_buf(lba_to_fad(tracks[0].first_lba), toc.track_entries[0].fad);
        toc.track_entries[0].adr_control = set_adr_control(1, 4);

        bswap24_to_buf(lba_to_fad(tracks[1].first_lba), toc.track_entries[1].fad);
        toc.track_entries[1].adr_control = set_adr_control(1, 0);

        bswap24_to_buf(1, toc.start_track_entry.fad);
        bswap24_to_buf(2, toc.end_track_entry.fad);
    }

    // TODO: leadout track?
    toc.leadout_entry.adr_control = set_adr_control(1, 0);
    bswap24_to_buf((u32)tracks.size() + 1, toc.end_track_entry.fad);

    return toc;
}

std::vector<u8> Gdi::read_sectors(const u32 fad, const u32 num_sectors, const bool is_cdda) {
    const u32 sector_size = is_cdda ? 2352 : 2048;

    assert(is_mounted());

    // Prepare buffer for sector data
    std::vector<u8> sector_bytes;

    sector_bytes.resize(sector_size * num_sectors);

    const u32 lba = fad_to_lba(fad);

    // Find correct track
    for (const Track& track : this->tracks) {
        const u32 first_lba = track.first_lba;
        const u32 track_length = track.track_length;

        if ((lba >= first_lba) && (lba < (first_lba + track_length))) {
            assert(sector_size <= track.sector_size);
            assert((lba + num_sectors) < (first_lba + track_length));

            for (u32 sector = 0; sector < num_sectors; sector++) {
                std::fseek(track.file, track.sector_size * (lba - track.first_lba + sector) + !is_cdda * 16, SEEK_SET);
                std::fread(&sector_bytes[sector_size * sector], sizeof(u8), sector_size, track.file);
            }

            return sector_bytes;
        }
    }

    assert(false);
}

}
