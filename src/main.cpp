#include <Arduino.h>
#include <Wire.h>
#include <m5core2_power.hpp> // AXP192 power management (core2)
#include <uix.hpp> // user interface library
#include <gfx.hpp> // graphics library
#include "ui.hpp" // ui declarations
#include "panel.hpp" // display panel functionality

using namespace gfx; // graphics
using namespace uix; // user interface

using power_t = m5core2_power;
// for AXP192 power management
static power_t power(Wire1);

// the screen/control definitions
screen_t main_screen;
warhol_box_t main_box(main_screen);

void setup() {
    Serial.begin(115200);
    Serial.printf("Arduino version: %d.%d.%d\n",ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH);
    power.initialize(); // do this first
    panel_init(); // do this next
    // init the screen and callbacks
    main_screen.dimensions({320,240});
    main_screen.buffer_size(panel_transfer_buffer_size);
    main_screen.buffer1(panel_transfer_buffer1);
    main_screen.buffer2(panel_transfer_buffer2);
    main_screen.background_color(color_t::black);
    
    main_box.bounds(main_screen.bounds());
    main_screen.register_control(main_box);
    panel_set_active_screen(main_screen);
}

void loop()
{
    static int frames = 0;
    static int time_ts = millis();
    static long long total_ms = 0;
    uint32_t start_ts = millis();
    main_box.invalidate();
    panel_update();
    uint32_t end_ts = millis();
    total_ms += (end_ts-start_ts);
    ++frames;
    if(millis()>=time_ts+1000) {
        if(frames==0) {
            Serial.printf("<1 FPS, Total: %dms\n",(int)total_ms);
        } else {
            Serial.printf("%d FPS, Avg: %dms\n",frames,(int)(total_ms/frames));
        }
        frames = 0;
        total_ms = 0;
        time_ts = millis();
    }
}