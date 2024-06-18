#pragma once
#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#endif

#include <gfx.hpp>
#include <uix.hpp>
#include "assets/warhol320.hpp"
// colors for the UI
using color_t = gfx::color<gfx::rgb_pixel<16>>; // native
using color32_t = gfx::color<gfx::rgba_pixel<32>>; // uix

// the screen template instantiation aliases
using screen_t = uix::screen<gfx::rgb_pixel<16>>;
using surface_t = screen_t::control_surface_type;


template<typename ControlSurfaceType>
class warhol_box : public uix::control<ControlSurfaceType> {
   public:
    using control_surface_type = ControlSurfaceType;
    using base_type = uix::control<control_surface_type>;
    using pixel_type = typename base_type::pixel_type;
    using palette_type = typename base_type::palette_type;
    using bitmap_type = gfx::bitmap<pixel_type, palette_type>;
    using color_type = gfx::color<pixel_type>;
    using color32_type = gfx::color<gfx::rgba_pixel<32>>;
   private:
#ifndef ARDUINO
    static uint32_t millis() { return pdTICKS_TO_MS(xTaskGetTickCount()); }
    static int random() { return rand(); }
    static void randomSeed(int value) {return srand(value);}
#endif
    int draw_state;
    bitmap_type m_bmp;
    bitmap_type m_bmp2;
    constexpr static const size_t count = 3;
    constexpr static const int16_t size = 60;
    gfx::spoint16 pts[count];        // locations
    gfx::spoint16 dts[count];        // deltas
    gfx::rgba_pixel<32> cls[count];  // colors
    gfx::rgba_pixel<32> cls_next[count];
    float cls_blend[count];
    gfx::rgba_pixel<32> bg; // background color
    gfx::rgba_pixel<32> bg_next;
    float bg_blend;
    int iteration;
    static void* alloc(size_t size) {
        return heap_caps_malloc(size,MALLOC_CAP_SPIRAM);
    }
    void allocate() {
        deallocate();
        m_bmp = gfx::create_bitmap<pixel_type,palette_type>(gfx::size16(320,240),this->palette(),alloc);
        if(!m_bmp.begin()) {
            m_bmp = bitmap_type({0,0},nullptr);
            return;
        }
        m_bmp2 = gfx::create_bitmap<pixel_type,palette_type>(gfx::size16(320,240),this->palette(),alloc);
        if(!m_bmp2.begin()) {
            free(m_bmp.begin());
            m_bmp = bitmap_type({0,0},nullptr);
            m_bmp2 = bitmap_type({0,0},nullptr);
            return;
        }
        m_bmp.fill(m_bmp.bounds(),pixel_type());
        warhol320.seek(0);
        gfx::size16 dim;
        gfx::jpeg_image::dimensions(&warhol320,&dim);
        warhol320.seek(0);
        gfx::draw::image(m_bmp,dim.bounds().center(m_bmp.bounds()),&warhol320);
        memcpy(m_bmp2.begin(),m_bmp.begin(),bitmap_type::sizeof_buffer(m_bmp.dimensions()));
    }
    void deallocate() {
        if(m_bmp.begin()) {
            free(m_bmp.begin());
            m_bmp = bitmap_type({0,0},nullptr);
        }
        if(m_bmp2.begin()) {
            free(m_bmp2.begin());
            m_bmp2 = bitmap_type({0,0},nullptr);
        }
    }
    gfx::rgba_pixel<32> select_color(int index) {
        gfx::rgba_pixel<32> result;
        switch(index%7) {
            case 0:
                result=color32_t::blue;
                break;
            case 1:
                result = color32_t::red;
                break;
            case 2:
                result = color32_t::orange;
                break;
            case 3:
                result = color32_t::red;
                break;
            case 4:
                result = color32_t::green;
                break;
            case 5:
                result = color32_t::cyan;
                break;
            default:
                result = color32_t::purple;
                break;
        }
        result.template channel<gfx::channel_name::A>((random() % 180) + 32);
        return result;
    }
   public:
    warhol_box(uix::invalidation_tracker &parent, const palette_type *palette = nullptr)
        : base_type(parent, palette) ,draw_state(0),m_bmp({0,0},nullptr) {
    }
    warhol_box(warhol_box &&rhs) : m_bmp({0,0},nullptr) {
        draw_state = 0;
        do_move_control(rhs);
    }
    
