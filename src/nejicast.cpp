/*
 * nejicast is a Sega Dreamcast emulator.
 * Copyright (C) 2025  noumidev
 */

#include <nejicast.hpp>

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>

#include <scheduler.hpp>
#include <common/elf.hpp>
#include <hw/cpu/cpu.hpp>
#include <hw/g1/g1.hpp>
#include <hw/g2/g2.hpp>
#include <hw/holly/bus.hpp>
#include <hw/holly/holly.hpp>
#include <hw/maple/maple.hpp>
#include <hw/pvr/pvr.hpp>

constexpr int NUM_ARGS = 4;

namespace nejicast {

void initialize(const common::Config& config) {
    scheduler::initialize();

    hw::cpu::initialize();
    hw::g1::initialize(config.boot_path, config.flash_path);
    hw::g2::initialize();
    hw::holly::initialize();
    hw::maple::initialize();
    hw::pvr::initialize();

    common::load_elf(config.elf_path);
}

void shutdown() {
    scheduler::shutdown();

    hw::cpu::shutdown();
    hw::g1::shutdown();
    hw::g2::shutdown();
    hw::holly::shutdown();
    hw::maple::shutdown();
    hw::pvr::shutdown();
}

void reset() {
    scheduler::reset();

    hw::cpu::reset();
    hw::g1::reset();
    hw::g2::reset();
    hw::holly::reset();
    hw::maple::reset();
    hw::pvr::reset();
}

void sideload(const u32 entry) {
    hw::holly::bus::setup_for_sideload();
    hw::cpu::setup_for_sideload(entry);
}

}

using nejicast::SCREEN_WIDTH;
using nejicast::SCREEN_HEIGHT;

static struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Texture* texture;
} screen;

SDL_AppResult SDL_AppInit(void**, int argc, char** argv) {
    if (argc < NUM_ARGS) {
        std::puts("Usage: nejicast [path to boot ROM] [path to FLASH ROM] [path to ELF]");

        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer(
            "nejicast",
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            0,
            &screen.window,
            &screen.renderer
        )
    ) {
        SDL_Log("Failed to create window and renderer: %s", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    screen.texture = SDL_CreateTexture(
        screen.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (screen.texture == nullptr) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    common::Config config {
        .boot_path = argv[1],
        .flash_path = argv[2],
        .elf_path = argv[3]
    };
    
    nejicast::reset();
    nejicast::initialize(config);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void*, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void*) {
    // Run emulator for a frame
    while (scheduler::run()) {}

    SDL_UpdateTexture(screen.texture, nullptr, hw::pvr::get_color_buffer_ptr(), sizeof(u32) * SCREEN_WIDTH);
    SDL_RenderClear(screen.renderer);
    SDL_RenderTexture(screen.renderer, screen.texture, nullptr, nullptr);
    SDL_RenderPresent(screen.renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void*, SDL_AppResult) {
    SDL_DestroyTexture(screen.texture);
    SDL_DestroyRenderer(screen.renderer);
    SDL_DestroyWindow(screen.window);

    nejicast::shutdown();
}
