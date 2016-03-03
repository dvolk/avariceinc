#include "./button.h"

#include <cmath>

#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>

#include "./colors.h"
#include "./config.h"

extern ALLEGRO_FONT *g_font;
extern Colors colors;
extern Config cfg;

Button::Button(const char *_name) {
    m_name = _name;
    m_type = WidgetType::Button;
    m_x_off = 0;
    m_y_off = 0;
    m_pressed = false;
    m_color = colors.red;
}

void Button::draw(void) {
    al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, m_color);
    if(m_name != NULL) {
        al_draw_text(g_font, colors.white, m_x1 + m_x_off,
                     m_y1 + m_y_off, 0, m_name);
    }
}

void Button::mouseDownEvent(void) {
    m_pressed = true;
}

void Button::set_offsets(void) {
    m_x_off = round((m_x2 - m_x1 - al_get_text_width(g_font, m_name)) / 2);
    m_y_off = round((m_y2 - m_y1 - cfg.font_height) / 2);
}