    warhol_box &operator=(warhol_box &&rhs) {
        deallocate();
        draw_state= 0;
        do_move_control(rhs);
        return *this;
    }
    warhol_box(const warhol_box &rhs) : m_bmp({0,0},nullptr) {
        draw_state = 0;
        do_copy_control(rhs);
    }
    warhol_box &operator=(const warhol_box &rhs) {
        deallocate();
        draw_state = 0;
        do_copy_control(rhs);
        return *this;
    }
    virtual ~warhol_box() {
       deallocate();
    }
   
    virtual bool on_touch(size_t locations_size, const gfx::spoint16 *locations) {
        return true;
    }
    virtual void on_release() override {
        
    }
    virtual void on_before_render() override {
        
        randomSeed(millis());
        if(draw_state==0) {
            iteration = 0;
            allocate();
            if(m_bmp.begin()) {
                bg_blend = 0;
                bg = select_color(random());
                bg_next = select_color(random());
                size_t i;
                for (i = 0; i < count; ++i) {
                    cls_blend[i]=0;
                    pts[i] = gfx::spoint16((random() % (this->dimensions().width - size)) + size / 2, (random() % (this->dimensions().height - size)) + size / 2);
                    dts[i] = {0, 0};
                    // random deltas. Retry on (dy=0)
                    while (dts[i].x == 0) {
                        dts[i].x = (random() % 5) - 2;
                    }
                    while (dts[i].y == 0) {
                        dts[i].y = (random() % 5) - 2;
                    }
                    cls[i]=select_color(i);
                    cls_next[i]=select_color(random());
                }
                draw_state = 1;
            }
        } else if(iteration==10) {
            iteration = 0;
            if(m_bmp2.begin()) {
                memcpy(m_bmp2.begin(),m_bmp.begin(),bitmap_type::sizeof_buffer(m_bmp.dimensions()));
                gfx::rgba_pixel<32> px;
                bg_next.blend(bg,bg_blend,&px);
                gfx::draw::filled_rectangle(m_bmp2,m_bmp2.bounds(),px);
            }
        } else {
            ++iteration;
        }
    }
    virtual void on_after_render() {
        switch (draw_state) {
            case 0:
                break;
            case 1:
                for (size_t i = 0; i < count; ++i) {
                    gfx::spoint16& pt = pts[i];
                    gfx::spoint16& d = dts[i];
                    // move the bar
                    pt.x += d.x;
                    pt.y += d.y;
                    // if it is about to hit the edge, invert
                    // the respective deltas
                    if (pt.x + d.x + -size / 2 < 0 || pt.x + d.x + size / 2 > this->bounds().x2) {
                        d.x = -d.x;
                    }
                    if (pt.y + d.y + -size / 2 < 0 || pt.y + d.y + size / 2 > this->bounds().y2) {
                        d.y = -d.y;
                    }
                    cls_blend[i]+=.1;
                    if(cls_blend[i]>=1.1) {
                        cls[i]=cls_next[i];
                        cls_next[i]=select_color(random());
                        cls_blend[i]=0;
                    }
                    if(iteration==0) {
                        bg_blend+=.1;
                        if(bg_blend>=1.1) {
                            bg = bg_next;
                            bg_next = select_color(random());
                            bg_blend = 0;
                        }
                    }
                }

                break;
        }
    }
    virtual void on_paint(control_surface_type &destination, const gfx::srect16 &clip) override {
        gfx::srect16 sr=(gfx::srect16)m_bmp2.bounds().center(destination.bounds());
        sr=sr.crop(clip);
        gfx::draw::bitmap(destination,sr,m_bmp2,(gfx::rect16)clip);
        // draw the bars
        for (size_t i = 0; i < count; ++i) {
            gfx::spoint16& pt = pts[i];
            gfx::srect16 r(pt,size/2);
            if (clip.intersects(r)) {
                gfx::rgba_pixel<32> col;
                cls_next[i].blend(cls[i],cls_blend[i],&col);
                gfx::draw::filled_rectangle(destination, r, col, &clip);
            }
        }
    }
};
using warhol_box_t = warhol_box<surface_t>;

// the screen/control declarations
extern screen_t main_screen;
extern warhol_box_t main_box;
