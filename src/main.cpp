#if __has_include(<Arduino.h>)
#include <Arduino.h>
#include <Wire.h>
#else
#include <stdint.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
uint32_t millis() { return pdTICKS_TO_MS(xTaskGetTickCount()); }
void loop();
#endif
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ili9342.h>
#include <esp_i2c.hpp>
#include <m5core2_power.hpp> // AXP192 power management (core2)
#include <uix.hpp> // user interface library
#include <gfx.hpp> // graphics library
#define WARHOL320_IMPLEMENTATION
#include "assets/warhol320.h"

#include "ui.hpp" // ui declarations
#include "panel.hpp" // display panel functionality
using namespace gfx; // graphics
using namespace uix; // user interface
#ifdef ARDUINO
using namespace arduino;
#else
using namespace esp_idf;
#endif


// handle to the display
static esp_lcd_panel_handle_t lcd_handle;
// the transfer buffers
uint8_t* panel_transfer_buffer1 = nullptr;
uint8_t* panel_transfer_buffer2 = nullptr;

extern display disp;

using power_t = m5core2_power;
// for AXP192 power management
static power_t power(esp_i2c<1,21,22>::instance);
gfx::const_buffer_stream warhol_stm(warhol320,sizeof(warhol320));


// tell UIX the DMA transfer is complete
static bool panel_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                                esp_lcd_panel_io_event_data_t* edata, 
                                void* user_ctx) {
    disp.flush_complete();
    
    return true;
}
// tell the lcd panel api to transfer data via DMA
static void panel_on_flush(const rect16& bounds, const void* bmp, void* state) {
    int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1, y2 = bounds.y2 + 1;
    esp_lcd_panel_draw_bitmap(lcd_handle, x1, y1, x2, y2, (void*)bmp);
}

// initialize the screen using the esp panel API
void panel_init() {
    panel_transfer_buffer1 = (uint8_t*)heap_caps_malloc(panel_transfer_buffer_size,MALLOC_CAP_DMA);
    panel_transfer_buffer2 = (uint8_t*)heap_caps_malloc(panel_transfer_buffer_size,MALLOC_CAP_DMA);
    if(panel_transfer_buffer1==nullptr||panel_transfer_buffer2==nullptr) {
        puts("Out of memory allocating transfer buffers");
        while(1) vTaskDelay(5);
    }
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = 18;
    buscfg.mosi_io_num = 23;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = panel_transfer_buffer_size + 8;

    // Initialize the SPI bus on VSPI (SPI3)
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = 15,
    io_config.cs_gpio_num = 5,
    io_config.pclk_hz = 40*1000*1000,
    io_config.lcd_cmd_bits = 8,
    io_config.lcd_param_bits = 8,
    io_config.spi_mode = 0,
    io_config.trans_queue_depth = 10,
    io_config.on_color_trans_done = panel_flush_ready;
    // Attach the LCD to the SPI bus
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle);

    lcd_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config;
    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = -1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#else
    panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
#endif
    panel_config.bits_per_pixel = 16;

    // Initialize the LCD configuration
    esp_lcd_new_panel_ili9342(io_handle, &panel_config, &lcd_handle);

    // Reset the display
    esp_lcd_panel_reset(lcd_handle);

    // Initialize LCD panel
    esp_lcd_panel_init(lcd_handle);
    // esp_lcd_panel_io_tx_param(io_handle, LCD_CMD_SLPOUT, NULL, 0);
    //  Swap x and y axis (Different LCD screens may need different options)
    esp_lcd_panel_swap_xy(lcd_handle, false);
    esp_lcd_panel_set_gap(lcd_handle, 0, 0);
    esp_lcd_panel_mirror(lcd_handle, false, false);
    esp_lcd_panel_invert_color(lcd_handle, true);
    // Turn on the screen
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#else
    esp_lcd_panel_disp_off(lcd_handle, true);
#endif
    disp.buffer_size(panel_transfer_buffer_size);
    disp.buffer1(panel_transfer_buffer1);
    disp.buffer2(panel_transfer_buffer2);
    disp.on_flush_callback(panel_on_flush);
}
// the screen/control definitions
display disp;
screen_t main_screen;
warhol_box_t main_box(main_screen);
#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    Serial.printf("Arduino version: %d.%d.%d\n",ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH);
#else
static void loop_task(void* arg) {
    uint32_t time_ts = millis();
    while(1) {
        loop();
        if(millis()>time_ts+150) {
            time_ts = millis();
            vTaskDelay(1);
        }
    }
}
extern "C" void app_main() {
    printf("ESP-IDF version: %d.%d.%d\n",ESP_IDF_VERSION_MAJOR,ESP_IDF_VERSION_MINOR,ESP_IDF_VERSION_PATCH);
#endif
    power.initialize(); // do this first
    panel_init(); // do this next
    // init the screen and callbacks
    main_screen.dimensions({320,240});
    main_screen.background_color(color_t::black);
    main_box.bounds(main_screen.bounds());
    main_screen.register_control(main_box);
    disp.active_screen(main_screen);
#ifndef ARDUINO
    TaskHandle_t handle;
    xTaskCreatePinnedToCore(loop_task,"loop_task",4096,nullptr,24,&handle,xTaskGetCoreID(xTaskGetCurrentTaskHandle()));
#endif
}

void loop()
{
    static int frames = 0;
    static int time_ts = millis();
    static long long total_ms = 0;
    uint32_t start_ts = millis();
    main_box.invalidate();
    disp.update();
    uint32_t end_ts = millis();
    total_ms += (end_ts-start_ts);
    ++frames;
    if(millis()>=time_ts+1000) {
        if(frames==0) {
            printf("<1 FPS, Total: %dms\n",(int)total_ms);
        } else {
            printf("%d FPS, Avg: %dms\n",frames,(int)(total_ms/frames));
        }
        frames = 0;
        total_ms = 0;
        time_ts = millis();
    }
}
