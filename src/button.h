#pragma once

#include "./widget.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_color.h>

struct Button : Widget {
    const char *m_name;
    float m_x_off;
    float m_y_off;
    bool m_pressed;
    ALLEGRO_COLOR m_color;

    Button() {}
    explicit Button(const char *_name);

    void draw(void) override;
    void mouseDownEvent(void) override;
    void set_offsets(void);
};

