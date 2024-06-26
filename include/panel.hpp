#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ui.hpp"
// use two uffers (DMA)
constexpr const size_t panel_transfer_buffer_size = 320*60*2;
extern uint8_t* panel_transfer_buffer1;
extern uint8_t* panel_transfer_buffer2;

void panel_init();

void panel_update();

void panel_set_active_screen(screen_t& new_screen);