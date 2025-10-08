/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <vector>

#include <common/types.hpp>
#include <hw/maple/device.hpp>

namespace hw::maple {

class Controller : public MapleDevice {
private:
public:
    Controller() {}
    ~Controller() {}

    std::vector<u8> get_device_info() override {

    }
};

}
