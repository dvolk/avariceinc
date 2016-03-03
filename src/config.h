#pragma once

#include <cstdint>

struct Config {
    int8_t frame_rate;
    bool vsync;
    bool fullscreen;
    bool resolutionScaling;
    int16_t display_x;
    int16_t display_y;
    char *font_filename;
    int8_t font_height;
    int8_t aa_samples;
    bool custom_cursor;
    bool play_music;
    float music_volume;
    bool play_ui_sounds;
    float ui_sound_volume;
    bool esc_menu_quits;
    bool log_to_file;
    bool debug_output;

    void save(const char *filename);
    void load(const char *filename);
};
