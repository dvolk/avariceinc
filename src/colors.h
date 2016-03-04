#pragma once

#include <allegro5/allegro.h>
#include <allegro5/allegro_color.h>

void init_colors(void);

struct Colors {
    ALLEGRO_COLOR very_red;
    ALLEGRO_COLOR red_muted;
    ALLEGRO_COLOR red;
    ALLEGRO_COLOR light_red;
    ALLEGRO_COLOR very_blue;
    ALLEGRO_COLOR blue_muted;
    ALLEGRO_COLOR blue;
    ALLEGRO_COLOR light_blue;
    ALLEGRO_COLOR black;
    ALLEGRO_COLOR white;
    ALLEGRO_COLOR grey_middle;
    ALLEGRO_COLOR grey_dim;
};
