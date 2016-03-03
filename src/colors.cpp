#include "./colors.h"

extern Colors colors;

void init_colors(void) {
    colors.very_red    = al_map_rgb(255, 0, 0);
    colors.red_muted   = al_map_rgb(128, 0, 0);
    colors.red         = al_map_rgb(250, 50, 50);
    colors.light_red   = al_map_rgb(240, 128, 128);
    colors.very_blue   = al_map_rgb(0, 0, 255);
    colors.blue_muted  = al_map_rgb(0, 0, 128);
    colors.blue        = al_map_rgb(50, 50, 250);
    colors.light_blue  = al_map_rgb(128, 128, 240);
    colors.black       = al_map_rgb(0, 0, 0);
    colors.white       = al_map_rgb(255, 255, 255);
    colors.grey_middle = al_map_rgb(128, 128, 128);
}
