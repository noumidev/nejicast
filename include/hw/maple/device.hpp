/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <vector>

#include <common/types.hpp>

namespace hw::maple {

class MapleDevice {
private:
public:
    MapleDevice() {}
    virtual ~MapleDevice() {}

    virtual std::vector<u8> get_device_info() = 0;
};

}
