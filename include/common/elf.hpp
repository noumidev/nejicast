/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#pragma once

namespace common::elf {

bool is_elf(const char* path);

void load(const char* path);

}
