#include "./ui.h"

#include <cmath>

#include "util.h"

extern float mouse_x;
extern float mouse_y;
extern float scale;
extern int mouse_button;
extern float vx(float x);
extern float vy(float y);
extern void set_redraw(void);
extern void dispatch_hex_click(Widget *widget);

static bool (*global_key_callback)(void) = NULL;

void register_global_key_callback(bool (*cb)(void)) {
    global_key_callback = cb;
}

void UI::addWidget(Widget *w) {
    widgets.push_back(w);
}

void UI::addWidgets(std::vector<Widget *> ws) {
    for(auto w: ws)
        addWidget(w);
}

bool UI::is_hit(Widget *widget) {
    bool hit = false;
    float x1 = widget->m_x1;
    float y1 = widget->m_y1;
    float x2 = widget->m_x2;
    float y2 = widget->m_y2;
    float circle_bb_radius = widget->m_circle_bb_radius;

    if(widget->m_type == WidgetType::Hex) {
        x1 = vx(x1);
        y1 = vy(y1);
        x2 = vx(x2);
        y2 = vy(y2);
        circle_bb_radius *= scale;
    }

    if(widget->m_circle_bb == false) {
        hit =  x1 <= mouse_x
            && y1 <= mouse_y
            && x2 >= mouse_x
            && y2 >= mouse_y;
    } else {
        hit = pow((mouse_x - (x1 + circle_bb_radius)), 2)
            + pow((mouse_y - (y1 + circle_bb_radius)), 2)
            < pow(circle_bb_radius, 2);
    }

    return hit;
}

void UI::mouseDownEvent(void) {
    debug("%d", mouse_button);
    if(mouse_button == 1) {
        for(auto& widget : widgets) {
            if(widget->m_visible == false)
                continue;

            bool hit = is_hit(widget);

            if(hit == true) {
                debug("UI::mouseDownEvent(): hit!");
                widget->mouseDownEvent();
                if(widget->onMouseDown != nullptr) {
                    widget->onMouseDown();
                }
                if(widget->m_type == WidgetType::Hex) {
                    dispatch_hex_click(widget);
                }
                set_redraw();
                return;
            }
        }
    }
}

void UI::keyDownEvent(void) {
    set_redraw();
    if(global_key_callback != NULL) {
        if(global_key_callback() == true)
            return;
    }
}

void UI::update(void) {
    for(auto& widget : widgets) {
        widget->update();
    }
}

void UI::draw(void) {
    for(auto& widget : widgets) {
        if(widget->m_visible)
            widget->draw();
    }
}

