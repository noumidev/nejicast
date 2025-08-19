/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <common/config.hpp>

namespace nejicast {

void initialize(const common::Config& config);
void reset();
void shutdown();

void run();

}
