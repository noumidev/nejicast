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
        assert((frame.recipient_addr & 63) == 0x20);

        frame.receive_bytes.resize(48);

        frame.receive_bytes[0] = MAPLE_DEVICE_CONTROLLER;
        frame.receive_bytes[1] = 0xFE060F00;
        frame.receive_bytes[2] = 0x00000000;
        frame.receive_bytes[3] = 0x724400FF;

        // TODO: fill out remaining structures

        constexpr const char* STRING = "Dreamcast Controller         ";

        std::memcpy(((u8*)frame.receive_bytes.data()) + 18, STRING, 29);

        frame.sender_addr = 0x20;
        frame.result_code = 0x05;
    }

    void get_condition(Frame& frame) override {
        assert(frame.send_bytes[0] == MAPLE_DEVICE_CONTROLLER);
        assert((frame.recipient_addr & 63) == 0x20);

        frame.receive_bytes.push_back(MAPLE_DEVICE_CONTROLLER);

        frame.receive_bytes.push_back(nejicast::get_button_state());
        frame.receive_bytes.push_back(0x80808080);

        frame.sender_addr = 0x20;
        frame.result_code = 0x08;
    }
};

}
