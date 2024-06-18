#pragma once
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
   private:
    int draw_state;
    bitmap_type m_bmp;
    constexpr static const size_t count = 6;
    constexpr static const int16_t width = 4;
    gfx::spoint16 pts[count];        // locations
    gfx::spoint16 dts[count];        // deltas
    gfx::rgba_pixel<32> cls[count];  // colors
    static void* alloc(size_t size) {
        return heap_caps_malloc(size,MALLOC_CAP_SPIRAM);
    }
    void allocate() {
        deallocate();
        m_bmp = gfx::create_bitmap<pixel_type,palette_type>(gfx::size16(320,240),this->palette(),alloc);
        if(!m_bmp.begin()) {
            return;
        }
        m_bmp.fill(m_bmp.bounds(),pixel_type());
        warhol320.seek(0);
        gfx::size16 dim;
        gfx::jpeg_image::dimensions(&warhol320,&dim);
        warhol320.seek(0);
        gfx::draw::image(m_bmp,dim.bounds().center(m_bmp.bounds()),&warhol320);
    }
    void deallocate() {
        if(m_bmp.begin()) {
            free(m_bmp.begin());
            m_bmp = bitmap_type({0,0},nullptr);
        }
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
            allocate();
            if(m_bmp.begin()) {
                size_t i;
                for (i = 0; i < count; ++i) {
                    if ((i & 1)) {
                        pts[i] = gfx::spoint16(0, (random() % (this->dimensions().height - width)) + width / 2);
                        dts[i] = {0, 0};
                        // random deltas. Retry on (dy=0)
                        while (dts[i].y == 0) {
                            dts[i].y = (random() % 5) - 2;
                        }
                    } else {
                        pts[i] = gfx::spoint16((random() % (this->dimensions().width - width)) + width / 2, 0);
                        dts[i] = {0, 0};
                        // random deltas. Retry on (dx=0)
                        while (dts[i].x == 0) {
                            dts[i].x = (random() % 5) - 2;
                        }
                    }
                    // random color RGBA8888
                    gfx::ega_palette<gfx::rgba_pixel<32>> pal;
                    int ci = 1;
                    while(ci==0 || ci==1 || ci==8 || ci==9) {
                        ci=(random()%8)+8;
                    }
                    gfx::indexed_pixel<4> epx(ci);
                    gfx::rgba_pixel<32> px;
                    pal.map(epx,&px);
                    px.template channel<gfx::channel_name::A>((random() % 180) + 32);
                    cls[i]=px;
                }
                draw_state = 1;
            }
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
                    if (pt.x + d.x + -width / 2 < 0 || pt.x + d.x + width / 2 > this->bounds().x2) {
                        d.x = -d.x;
                    }
                    if (pt.y + d.y + -width / 2 < 0 || pt.y + d.y + width / 2 > this->bounds().y2) {
                        d.y = -d.y;
                    }
                }
                break;
        }
    }
    virtual void on_paint(control_surface_type &destination, const gfx::srect16 &clip) override {
        gfx::srect16 sr=(gfx::srect16)m_bmp.bounds().center(destination.bounds());
        sr=sr.crop(clip);
        gfx::draw::bitmap(destination,sr,m_bmp,(gfx::rect16)clip);
        // draw the bars
        for (size_t i = 0; i < count; ++i) {
            gfx::spoint16& pt = pts[i];
            gfx::spoint16& d = dts[i];
            gfx::srect16 r;
            if (d.y == 0) {
                r = gfx::srect16(pt.x - width / 2, 0, pt.x + width / 2, this->bounds().y2);
            } else {
                r = gfx::srect16(0, pt.y - width / 2, this->bounds().x2, pt.y + width / 2);
            }
            if (clip.intersects(r)) {
                gfx::rgba_pixel<32>& col = cls[i];
                gfx::draw::filled_rectangle(destination, r, col, &clip);
            }
        }
    }
};
using warhol_box_t = warhol_box<surface_t>;

// the screen/control declarations
extern screen_t main_screen;
extern warhol_box_t main_box;
