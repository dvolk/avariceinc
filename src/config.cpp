#include "./config.h"
#include "./util.h"

#include <stdio.h>
#include <allegro5/allegro.h>

static const char *with_default(const char *str, const char *def) {
    if(str == NULL) return def;
    else return str;
}

void Config::save(const char *filename) {
    ALLEGRO_CONFIG *cfg = al_create_config();
    char buf[512];

    snprintf(buf, sizeof(buf), "%d", frame_rate);
    al_set_config_value(cfg, NULL, "frame-rate", buf);
    snprintf(buf, sizeof(buf), "%d", vsync);
    al_set_config_value(cfg, NULL, "v-sync", buf);
    snprintf(buf, sizeof(buf), "%d", fullscreen);
    al_set_config_value(cfg, NULL, "fullscreen", buf);
    snprintf(buf, sizeof(buf), "%d", resolutionScaling);
    al_set_config_value(cfg, NULL, "resolution-scaling", buf);
    snprintf(buf, sizeof(buf), "%d", display_x);
    al_set_config_value(cfg, NULL, "display-x", buf);
    snprintf(buf, sizeof(buf), "%d", display_y);
    al_set_config_value(cfg, NULL, "display-y", buf);
    snprintf(buf, sizeof(buf), "%s", font_filename);
    free(font_filename);
    al_set_config_value(cfg, NULL, "font-filename", buf);
    snprintf(buf, sizeof(buf), "%d", font_height);
    al_set_config_value(cfg, NULL, "font-height", buf);
    snprintf(buf, sizeof(buf), "%d", aa_samples);
    al_set_config_value(cfg, NULL, "aa-samples", buf);
    snprintf(buf, sizeof(buf), "%d", play_music);
    al_set_config_value(cfg, NULL, "play-music", buf);
    snprintf(buf, sizeof(buf), "%d", int(music_volume * 100.0));
    al_set_config_value(cfg, NULL, "music-volume", buf);
    snprintf(buf, sizeof(buf), "%d", custom_cursor);
    al_set_config_value(cfg, NULL, "custom-cursor", buf);
    snprintf(buf, sizeof(buf), "%d", play_ui_sounds);
    al_set_config_value(cfg, NULL, "play-ui-sounds", buf);
    snprintf(buf, sizeof(buf), "%d", int(ui_sound_volume * 100.0));
    al_set_config_value(cfg, NULL, "ui-sound-volume", buf);
    snprintf(buf, sizeof(buf), "%d", esc_menu_quits);
    al_set_config_value(cfg, NULL, "esc-menu-quits", buf);
    snprintf(buf, sizeof(buf), "%d", log_to_file);
    al_set_config_value(cfg, NULL, "log-to-file", buf);
    snprintf(buf, sizeof(buf), "%d", debug_output);
    al_set_config_value(cfg, NULL, "debug-output", buf);

    al_save_config_file(filename, cfg);
    al_destroy_config(cfg);
}

void Config::load(const char *filename) {
    ALLEGRO_CONFIG *cfg = al_load_config_file(filename);

    if(cfg == NULL) {
        info("Couldn't load game.conf");
        cfg = al_create_config();
    } else {
        info("Loaded game.conf");
    }

    const char *s;

    s = al_get_config_value(cfg, 0, "frame-rate");
    frame_rate = atoi(with_default(s, "60"));

    s = al_get_config_value(cfg, 0, "v-sync");
    vsync = atoi(with_default(s, "1"));

    s = al_get_config_value(cfg, 0, "fullscreen");
    fullscreen = atoi(with_default(s, "0"));

    s = al_get_config_value(cfg, 0, "resolution-scaling");
    resolutionScaling = atoi(with_default(s, "1"));

    s = al_get_config_value(cfg, 0, "display-x");
    display_x = atoi(with_default(s, "720"));

    s = al_get_config_value(cfg, 0, "display-y");
    display_y = atoi(with_default(s, "480"));

    s = al_get_config_value(cfg, 0, "font-filename");
    font_filename = strdup(with_default(s, "./DejaVuSans-Bold.ttf"));

    s = al_get_config_value(cfg, 0, "font-height");
    font_height = atoi(with_default(s, "16"));

    s = al_get_config_value(cfg, 0, "aa-samples");
    aa_samples = atoi(with_default(s, "4"));

    s = al_get_config_value(cfg, 0, "play-music");
    play_music = atoi(with_default(s, "1"));

    s = al_get_config_value(cfg, 0, "music-volume");
    music_volume = float(atoi(with_default(s, "50"))) / 100.0;

    s = al_get_config_value(cfg, 0, "custom-cursor");
    custom_cursor = atoi(with_default(s, "1"));

    s = al_get_config_value(cfg, 0, "play-ui-sounds");
    play_ui_sounds = atoi(with_default(s, "1"));

    s = al_get_config_value(cfg, 0, "ui-sound-volume");
    ui_sound_volume = float(atoi(with_default(s, "50"))) / 100.0;

    s = al_get_config_value(cfg, 0, "esc-menu-quits");
    esc_menu_quits = atoi(with_default(s, "0"));

    s = al_get_config_value(cfg, 0, "log-to-file");
    log_to_file = atoi(with_default(s, "0"));

    s = al_get_config_value(cfg, 0, "debug-output");
    debug_output = atoi(with_default(s, "1"));

    al_destroy_config(cfg);
}
