/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <cassert>
#include <vector>

#include <nejicast.hpp>
#include <common/types.hpp>
#include <hw/maple/device.hpp>
#include <hw/maple/maple.hpp>

namespace hw::maple {

class Controller : public MapleDevice {
private:
public:
    Controller() {}
    ~Controller() {}

    void get_device_info(Frame& frame) override {
        frame.receive_bytes.push_back(MAPLE_DEVICE_CONTROLLER);

        // TODO
        for (int i = 0; i < 47; i++) {
            frame.receive_bytes.push_back(0);
        }

        frame.result_code = 0x05;
    }

    void get_condition(Frame& frame) override {
        assert(frame.send_bytes[0] == MAPLE_DEVICE_CONTROLLER);

        frame.receive_bytes.push_back(MAPLE_DEVICE_CONTROLLER);

        frame.receive_bytes.push_back(nejicast::get_button_state());
        frame.receive_bytes.push_back(0x80808080);

        frame.result_code = 0x08;
    }
};

}
