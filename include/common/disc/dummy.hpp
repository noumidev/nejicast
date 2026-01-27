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

namespace common::disc {

class DummyDisc : public Disc {
private:
public:
    DummyDisc() {}
    ~DummyDisc() {}

    u8 get_disc_format() override {
        return DISC_FORMAT_NONE;
    }

    bool load(const char*) override {
        return false;
    };

    bool is_mounted() override {
        return false;
    };

    Toc read_toc(const bool) override {
        return Toc{};
    }

    SessionInfo request_session(const u8) override {
        return SessionInfo{};
    };

    std::vector<u8> read_sectors(const u32, const u32, const bool) override {
        return std::vector<u8>{};
    }
};

}
