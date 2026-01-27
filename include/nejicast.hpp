/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/config.hpp>
#include <common/types.hpp>
#include <common/disc/disc.hpp>

namespace nejicast {

constexpr int SCREEN_WIDTH = 640;
constexpr int SCREEN_HEIGHT = 480;

u16 get_button_state();

void initialize(const common::Config& config);
void reset();
void shutdown();

common::disc::Disc* get_disc();

void sideload(const u32 entry);

}
