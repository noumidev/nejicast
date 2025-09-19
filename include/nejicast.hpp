/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

#include <common/config.hpp>
#include <common/types.hpp>

namespace nejicast {

constexpr int SCREEN_WIDTH = 640;
constexpr int SCREEN_HEIGHT = 480;

void initialize(const common::Config& config);
void reset();
void shutdown();

void sideload(const u32 entry);

}
