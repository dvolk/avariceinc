#pragma once

#include <vector>

#include <allegro5/allegro.h>
#include <allegro5/allegro_color.h>

#include "./widget.h"

void register_global_key_callback(bool (*cb)(void));

struct UI {
    std::vector<Widget *> widgets;

    ALLEGRO_COLOR clear_to;

    UI() { clear_to = al_map_rgb(0, 0, 0); }
    virtual ~UI() { }

    bool is_hit(Widget *w);
    virtual void mouseDownEvent(void);
    virtual void mouseUpEvent(void) {}
    virtual void keyDownEvent(void);
    virtual void hoverOverEvent(void) {}

    virtual void update(void);
    virtual void draw(void);

    void addWidget(Widget *w);
    void addWidgets(std::vector<Widget *> ws);
};
