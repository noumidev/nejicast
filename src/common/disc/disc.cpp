/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025-2026  noumidev
 */

#include <common/disc/disc.hpp>

#include <common/disc/cdi.hpp>
#include <common/disc/gdi.hpp>
#include <common/disc/dummy.hpp>

namespace common::disc {

Disc* mount_disc(const char* path) {
    // Try to mount CDI
    Disc* disc = new cdi::Cdi();

    if (disc->load(path)) {
        return disc;
    }

    delete disc;

    // Try to mount GDI
    disc = new gdi::Gdi();

    if (disc->load(path)) {
        return disc;
    }

    delete disc;

    // Return null disc
    return new DummyDisc;
}

}
