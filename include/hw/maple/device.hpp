/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <vector>

#include <common/types.hpp>
#include <hw/maple/maple.hpp>

namespace hw::maple {

class MapleDevice {
private:
public:
    MapleDevice() {}
    virtual ~MapleDevice() {}

    virtual void get_device_info(Frame& frame) = 0;
    virtual void get_condition(Frame& frame) = 0;
};

}
