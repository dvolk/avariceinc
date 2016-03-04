#include "./sidebutton.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_color.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>

#include "./colors.h"

extern bool is_opt_button(SideButton *b);
extern void clear_opt_buttons(void);
extern Side get_current_side(void);

extern ALLEGRO_FONT *g_font;
extern Colors colors;

SideButton::SideButton(const char *_name) {
    m_name = _name;
    m_type = WidgetType::Button;
    m_pressed = false;
    m_x_off = 0;
    m_y_off = 0;
    m_outlined = false;
    m_outline_cond_1 = 0;
    m_outline_cond_2 = 0;
    m_outline_right = true;
}

void SideButton::draw(void) {
    ALLEGRO_COLOR c = m_pressed ? colors.red_muted : colors.red;
    if(get_current_side() == Side::Blue) {
        c = m_pressed ? colors.blue_muted : colors.blue;
    }
    if(m_outlined == false) {
        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, colors.grey_dim);
        if(m_name != NULL) {
            al_draw_text(g_font, colors.grey_middle, m_x1 + m_x_off,
                         m_y1 + m_y_off, 0, m_name);
        }
    } else {

        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, c);
        if(m_name != NULL) {
            al_draw_text(g_font, colors.white, m_x1 + m_x_off,
                         m_y1 + m_y_off, 0, m_name);
        }
    }
}

void SideButton::mouseDownEvent(void) {
    clear_opt_buttons();
    if(is_opt_button(this) == true) {
        this->m_pressed = true;
    }
}

void SideButton::outline(int cur) {
    m_outlined = false;
    if(cur >= m_outline_cond_1) {
        m_outlined = true;
    }
}
