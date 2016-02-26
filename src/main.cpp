#include <allegro5/allegro.h>
#include <allegro5/allegro_color.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_opengl.h>

#include <cstdint>
#include <cmath>

#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

#include "./util.h"

using namespace std;

struct HexMap;
struct Game;
struct SideFlag;
struct SideButton;
struct MessageLog;
struct SideInfo;

struct UI;
struct MainMenuUI;
struct GameSetupUI;
struct MapUI;

ALLEGRO_DISPLAY *display;
ALLEGRO_KEYBOARD_STATE keyboard_state;
ALLEGRO_MOUSE_STATE mouse_state;
ALLEGRO_EVENT_QUEUE *event_queue;
ALLEGRO_TIMER *timer;
ALLEGRO_FONT *g_font;

bool running;
float mouse_x;
float mouse_y;
int display_x;
int display_y;
int mouse_button;
int key;
HexMap *map;
UI *ui;
MainMenuUI *MainMenu_UI;
GameSetupUI *GameSetup_UI;
MapUI *Map_UI;
float view_x = -65;
float view_y = 0;
float scale = 1;
double dt;
Game *game;
vector<SideButton *> opt_buttons;
MessageLog *msg;
SideInfo *sideinfo1;
SideInfo *sideinfo2;

static bool (*global_key_callback)(void) = NULL;

inline float vx(float x) {
    return round((x - view_x) * scale);
}
inline float vy(float y) {
    return round((y - view_y) * scale);
}

enum class WidgetType {
    Hex,
    Button,
    SideButton,
    MenuEntry,
    MapSelectButton,
    Other,
};

enum class Side {
    Red,
    Blue,
    Neutral,
};

enum class MapAction {
    MovingUnits,
    BuildHarvester,
    DestroyHarvester,
    AddAmmoToCannon,
    BuildCannon,
    FireCannon,
    BuildWalker,
    BuildArmory,
    BuildCarrier,
};

ALLEGRO_COLOR color_very_red;
ALLEGRO_COLOR color_red_muted;
ALLEGRO_COLOR color_red;
ALLEGRO_COLOR color_light_red;
ALLEGRO_COLOR color_very_blue;
ALLEGRO_COLOR color_blue_muted;
ALLEGRO_COLOR color_blue;
ALLEGRO_COLOR color_light_blue;
ALLEGRO_COLOR color_black;
ALLEGRO_COLOR color_white;
ALLEGRO_COLOR color_grey_middle;

static inline void draw_hex(float x, float y, float a, float zrot, float cr, float cg, float cb, float r, float d);

static void init_colors(void) {
    color_very_red    = al_map_rgb(255, 0, 0);
    color_red_muted   = al_map_rgb(128, 0, 0);
    color_red         = al_map_rgb(250, 50, 50);
    color_light_red   = al_map_rgb(240, 128, 128);
    color_very_blue   = al_map_rgb(0, 0, 255);
    color_blue_muted  = al_map_rgb(0, 0, 128);
    color_blue        = al_map_rgb(50, 50, 250);
    color_light_blue  = al_map_rgb(128, 128, 240);
    color_black       = al_map_rgb(0, 0, 0);
    color_white       = al_map_rgb(255, 255, 255);
    color_grey_middle = al_map_rgb(128, 128, 128);
}

const char *map_action_to_string(MapAction act) {
    if(act == MapAction::MovingUnits) {
        return "moving units";
    } else if(act == MapAction::BuildHarvester) {
        return "build harvester";
    } else if(act == MapAction::DestroyHarvester) {
        return "destroy harvester";
    } else if(act == MapAction::AddAmmoToCannon) {
        return "make ammo for cannon";
    } else if(act == MapAction::BuildCannon) {
        return "build cannon";
    } else if(act == MapAction::FireCannon) {
        return "fire cannon";
    } else if(act == MapAction::BuildWalker) {
        return "build walker";
    } else if(act == MapAction::BuildArmory) {
        return "build armory";
    } else if(act == MapAction::BuildCarrier) {
        return "build carrier";
    }
    return "bug";
}

struct Widget {
public:
    float m_x1;
    float m_y1;
    float m_x2;
    float m_y2;

    bool m_visible;
    bool m_below;
    bool m_circle_bb;
    float m_circle_bb_radius;
    WidgetType m_type;

    Widget();
    Widget(float x1, float y1, float x2, float y2);

    virtual ~Widget() {};

    void init(void);

    virtual void mouseDownEvent(void) {};
    virtual void mouseUpEvent(void) {};
    virtual void keyDownEvent(void) {};
    virtual void hoverOverEvent(void) {};

    void (*onMouseDown)(void);
    void (*onMouseUp)(void);
    void (*onKeyDown)(void);

    virtual void update() {};
    virtual void draw() = 0;

    void draw_bb(void) {
        al_draw_rectangle(vx(m_x1), vy(m_y1), vx(m_x2), vy(m_y2), color_very_red, 1);
        al_draw_circle(vx(m_x1 + m_circle_bb_radius),
                       vy(m_y1 + m_circle_bb_radius),
                       m_circle_bb_radius * scale,
                       color_very_red,
                       1);
    }

    void setpos(float x1, float y1, float x2, float y2) {
        m_x1 = x1;
        m_y1 = y1;
        m_x2 = x2;
        m_y2 = y2;
    }
};

struct MessageLog : public Widget {
    vector<string> m_lines;
    bool m_hide;

    void draw(void);
    void hide(void) {
        m_hide = true;
    }

    void add(const char *format_string, ...);
};

void MessageLog::draw(void) {
    if(m_lines.empty() == true or m_hide == true)
        return;
    // bg
    const char *txt = m_lines.back().c_str();
    const float txtlen = al_get_text_width(g_font, txt);

    al_draw_filled_rectangle(0, display_y - 25, txtlen + 10, display_y, color_black);

    al_draw_text(g_font, color_white, 5, display_y - 20, 0, txt);
}

void MessageLog::add(const char *format_string, ...) {
    char str[256];
    va_list args;
    va_start(args, format_string);
    vsnprintf(str, sizeof(str), format_string, args);
    va_end(args);

    m_lines.push_back(str);
    m_hide = false;
}

struct UI {
    std::vector<Widget *> widgets;

    ALLEGRO_COLOR clear_to;

    virtual ~UI() { }

    bool is_hit(Widget *w);
    virtual void mouseDownEvent(void);
    virtual void mouseUpEvent(void) {};
    virtual void keyDownEvent(void);
    virtual void hoverOverEvent(void) {};

    virtual void update(void);
    virtual void draw(void);

    void addWidget(Widget *w);
    void addWidgets(vector<Widget *> ws);
};

struct SideButton;
static Side get_current_side(void);
static bool is_opt_button(SideButton *b);
static void clear_opt_buttons(void);

struct Button : Widget {
    const char *m_name;
    float m_x_off;
    float m_y_off;
    bool m_active;
    bool m_pressed;
    ALLEGRO_COLOR m_color = color_red;

    explicit Button(const char *_name) {
        m_name = _name;
        m_type = WidgetType::Button;
        m_x_off = 0;
        m_y_off = 0;
        m_active = false;
        m_pressed = false;
    }

    void draw(void) override {
        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, m_color);
        if(m_name != NULL) {
            al_draw_text(g_font, color_white, m_x1 + m_x_off,
                         m_y1 + m_y_off, 0, m_name);
        }
        //draw_bb();
    }

    void mouseDownEvent(void) override {
        m_pressed = true;
    }

    void set_offsets(void) {
        m_x_off = round((m_x2 - m_x1 - al_get_text_width(g_font, m_name)) / 2);
        m_y_off = round((m_y2 - m_y1 - 14) / 2);
    }
};

struct SideButton : Widget {
    const char *m_name;
    bool m_pressed;
    float m_x_off;
    float m_y_off;

    explicit SideButton(const char *_name) {
        m_name = _name;
        m_type = WidgetType::Button;
        m_pressed = false;
        m_x_off = 0;
        m_y_off = 0;
    }

    void draw(void) override {
        ALLEGRO_COLOR c = m_pressed ? color_red_muted : color_red;
        if(get_current_side() == Side::Blue) {
            c = m_pressed ? color_blue_muted : color_blue;
        }

        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, c);
        if(m_name != NULL) {
            al_draw_text(g_font, color_white, m_x1 + m_x_off,
                         m_y1 + m_y_off, 0, m_name);
        }
        //draw_bb();
    }
    void mouseDownEvent(void) override {
        clear_opt_buttons();
        if(is_opt_button(this) == true) {
            this->m_pressed = true;
        }
    }
    void set_offsets(void) {
        m_x_off = round((m_x2 - m_x1 - al_get_text_width(g_font, m_name)) / 2);
        m_y_off = round((m_y2 - m_y1 - 14) / 2);
    }
};

struct Hex;

struct AIAction {
    AIAction(MapAction act, Hex *src, Hex *dst, int amount) {
        m_act = act;
        m_src = src;
        m_dst = dst;
        m_amount = amount;
    }

    MapAction m_act;
    Hex *m_src;
    Hex *m_dst;
    int m_amount;
};

struct SideController {
    Side m_side;
    const char *m_name;
    int m_resources;
    int m_carriers;
    ALLEGRO_COLOR m_color;
    bool m_ai_control;

    SideController() { }

    explicit SideController(Side s) {
        m_resources = 12;
        m_carriers = 0;
        m_side = s;
        m_ai_control = true;

        if(s == Side::Red) {
            m_name = "Red";
            m_color = color_red;
        }
        else if(s == Side::Blue) {
            m_name = "Blue";
            m_color = color_blue;
        }
        else {
            fatal_error("SideController(): invalid side %d", (int)s);
        }
    }

    bool is_AI(void) {
        return m_ai_control;
    }

    vector<AIAction> do_AI(void);
};

struct HexMap {
    int m_moving_units = 1;
    int m_buying_units = 1;
    const int m_max_units_moved = 8;
    const float m_cannon_min_range = 1;
    const float m_cannon_max_range = 3;

    vector<Hex *> m_hexes;

    struct StoredState {
        SideController m_sc;
        vector<Hex> m_hexes;
    };

    vector<StoredState> m_old_states;
    vector<vector<Hex *>> m_neighbors;

    HexMap() { }
    ~HexMap();

    float hex_distance(Hex *h1, Hex *h2);
    void gen_neighbors(void);
    vector<Hex *> neighbors(Hex *base);
    bool is_neighbor(Hex *h1, Hex *h2);
    void harvest(void);
    void move_or_attack(Hex *attacker, Hex *defender);
    void free_units(void);
    Hex *get_active_hex(void);
    vector<Hex *> BFS(Hex *, int range);

    void store_current_state(void);
    void clear_old_states(void);
    void undo(void);
};

static void clear_active_hex(void);

struct Game {
    vector<SideController *> players;

    Game() {
        players.push_back(new SideController(Side::Red));
        players.push_back(new SideController(Side::Blue));
        m_current_controller = players[0];
    }
    ~Game() {
        for(auto&& player : players) delete player;
    }

    SideController *m_current_controller;
    SideController *get_next_controller();

    SideController *controller(void) {
        return m_current_controller;
    }
    int& controller_resources(void) {
        return controller()->m_resources;
    }
    int& controller_carriers(void) {
        return controller()->m_carriers;
    }
    void controller_pay(int n) {
        controller_resources() -= n;
    }
    bool controller_has_resources(int n) {
        return controller_resources() >= n;
    }
};

static Side get_current_side(void) {
    return game->m_current_controller->m_side;
}

SideController *Game::get_next_controller() {
    vector<SideController *>::iterator it = find(players.begin(), players.end(), m_current_controller);

    // wrap around
    if(++it == players.end()) {
        m_current_controller = players[0];
        return players[0];
    }
    else {
        m_current_controller = *it;
        return *it;
    }
}

struct SideInfo : Widget {
    SideController *m_s;
    float m_x_off;
    float m_y_off;

    explicit SideInfo(SideController *s) {
        m_s = s;
        m_x_off = 0;
        m_y_off = 0;
    }

    void draw(void) override {
        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, m_s->m_color);
        al_draw_textf(g_font, color_white, m_x1 + m_x_off,
                      m_y1 + m_y_off, 0,
                      "%d/%d", m_s->m_resources, m_s->m_carriers);
    }
    void set_offsets(void) {
        char tmp[10];
        snprintf(tmp, sizeof(tmp), "%d/%d", m_s->m_resources, m_s->m_carriers);
        m_x_off = round((m_x2 - m_x1 - al_get_text_width(g_font, tmp)) / 2);
        m_y_off = round((m_y2 - m_y1 - 14) / 2);
    }
};

static bool marked_hexes(void);

struct Hex : Widget {
    int m_index;
    int m_level;
    float m_a;
    float m_r;
    bool m_active;
    bool m_marked;
    float m_cx;
    float m_cy;

    bool m_contains_harvester;
    bool m_harvested;
    bool m_contains_armory;
    bool m_contains_cannon;
    int m_ammo;
    int m_loaded_ammo;

    int m_units_free;
    int m_units_moved;

    Side m_side;

    Hex(float x1, float y1, float a, int level, int index) {
        m_index = index;

        // radius of inscribed circle
        float r = round(0.5 * sqrt(3) * a);

        m_x1 = round(x1);
        m_y1 = round(y1);
        m_a = round(a);
        m_r = r;
        m_cx = m_x1 + r;
        m_cy = m_y1 + r;
        m_x2 = m_x1 + 2 * r;
        m_y2 = m_y1 + 2 * r;
        m_circle_bb = true;
        m_circle_bb_radius = r;

        m_active = false;
        m_marked = false;
        m_type = WidgetType::Hex;

        m_level = level;
        m_contains_harvester = false;
        m_harvested = false;
        m_contains_armory = false;
        m_contains_cannon = false;
        m_ammo = 0;
        m_loaded_ammo = 0;
        m_units_free = 0;
        m_units_moved = 0;

        m_side = Side::Neutral;
    }

    void update(void) override {
        if(m_level == 0) {
            if(m_a > 5) {
                m_a -= 0.1 * m_circle_bb_radius;
            }
            else {
                m_level = -1;
            }
        }
    }

    void draw(void) override {
        if(m_level == -1) return;

        float r = 0.5;
        float g = 0.5;
        float b = 0.5;

        if(m_side == Side::Red) {
            if(m_active == true) {
                r = 0.98;
                g = 0.2;
                b = 0.2;
            }
            else {
                r = 0.94;
                g = 0.5;
                b = 0.5;
            }
        }
        else if(m_side == Side::Blue) {
            if(m_active == true) {
                r = 0.2;
                g = 0.2;
                b = 0.98;
            }
            else {
                r = 0.5;
                g = 0.5;
                b = 0.94;
            }
        }

        ALLEGRO_COLOR txt_color = color_white;

        if(m_marked == true) {
            const float space = 2;
            draw_hex(vx(m_cx), vy(m_cy),
                     scale * (m_a - space), 0, 1, 1, 1,
                     0.5 * sqrt(3) * (m_a - space) * scale,
                     sin(1.0 / 2.0) * (m_a - space) * scale);
        }
        else if(m_active == false and marked_hexes() == true) {
            r /= 3;
            g /= 3;
            b /= 3;
            txt_color = color_grey_middle;
        }

        const float space = 4;

        draw_hex(vx(m_cx), vy(m_cy), scale * (m_a - space), 0, r, g, b,
                 0.5 * sqrt(3) * (m_a - space) * scale,
                 sin(1.0 / 2.0) * (m_a - space) * scale);

        if(m_level == 0) return;

        al_draw_textf(g_font,
                      txt_color,
                      vx(m_cx) - 5,
                      vy(m_cy) - 7,
                      0,
                      "%d", m_level);

        if(m_contains_harvester == true)
            al_draw_text(g_font, txt_color,
                         vx(m_cx) - 25, vy(m_cy) - 25,
                         0, "H");

        if(m_contains_armory == true)
            al_draw_text(g_font, txt_color,
                         vx(m_cx) - 5, vy(m_cy) - 25,
                         0, "A");

        if(m_contains_cannon == true)
            al_draw_text(g_font, txt_color,
                         vx(m_cx) + 15, vy(m_cy) - 25,
                         0, "C");

        if(m_ammo > 0 or m_loaded_ammo > 0)
            al_draw_textf(g_font, txt_color,
                         vx(m_cx) + 15, vy(m_cy) - 7,
                          0, "%d", m_ammo + m_loaded_ammo);

        if(m_units_free > 0 or m_units_moved > 0)
            al_draw_textf(g_font, txt_color,
                          vx(m_cx) - 12, vy(m_cy) + 10,
                          0, "%d/%d", m_units_free, m_units_moved);
    }

    void mouseDownEvent(void) override {
        if(m_level < 1)
            return;
    }

    bool alive(void) {
        return m_level > 0;
    }

    bool harvested(void) {
        return m_harvested;
    }

    void harvest(void) {
        assert(m_level > 0);
        m_level -= 1;
        m_harvested = true;
    }

    void destroy_units(int n) {
        int rest = m_units_free - n;
        m_units_free -= min(m_units_free, n);
        if(rest < 0) {
            m_units_moved += rest;
            m_units_moved = max(0, m_units_moved);
        }
    }
};

HexMap::~HexMap()
{
}

vector<AIAction> SideController::do_AI(void) {
    // save the state before, do the ai while saving individual actions, undo the map, then replay it slowly
    map->store_current_state();
    vector<AIAction> ret;

    vector<Hex *> mine;
    for(auto&& h : map->m_hexes) {
        if(h->alive() && h->m_side == m_side) {
            mine.push_back(h);
        }
    }

    for(auto&& h : mine) {
        for(auto&& neighbor : map->neighbors(h)) {
            if(neighbor->alive() and h->m_units_free >= 1 and neighbor->m_side != h->m_side) {
                map->m_moving_units = 1;
                map->move_or_attack(h, neighbor);
                ret.push_back(AIAction(MapAction::MovingUnits, h, neighbor, 1));
            }
        }
    }

    map->undo();
    debug("number of ai actions: %d", ret.size());
    return ret;
}

static void goto_mainmenu(void);

struct MapUI : UI {
private:
    MapAction m_current_action;
public:
    vector<AIAction> m_ai_acts;
    bool m_ai_replay;
    int m_ai_acts_stage;
    int m_ai_play_time;
    bool m_game_won;
    bool m_game_lost;
    const int m_ai_play_delay = 15;
    bool m_draw_buttons;
    bool m_marked_hexes;
    float m_turn_anim;

    MapUI() {
        m_current_action = MapAction::MovingUnits;
        m_ai_replay = false;
        m_ai_acts_stage = 0;
        m_ai_play_time = 0;
        m_game_won = false;
        m_game_lost = false;
        m_draw_buttons = true;
        m_marked_hexes = false;
        m_turn_anim = 0;
    }
    ~MapUI() {
        for(auto&& w : widgets) delete w;
    }

    void mouseDownEvent(void) override;
    void keyDownEvent(void) override {
        if(m_game_won or m_game_lost) {
            goto_mainmenu();
        } else {
            if(key == ALLEGRO_KEY_H) m_draw_buttons = !m_draw_buttons;
            UI::keyDownEvent();
        }
    }

    void draw(void) override;
    void update(void) override;

    void set_current_action(MapAction act) {
        debug("MapUI::set_current_action(): %s", map_action_to_string(act));
        if(act == MapAction::MovingUnits)
            clear_opt_buttons();
        m_current_action = act;
    }
    MapAction get_current_action(void) {
        return m_current_action;
    }

    void MapHexSelected(Hex *h);

    void ai_play(vector<AIAction> acts);
    bool ai_replay(void);

    void mark(vector<Hex *> hs) {
        for(auto&& h : hs)
            if(h->alive())
                h->m_marked = true;
        m_marked_hexes = true;
    }
    void clear_mark(void) {
        for(auto&& h : map->m_hexes) h->m_marked = false;
        m_marked_hexes = false;
    }
};

static bool marked_hexes(void) {
    return Map_UI->m_marked_hexes;
}

void MapUI::ai_play(vector<AIAction> acts) {
    debug("MapUI::ai_play() %d", acts.size());
    m_ai_acts = acts;
    m_ai_acts_stage = 0;
    m_ai_replay = true;
    msg->add("AI turn. Hold 's' to skip ahead.");
}

bool MapUI::ai_replay(void) {
    debug("MapUI::ai_replay()");
    if(m_ai_acts.empty() == true or m_ai_acts_stage >= (int)m_ai_acts.size()) {
        debug("MapUI::ai_replay() exit, stage: %d", m_ai_acts_stage);
        return false;
    }

    AIAction a = m_ai_acts[m_ai_acts_stage];

    if(a.m_act == MapAction::MovingUnits) {
        map->m_moving_units = a.m_amount;
        map->move_or_attack(a.m_src, a.m_dst);
    }
    else {
        fatal_error("not implemented yet");
    }

    m_ai_acts_stage++;
    return true;
}

struct MainMenuUI : UI {
    float m_rot;

    MainMenuUI();
    ~MainMenuUI() {
        for(auto&& w : widgets)
            if(w->m_type == WidgetType::MenuEntry) delete w;
    }
    void draw(void) override;
    void update(void) override {
        m_rot += 1;
    }

    void handlePress(const char *name);
};

struct GameSetupUI : UI {
    Button *m_selected_map;
    Button *m_selected_player;

    vector<Button *> m_player_select_btns;
    vector<Button *> m_map_select_btns;
    
    GameSetupUI();
    ~GameSetupUI() { for(auto&& w: widgets) delete w; }
};

static void new_game(void);
static void btn_begin_cb(void);
static void btn_map_select_cb(void);
static void btn_player_select_cb(void);

GameSetupUI::GameSetupUI() {
    Button *btn_begin = new Button("Begin");
    btn_begin->setpos(display_x/2 - 40, display_y - 70,
                      display_x/2 + 40, display_y - 30);
    btn_begin->onMouseDown = btn_begin_cb;
    btn_begin->set_offsets();
    addWidget(btn_begin);

    int y = 45;

    vector<Widget *> map_btns;

    for(auto&& map_name : { "Europe", "North America", "Euroasia",
                "Australia", "Indochina", "Siberia",
                "Pacific Domains", "Africa", "South America" })
        {
            Button *btn_map = new Button(map_name);
            int xsize = 10 + al_get_text_width(g_font, map_name);
            btn_map->setpos(150,  y,
                            150 + xsize, y + 30);
            btn_map->onMouseDown = btn_map_select_cb;
            btn_map->set_offsets();
            btn_map->m_type = WidgetType::MapSelectButton;
            map_btns.push_back(btn_map);

            y += 35;
        }

    addWidgets(map_btns);

    // press the first button
    Button *btn = static_cast<Button*>(map_btns.front());
    btn->m_pressed = true;
    btn->m_color = color_red_muted;
    m_selected_map = btn;

    y = 150;
    Button *btn_blue_player_ai = new Button("vs. A.I.");
    int xsize = 10 + al_get_text_width(g_font, "vs. A.I.");
    btn_blue_player_ai->setpos(display_x - 200 - xsize,  y,
                               display_x - 200, y + 30);
    btn_blue_player_ai->onMouseDown = btn_player_select_cb;
    btn_blue_player_ai->set_offsets();
    addWidget(btn_blue_player_ai);

    Button *btn_blue_player_human = new Button("vs. Human");
    xsize = 10 + al_get_text_width(g_font, "vs. Human");
    btn_blue_player_human->setpos(display_x - 200 - xsize, y + 35,
                                  display_x - 200, y + 65);
    btn_blue_player_human->onMouseDown = btn_player_select_cb;
    btn_blue_player_human->set_offsets();
    btn_blue_player_human->m_color = color_blue;
    addWidget(btn_blue_player_human);

    btn_blue_player_ai->m_pressed = true;
    btn_blue_player_ai->m_color = color_blue_muted;
    m_selected_player = btn_blue_player_ai;

    m_player_select_btns = { btn_blue_player_ai, btn_blue_player_human };
}

static void btn_map_select_cb(void) {
    for(auto&& b : GameSetup_UI->m_map_select_btns) {
        if(b->m_pressed == true) {
            GameSetup_UI->m_selected_map = b;
            b->m_color = color_red_muted;
        }
    }
    for(auto&& b : GameSetup_UI->m_map_select_btns) {
        b->m_pressed = false;
        if(b != GameSetup_UI->m_selected_map) {
            b->m_color = color_red;
        }
    }
}

static void btn_player_select_cb(void) {
    for(auto&& b : GameSetup_UI->m_player_select_btns) {
        if(b->m_pressed == true) {
            GameSetup_UI->m_selected_player = b;
            b->m_color = color_blue_muted;
        }
    }
    for(auto&& b : GameSetup_UI->m_player_select_btns) {
        b->m_pressed = false;
        if(b != GameSetup_UI->m_selected_player) {
            b->m_color = color_blue;
        }
    }
}

static void btn_begin_cb(void) {
    new_game();
    ui = Map_UI;
}

static inline void draw_hex(float x, float y, float a, float zrot, float cr, float cg, float cb, float r, float d) {
    glLoadIdentity();
    glTranslatef(x, y, 0);
    glRotatef(zrot, 0, 0, 1);

    glBegin(GL_POLYGON);
    glColor3f(cr, cg, cb);
    glVertex3f(-a/2, - r, 0);
    glVertex3f(a/2, - r, 0);
    glVertex3f(d + a/2, 0, 0);
    glVertex3f(a/2, r, 0);
    glVertex3f(-a/2, r, 0);
    glVertex3f(-a/2 - d, 0, 0);
    glEnd();
}

void MainMenuUI::draw(void) {
    glPushMatrix();
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, display_x, display_y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, display_x, display_y, 0, -200, 200);
    glClearColor(0,0,0,1);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);

    draw_hex(display_x/2, display_y/2, 150, m_rot, 0.8, 0.2, 0.2, 0.5*sqrt(3)*150, 150*sin(1.0/2.0));
    draw_hex(display_x/2, display_y/2, 100, -m_rot, 0.2, 0.2, 0.8, 0.5*sqrt(3)*100, 100*sin(1.0/2.0));

    // glLoadIdentity();
    // glTranslatef(display_x/2, display_y/2, 0);
    // glRotatef(rot, 0, 0, 1);

    // glBegin(GL_POLYGON);
    // glColor3f(0.5, 0, 0);
    // glVertex3f(-a/2, - 0.5 * sqrt(3) * a, 0);
    // glVertex3f(a/2, - 0.5 * sqrt(3) * a, 0);
    // glVertex3f(a * sin(1.0/2.0) + a/2, 0, 0);
    // glVertex3f(a/2, 0.5 * sqrt(3) * a, 0);
    // glVertex3f(-a/2, 0.5 * sqrt(3) * a, 0);
    // glVertex3f(-a/2 - a * sin(1.0/2.0), 0, 0);
    // // glVertex3f(-100, -100, 0);
    // // glVertex3f(100, -100, 0);
    // // glVertex3f(100, 100, 0);
    // // glVertex3f(-100, 100, 0);
    // glEnd();

    glFlush();
    glPopMatrix();

    //al_draw_circle(display_x/2, display_y/2, 0.5 * sqrt(3) * a - 2, color_white, 3);
    UI::draw();
}

struct MenuEntry : public Widget {
    const char *m_name;
    int m_x_off;
    int m_y_off;

    explicit MenuEntry(const char *name) {
        m_name = name;
        m_type = WidgetType::MenuEntry;
        m_x_off = 0;
        m_y_off = 0;
    }

    void draw(void) override;
    void mouseDownEvent(void) {
        MainMenu_UI->handlePress(m_name);
    }

    void set_offsets(void) {
        m_x_off = round((m_x2 - m_x1 - al_get_text_width(g_font, m_name)) / 2);
        m_y_off = round((m_y2 - m_y1 - 14) / 2);
    }
};

void MenuEntry::draw(void) {
    al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, color_red);
    al_draw_text(g_font, color_white, m_x1 + m_x_off, m_y1 + m_y_off, 0, m_name);
}

static void delete_game(void);
static void deinit(void);

void MainMenuUI::handlePress(const char *name) {
    if(strcmp(name, "New") == 0) {
        if(Map_UI != NULL) delete_game();
        ui = GameSetup_UI;
    }
    else if(strcmp(name, "Exit") == 0) {
        if(Map_UI != NULL) delete_game();
        deinit();
        exit(0);
    }
    else {
        fatal_error("MainMenuUI::HandlePress(): unknown button pressed");
    }
}

MainMenuUI::MainMenuUI() {
    MenuEntry *e1 = new MenuEntry("New");
    e1->m_x1 = (display_x / 2) - 50;
    e1->m_x2 = (display_x / 2) + 50;
    e1->m_y1 = display_y / 2 - 55;
    e1->m_y2 = display_y / 2 - 5;
    e1->set_offsets();

    MenuEntry *e2 = new MenuEntry("Exit");
    e2->m_x1 = (display_x / 2) - 50;
    e2->m_x2 = (display_x / 2) + 50;
    e2->m_y1 = display_y / 2 + 5;
    e2->m_y2 = display_y / 2 + 55;
    e2->set_offsets();

    addWidgets({ e1, e2 });

    m_rot = 0;
}

void MapUI::mouseDownEvent(void) {
    if(m_ai_replay == true or m_game_won or m_game_lost)
        return;

    if(mouse_button == 2) {
        msg->hide();
        set_current_action(MapAction::MovingUnits);
        clear_active_hex();
    }
    UI::mouseDownEvent();
}

void Widget::init(void) {
    m_x1 = 0;
    m_y1 = 0;
    m_x2 = 100;
    m_y2 = 100;

    m_visible = true;
    m_below = false;
    m_circle_bb = false;
    m_circle_bb_radius = 10;

    onMouseDown = nullptr;
    onMouseUp = nullptr;
    onKeyDown = nullptr;

    m_type = WidgetType::Other;
}

Widget::Widget() {
    init();
}

Widget::Widget(float x1, float y1, float x2, float y2) {
    init();

    m_x1 = x1;
    m_y1 = y1;
    m_x2 = x2;
    m_y2 = y2;
}

void UI::addWidget(Widget *w) { widgets.push_back(w); }
void UI::addWidgets(vector<Widget *> ws) {
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
    if(mouse_button == 1) {
        for(auto& widget : widgets) {
            bool hit = is_hit(widget);

            if(hit == true) {
                widget->mouseDownEvent();
                if(widget->onMouseDown != nullptr) {
                    widget->onMouseDown();
                }
            }

            if(hit == true and widget->m_type == WidgetType::Hex) {
                Hex *h = static_cast<Hex *>(widget);
                Map_UI->MapHexSelected(h);
            }
        }
    }
}

void UI::keyDownEvent(void) {
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

vector<Hex *> HexMap::BFS(Hex *base, int range) {
    struct bfsdata {
        float distance;

        bfsdata() { distance = -1; }
    };
    
    vector<bfsdata> hexdata(m_hexes.size());
    
    deque<Hex *> q;
    q.push_back(base);

    hexdata[base->m_index].distance = 0;

    while(not q.empty()) {
        Hex *cur = q.back();
        q.pop_back();
        for(auto&& neighbor : neighbors(cur)) {
            
            bool not_visited = hexdata[neighbor->m_index].distance == -1;
            bool ok_side_or_base_neighbor =
                neighbor->m_side == base->m_side || cur == base;
            
            if(neighbor->alive() && not_visited && ok_side_or_base_neighbor) {

                hexdata[neighbor->m_index].distance =
                    hexdata[cur->m_index].distance + 1;

                if(neighbor->m_side == base->m_side &&
                   hexdata[neighbor->m_index].distance <= range)
                    q.push_back(neighbor);
            }
        }
    }

    vector<Hex *> ret;
    for(auto&& h : m_hexes) {
        if(hexdata[h->m_index].distance != -1
           && hexdata[h->m_index].distance <= range)
            {
                ret.push_back(h);
            }
    }
    return ret;
}

void HexMap::undo(void) {
    if(m_old_states.empty() == true)
        return;

    SideController &old_cont = m_old_states.back().m_sc;
    vector<Hex> old_hexes = m_old_states.back().m_hexes;

    debug("undo to %p", old_hexes);
    msg->add("Undo!");

    assert(old_hexes.size() == m_hexes.size());

    for(size_t i = 0; i < m_hexes.size(); i++) {
        *m_hexes[i] = old_hexes[i];
    }

    *(game->controller()) = old_cont;

    m_old_states.pop_back();
    clear_active_hex();
}

void HexMap::store_current_state(void) {
    vector<Hex> copy;

    for(auto&& h : m_hexes) {
        copy.push_back(*h);
    }

    StoredState stored_state;
    stored_state.m_sc = *(game->controller());
    stored_state.m_hexes = copy;

    m_old_states.push_back(stored_state);

    debug("HexMap::store_current_state(): undo states: %d", m_old_states.size());
}

void HexMap::clear_old_states(void) {
    m_old_states.clear();

}
Hex *HexMap::get_active_hex(void) {
    for(auto&& h : m_hexes) {
        if(h->m_active == true) {
            return h;
        }
    }
    return NULL;
}

float HexMap::hex_distance(Hex *h1, Hex *h2) {
    return sqrt(pow(h1->m_cx - h2->m_cx, 2)
                + pow(h1->m_cy - h2->m_cy, 2));
}

vector<Hex *> HexMap::neighbors(Hex *base) {
    return m_neighbors[base->m_index];
}

void HexMap::gen_neighbors() {
    m_neighbors.clear();
    for(auto&& base : m_hexes) {
        vector<Hex *> neighbors;
        for(auto&& h : m_hexes) {
            if(hex_distance(base, h) < 2 * base->m_r + 10) {
                if(h != base)
                    neighbors.push_back(h);
            }
        }
        m_neighbors.push_back(neighbors);
    }
}


bool is_neighbor(Hex *h1, Hex *h2) {
    vector<Hex *> neighbors = map->neighbors(h1);
    return find(neighbors.begin(), neighbors.end(), h2) != neighbors.end();
}

void HexMap::harvest(void) {
    for(auto&& h : m_hexes) {
        h->m_harvested = false;
    }
    for(auto&& h : m_hexes) {
        if(h->alive() && h->m_contains_harvester == true && h->m_side == get_current_side()) {
            for(auto&& h_neighbor : neighbors(h)) {
                if(h_neighbor->alive() == true &&
                   h_neighbor->harvested() == false)
                    {
                        h_neighbor->harvest();
                        game->controller_resources() += 2;
                    }
            }
            h->harvest();
            game->controller_resources() += 2;
        }
    }
}

void HexMap::move_or_attack(Hex *attacker, Hex *defender) {
    assert(defender != NULL);
    assert(defender->m_level >= 1);
    assert(attacker != NULL);
    assert(attacker->m_level >= 1);
    assert(attacker->m_units_free >= 0);

    if(defender->m_side == Side::Neutral or
       defender->m_side == attacker->m_side) {
        // moving units
        int moved =
            min({
                    m_moving_units,
                    attacker->m_units_free,
                    m_max_units_moved
               });

        defender->m_units_moved += moved;
        defender->m_side = attacker->m_side;
        attacker->m_units_free -= moved;
        debug("%p moves %d to %p", defender, moved, attacker);
    }
    else {
        // attacking
        int moved =
            min({
                    m_moving_units,
                    attacker->m_units_free,
                    m_max_units_moved
               });

        debug("%p attacked %p with %d", defender, attacker, moved);
        if(moved >= defender->m_units_free + defender->m_units_moved) {
            // we've conquered this hex
            defender->m_units_moved = moved - (defender->m_units_free + defender->m_units_moved);
            defender->m_side = attacker->m_side;
            defender->m_units_free = 0;
            attacker->m_units_free -= moved;
        } else {
            // attacked but not conquered
            attacker->m_units_free -= moved;

            if(moved < defender->m_units_free) {
                defender->m_units_free -= moved;
            } else {
                defender->m_units_moved -= (moved - defender->m_units_free);
                defender->m_units_free = 0;
            }
        }
    }

}

void HexMap::free_units(void) {
    for(auto&& h : m_hexes) {
        if(h->m_side == get_current_side()) {
            h->m_units_free += h->m_units_moved;
            h->m_units_moved = 0;

            // transfer cannon ammo
            h->m_ammo += h->m_loaded_ammo;
            h->m_loaded_ammo = 0;
        }
    }
}

void MapUI::MapHexSelected(Hex *h) {
    debug("MapUI::MapHexSelected()");
    Hex *prev = NULL;
    for(auto&& h : map->m_hexes) {
        if(h->m_active == true) {
            prev = h;
            break;
        }
    }

    clear_active_hex();
    if(prev == NULL && h->m_side != game->m_current_controller->m_side) {
        msg->add("You don't own that tile.");
        return;
    }
    if(h->m_side != Side::Neutral and
       h->m_side == game->m_current_controller->m_side and
       prev == NULL) {
        h->m_active = true;

        if(get_current_action() == MapAction::MovingUnits) {
            msg->add("Moving %d units.", map->m_moving_units);
        }
    }

    if(get_current_action() == MapAction::MovingUnits) {
        if(prev == NULL) {
            mark(map->BFS(h, 4));
        }

        else if (prev != NULL) {
            if(h != prev && prev->m_units_free > 0 && h->alive()) {
                vector<Hex *> bfs = map->BFS(prev, 4);
                bool free_move = find(bfs.begin(), bfs.end(), h) != bfs.end();
            
                if(free_move == true) {
                    map->store_current_state();
                    map->move_or_attack(prev, h);
                    clear_active_hex();
                }
                else if(game->controller()->m_carriers >= 1) {
                    map->store_current_state();
                    map->move_or_attack(prev, h);
                    clear_active_hex();
                    game->controller()->m_carriers -= 1;
                }
                else {
                    clear_active_hex();
                }
            }
        }
    }
    else if (get_current_action() == MapAction::FireCannon) {
        if(prev == NULL) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            msg->add("Select the cannon first.");
            return;
        }

        if(prev->m_contains_cannon == false) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            msg->add("Selected hex doesn't contain a cannon.");
            return;
        }

        float dist = map->hex_distance(prev, h);

        if(dist < (0.4 + 2 * map->m_cannon_min_range) * prev->m_circle_bb_radius or dist > (0.4 + 2 * map->m_cannon_max_range) * prev->m_circle_bb_radius) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            msg->add("Target isn't in range.");
            return;
        }

        if(prev->m_ammo <= 0) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            msg->add("That cannon doesn't have any loaded ammo.");
            return;
        }

        msg->add("Firing!");
        map->store_current_state();
        prev->m_ammo -= 1;
        h->m_level -= 1;
        h->destroy_units(8);
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildHarvester) {

        if(h->m_contains_harvester == false) {
            map->store_current_state();
            game->controller_pay(10);
            h->m_contains_harvester = true;
        }

        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::DestroyHarvester) {
        if(h->m_contains_harvester == true) {
            map->store_current_state();
            for(auto&& n : map->neighbors(h)) {
                if(n->alive() == true) {
                    n->harvest();
                }
            }
            h->harvest();
            h->m_contains_harvester = false;
        }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::AddAmmoToCannon) {
        // if(prev == NULL) {
        //     set_current_action(MapAction::MovingUnits);
        //     clear_active_hex();
        //     return;
        // }
        if(h->m_contains_cannon == true) {
            map->store_current_state();
            game->controller_pay(20);
            h->m_loaded_ammo++;
        }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildArmory) {
        if(h->m_contains_armory == false) {
            map->store_current_state();
            game->controller_pay(35);
            h->m_contains_armory = true;
        }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildCannon) {
        if(h->m_contains_cannon == false) {
            map->store_current_state();
            game->controller_pay(30);
            h->m_contains_cannon = true;
        }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildWalker) {
        if(h->m_contains_armory == true) {
            if(game->controller_resources() >= map->m_buying_units * 8) {
                map->store_current_state();
                h->m_units_moved += map->m_buying_units;
                game->controller_pay(map->m_buying_units * 8);
            } else {
                msg->add("You don't have enough to get %d units",
                         map->m_buying_units);
            }
        } else {
            msg->add("You can only recruit on hexes with armories.");
        }

        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
}

void MapUI::draw(void) {
    // draw normal hexes
    for(auto&& h : map->m_hexes) {
        h->draw();
    }

    if(m_game_won or m_game_lost) {
        al_draw_filled_rectangle(display_x/2 - 150, display_y/2 - 50,
                                 display_x/2 + 150, display_y/2 + 50,
                                 color_red);

        const char *txt = "You've won!";
        if(m_game_lost)
            txt = "You've lost!";

        int x_off = al_get_text_width(g_font, txt) / 2;

        al_draw_text(g_font, color_white, display_x/2 - x_off, display_y/2-20, 0, txt);

        txt = "Press any key to continue.";
        x_off = al_get_text_width(g_font, txt) / 2;

        al_draw_text(g_font, color_white, display_x/2 - x_off, display_y/2+4, 0, txt);
        return;
    }

    // draw all other widgets
    if(m_draw_buttons == true) {
        for(auto&& w : widgets) {
            if(w->m_type != WidgetType::Hex) {
                w->draw();
            }
        }
    }
}

static void end_turn_cb(void);

void MapUI::update(void) {
    UI::update();

    if(m_ai_replay == true) {
        if(al_key_down(&keyboard_state, ALLEGRO_KEY_S)) {
            while(ai_replay()) {};
            m_ai_acts.clear();
            m_ai_replay = false;
            end_turn_cb();
            return;
        }

        if(m_ai_play_time >= m_ai_play_delay) {
            if(ai_replay() == false) {
                m_ai_acts.clear();
                m_ai_replay = false;
                end_turn_cb();
                return;
            }
            m_ai_play_time = 0;
        }
        m_ai_play_time++;
        return;
    }

    int d = 100 * dt;

    if(al_key_down(&keyboard_state, ALLEGRO_KEY_UP))
        view_y -= d;
    else if(al_key_down(&keyboard_state, ALLEGRO_KEY_DOWN))
        view_y += d;

    if(al_key_down(&keyboard_state, ALLEGRO_KEY_LEFT))
        view_x -= d;
    else if(al_key_down(&keyboard_state, ALLEGRO_KEY_RIGHT))
        view_x += d;

    float ds = 1.02;
    if(al_key_down(&keyboard_state, ALLEGRO_KEY_OPENBRACE)) {
        scale *= ds;
    }
    else if(al_key_down(&keyboard_state, ALLEGRO_KEY_CLOSEBRACE)) {
        scale /= ds;
    }
}

static void goto_mainmenu(void) {
    ui = MainMenu_UI;
}

static void allegro_init() {
    bool ret = 0;
    init_logging();

    al_init(); // no return value
    if(al_is_system_installed() == false)
        fatal_error("Failed to initialize core allegro library.");
    else
        info("Initialized core allegro library.");

    ret = al_init_primitives_addon();
    if(ret == false)
        fatal_error("Failed to initialize allegro addon: primitives.");
    else
        info("Initialized allegro addon: primitives.");

    ret = al_init_image_addon();
    if(ret == false)
        fatal_error("Failed to initialize allegro addon: image.");
    else
        info("Initialized allegro addon: image.");

    al_init_font_addon(); // no return value
    info("Probably initialized allegro addon: font.");

    ret = al_init_ttf_addon();
    if(ret == false)
        fatal_error("Failed to initialize allegro addon: ttf.");
    else
        info("Initialized allegro addon: ttf.");

    g_font = al_load_font("DejaVuSans-Bold.ttf", 14, 0);
    if(g_font == NULL)
        fatal_error("failed to load font");
    else
        info("Loaded font");

    ret = al_init_native_dialog_addon();
    if(ret == false)
        fatal_error("Failed to initialize allegro addon: native dialogs.");
    else
        info("Initialized allegro addon: native dialogs.");

    display_x = 720;
    display_y = 480;

    al_set_new_display_flags(ALLEGRO_OPENGL | ALLEGRO_WINDOWED);

    display = al_create_display(display_x, display_y);

    if(display == NULL)
        fatal_error("Failed to create display.");
    else
        info("Created display.");

    ret = al_install_keyboard();
    if(ret == false)
        fatal_error("Failed to initialize keyboard.");
    else
        info("Initialized keyboard.");

    ret = al_install_mouse();
    if(ret == false)
        fatal_error("Failed to initialize mouse.");
    else
        info("Initialized mouse.");

    event_queue = al_create_event_queue();
    if(event_queue == NULL)
        fatal_error("Failed to create event queue.");
    else
        info("Created event queue.");

    timer = al_create_timer(1.0 / 30.0);
    if(timer == NULL)
        fatal_error("Error: failed to create timer.");
    else
        info("Created timer.");

    al_start_timer(timer);

    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_register_event_source(event_queue, al_get_keyboard_event_source());
}

Hex *addHex(float x, float y, float a, int level, int index) {
    float r  = 0.5 * sqrt(3) * a;
    float sx = 2 * r;
    float sy = 2 * r;

    float x_off = a * sin(1.0/4.0);
    float y_off = int(x) % 2 == 0 ? -r : 0;

    Hex *h = new Hex(x * (sx - x_off),
                     y * sy - y_off,
                     a,
                     level,
                     index);

    Map_UI->addWidget(h);
    map->m_hexes.push_back(h);
    return h;
}

static void is_won(void) {
    int red = 0;
    int blue = 0;
    for(auto&& h : map->m_hexes) {
        if(h->alive() && h->m_units_free + h->m_units_moved >= 1) {
            if(h->m_side == Side::Red) red++;
            else if(h->m_side == Side::Blue) blue++;
        }
    }
    if(blue == 0)
        Map_UI->m_game_won = true;
    if(red == 0)
        Map_UI->m_game_lost = true;
}

static void end_turn_cb(void) {
    Map_UI->m_turn_anim = 1.0;
    
    is_won();

    SideController *s = game->get_next_controller();

    map->harvest();
    map->free_units();
    map->clear_old_states();
    clear_active_hex();

    sideinfo1->set_offsets();
    sideinfo2->set_offsets();

    msg->add("It's %s's turn", s->m_name);

    if(s->is_AI() == true) {
        Map_UI->ai_play(s->do_AI());
    }
}

static void build_harvester_cb(void) {
    if(game->controller_has_resources(10)) {
        clear_active_hex();

        vector<Hex *> mark;
        for(auto&& h : map->m_hexes)
            if(h->alive() && h->m_side == game->controller()->m_side &&
               h->m_contains_harvester == false)
                mark.push_back(h);
        Map_UI->mark(mark);

        msg->add("Select hex to build on.");
        Map_UI->set_current_action(MapAction::BuildHarvester);
    }
    else {
        msg->add("Insufficient resources (10)");
        clear_opt_buttons();
    }
}
static void destroy_harvester_cb(void) {
    clear_active_hex();

    vector<Hex *> mark;
    for(auto&& h : map->m_hexes)
        if(h->alive() && h->m_side == game->controller()->m_side &&
           h->m_contains_harvester == true)
            mark.push_back(h);
    Map_UI->mark(mark);

    Map_UI->set_current_action(MapAction::DestroyHarvester);
}
static void build_armory_cb(void) {
    if(game->controller_has_resources(35)) {
        clear_active_hex();

        vector<Hex *> mark;
        for(auto&& h : map->m_hexes)
            if(h->alive() && h->m_side == game->controller()->m_side &&
               h->m_contains_armory == false)
                mark.push_back(h);
        Map_UI->mark(mark);

        msg->add("Select hex to build on.");
        Map_UI->set_current_action(MapAction::BuildArmory);
    } else {
        msg->add("Insufficient resources (35)");
        clear_opt_buttons();
    }
}
static void build_carrier_cb(void) {
    if(game->controller_has_resources(50)) {
        map->store_current_state();
        game->controller_pay(50);
        game->controller()->m_carriers += 1;
        clear_active_hex();
        msg->add("Built a carrier.");
        Map_UI->set_current_action(MapAction::MovingUnits);
    } else {
        msg->add("Insufficient resources (50)");
        clear_opt_buttons();
    }
}
static void build_walker_cb(void) {
    if(game->controller_has_resources(8)) {
        clear_active_hex();

        vector<Hex *> mark;
        for(auto&& h : map->m_hexes)
            if(h->alive() && h->m_side == game->controller()->m_side &&
               h->m_contains_armory == true)
                mark.push_back(h);
        Map_UI->mark(mark);

        msg->add("Select hex to build on.");
        Map_UI->set_current_action(MapAction::BuildWalker);
    } else {
        msg->add("Insufficient resources (min 8)");
        clear_opt_buttons();
    }
}
static void build_cannon_cb(void) {
    if(game->controller_has_resources(30)) {
        clear_active_hex();

        vector<Hex *> mark;
        for(auto&& h : map->m_hexes)
            if(h->alive() && h->m_side == game->controller()->m_side &&
               h->m_contains_cannon == false)
                mark.push_back(h);
        Map_UI->mark(mark);

        msg->add("Select hex to build on.");
        Map_UI->set_current_action(MapAction::BuildCannon);
    } else {
        msg->add("Insufficient resources (30)");
        clear_opt_buttons();
    }
}
static void build_cannon_ammo_cb(void) {
    if(game->controller_has_resources(20)) {
        clear_active_hex();

        vector<Hex *> mark;
        for(auto&& h : map->m_hexes)
            if(h->alive() && h->m_side == game->controller()->m_side &&
               h->m_contains_cannon == true)
                mark.push_back(h);
        Map_UI->mark(mark);

        msg->add("Select cannon to add ammo to.");
        Map_UI->set_current_action(MapAction::AddAmmoToCannon);
    } else {
        msg->add("Insufficient resources (20)");
        clear_opt_buttons();
    }
}
static void fire_cannon_cb(void) {
    Hex *act = map->get_active_hex();
    if(act == NULL) {
        msg->add("Select the cannon first.");
        Map_UI->set_current_action(MapAction::MovingUnits);
        clear_opt_buttons();
        return;
    }
    if(act->m_contains_cannon == false) {
        msg->add("That hex doesn't have a cannon.");
        Map_UI->set_current_action(MapAction::MovingUnits);
        clear_opt_buttons();
        return;
    }
    if(act->m_ammo == false) {
        msg->add("That cannon doesn't have any ammo loaded.");
        Map_UI->set_current_action(MapAction::MovingUnits);
        clear_opt_buttons();
        return;
    }
    if(act->m_side != get_current_side()) {
        fatal_error("oy! that's not your cannon!");
    }

    Map_UI->clear_mark();
    vector<Hex *> hexes_in_range;
    for(auto&& h : map->m_hexes) {
        float dist = map->hex_distance(act, h);
        if(dist > (0.4 + 2 * map->m_cannon_min_range) * act->m_circle_bb_radius and dist < (0.4 + 2 * map->m_cannon_max_range) * act->m_circle_bb_radius) {
            hexes_in_range.push_back(h);
        }
    }
    Map_UI->mark(hexes_in_range);

    msg->add("Select a target.");
    Map_UI->set_current_action(MapAction::FireCannon);
}
static void map_undo_cb(void) {
    map->undo();
}

static void init_buttons(void) {
    SideButton *end_turn = new SideButton("Turn");
    end_turn->m_x1 = display_x - 70;
    end_turn->m_y1 = display_y - 50;
    end_turn->m_x2 = display_x;
    end_turn->m_y2 = display_y;
    end_turn->onMouseDown = end_turn_cb;
    end_turn->set_offsets();
    Map_UI->addWidget(end_turn);

    SideButton *undo = new SideButton("Undo");
    undo->m_x1 = display_x - 70;
    undo->m_y1 = display_y - 105;
    undo->m_x2 = display_x;
    undo->m_y2 = display_y - 55;
    undo->onMouseDown = map_undo_cb;
    undo->set_offsets();
    Map_UI->addWidget(undo);

    int btn_startx = 0;
    int btn_starty = 0;
    int btn_sx = 60;
    int btn_sy = 40;
    int spc_y = 5;

    SideButton *button_build_harvester = new SideButton("Harv +");
    button_build_harvester->m_x1 = btn_startx;
    button_build_harvester->m_y1 = btn_starty;
    button_build_harvester->m_x2 = btn_sx;
    button_build_harvester->m_y2 = btn_sy;
    button_build_harvester->onMouseDown = build_harvester_cb;
    button_build_harvester->set_offsets();
    Map_UI->addWidget(button_build_harvester);

    SideButton *button_destroy_harvester = new SideButton("Harv -");
    button_destroy_harvester->m_x1 = btn_startx;
    button_destroy_harvester->m_y1 = btn_starty + 1 * (btn_sy + spc_y);
    button_destroy_harvester->m_x2 = btn_sx;
    button_destroy_harvester->m_y2 = btn_starty + 1 * (btn_sy + spc_y) + btn_sy;
    button_destroy_harvester->onMouseDown = destroy_harvester_cb;
    button_destroy_harvester->set_offsets();
    Map_UI->addWidget(button_destroy_harvester);

    SideButton *button_build_cannon = new SideButton("Cann");
    button_build_cannon->m_x1 = btn_startx;
    button_build_cannon->m_y1 = btn_starty + 2 * (btn_sy + spc_y);
    button_build_cannon->m_x2 = btn_sx;
    button_build_cannon->m_y2 = btn_starty + 2 * (btn_sy + spc_y) + btn_sy;
    button_build_cannon->onMouseDown = build_cannon_cb;
    button_build_cannon->set_offsets();
    Map_UI->addWidget(button_build_cannon);

    SideButton *button_build_cannon_ammo = new SideButton("Ammo");
    button_build_cannon_ammo->m_x1 = btn_startx;
    button_build_cannon_ammo->m_y1 = btn_starty + 3 * (btn_sy + spc_y);
    button_build_cannon_ammo->m_x2 = btn_sx;
    button_build_cannon_ammo->m_y2 = btn_starty + 3 * (btn_sy + spc_y) + btn_sy;
    button_build_cannon_ammo->onMouseDown = build_cannon_ammo_cb;
    button_build_cannon_ammo->set_offsets();
    Map_UI->addWidget(button_build_cannon_ammo);

    SideButton *button_fire_cannon = new SideButton("Fire");
    button_fire_cannon->m_x1 = btn_startx;
    button_fire_cannon->m_y1 = btn_starty + 4 * (btn_sy + spc_y);
    button_fire_cannon->m_x2 = btn_sx;
    button_fire_cannon->m_y2 = btn_starty + 4 * (btn_sy + spc_y) + btn_sy;
    button_fire_cannon->onMouseDown = fire_cannon_cb;
    button_fire_cannon->set_offsets();
    Map_UI->addWidget(button_fire_cannon);

    SideButton *button_build_armory = new SideButton("Armo");
    button_build_armory->m_x1 = btn_startx;
    button_build_armory->m_y1 = btn_starty + 5 * (btn_sy + spc_y);
    button_build_armory->m_x2 = btn_sx;
    button_build_armory->m_y2 = btn_starty + 5 * (btn_sy + spc_y) + btn_sy;
    button_build_armory->onMouseDown = build_armory_cb;
    button_build_armory->set_offsets();
    Map_UI->addWidget(button_build_armory);

    SideButton *button_build_walker = new SideButton("Walk");
    button_build_walker->m_x1 = btn_startx;
    button_build_walker->m_y1 = btn_starty + 6 * (btn_sy + spc_y);
    button_build_walker->m_x2 = btn_sx;
    button_build_walker->m_y2 = btn_starty + 6 * (btn_sy + spc_y) + btn_sy;
    button_build_walker->onMouseDown = build_walker_cb;
    button_build_walker->set_offsets();
    Map_UI->addWidget(button_build_walker);

    SideButton *button_build_carrier = new SideButton("Carr");
    button_build_carrier->m_x1 = btn_startx;
    button_build_carrier->m_y1 = btn_starty + 7 * (btn_sy + spc_y);
    button_build_carrier->m_x2 = btn_sx;
    button_build_carrier->m_y2 = btn_starty + 7 * (btn_sy + spc_y) + btn_sy;
    button_build_carrier->onMouseDown = build_carrier_cb;
    button_build_carrier->set_offsets();
    Map_UI->addWidget(button_build_carrier);

    opt_buttons = { button_build_harvester, button_destroy_harvester,
                    button_build_cannon, button_build_cannon_ammo,
                    button_fire_cannon, button_build_armory,
                    button_build_walker, button_build_carrier };
}

static void gen_map(int seed) {
    int i = 0;
    bool red = false;

    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 5; y++) {

            int level = 2 + (x * y) % max(3, min(seed + 1, 9));

            Hex *h = addHex(x, y, 50, level, i);
            i++;

            if(level == 3) {
                h->m_contains_harvester = true;
                h->m_contains_cannon = true;
                h->m_ammo = 3;
                h->m_side = Side::Red;
                h->m_units_free = 3;
                h->m_units_moved = 0;

                if(red == false) {
                    h->m_side = Side::Blue;
                    red = true;
                } else {
                    red = false;
                }
            }
        }
    }
}

static void init(void) {
    allegro_init();

    init_colors();
    MainMenu_UI = new MainMenuUI;
    GameSetup_UI = new GameSetupUI;

    Map_UI = NULL;
}

static void new_game() {
    game = new Game;
    game->players[0]->m_ai_control = false;

    if(strcmp(GameSetup_UI->m_selected_player->m_name, "vs. Human") == 0) {
        game->players[1]->m_ai_control = false;
    }

    map = new HexMap;
    Map_UI = new MapUI;
    Map_UI->clear_to = color_black;

    const char *map_name = GameSetup_UI->m_selected_map->m_name;
    int seed = 0;

    if(strcmp(map_name, "Europe") == 0) { seed = 1; }
    else if(strcmp(map_name, "North America") == 0) { seed = 2; }
    else if(strcmp(map_name, "Euroasia") == 0) { seed = 3; }
    else if(strcmp(map_name, "Australia") == 0) { seed = 4; }
    else if(strcmp(map_name, "Indochina") == 0) { seed = 5; }
    else if(strcmp(map_name, "Siberia") == 0) { seed = 6; }
    else if(strcmp(map_name, "Pacific Domains") == 0) { seed = 7; }
    else if(strcmp(map_name, "Africa") == 0) { seed = 8; }
    else if(strcmp(map_name, "South America") == 0) { seed = 9; }

    gen_map(seed);
    map->gen_neighbors();

    init_buttons();

    sideinfo1 = new SideInfo(game->players[0]);
    sideinfo1->setpos(75, 0, 125, 30);
    sideinfo1->set_offsets();

    sideinfo2 = new SideInfo(game->players[1]);
    sideinfo2->setpos(130, 0, 180, 30);
    sideinfo2->set_offsets();

    Map_UI->addWidgets({ sideinfo1, sideinfo2 });

    msg = new MessageLog;
    msg->add("It's %s's turn", game->controller()->m_name);

    Map_UI->addWidget(msg);
    ui = Map_UI;
}

static void delete_game(void) {
    assert(MainMenu_UI);
    ui = MainMenu_UI;

    delete map;
    delete Map_UI;
    delete game;
    Map_UI = NULL;
}

static bool is_opt_button(SideButton *b) {
    return find(opt_buttons.begin(), opt_buttons.end(), b) != opt_buttons.end();
}

static void clear_opt_buttons(void) {
    for(auto&& b : opt_buttons) {
        b->m_pressed = false;
    }
}

static void clear_active_hex(void) {
    for(auto &&h : map->m_hexes) {
        h->m_active = false;
    }
    Map_UI->clear_mark();
}

void register_global_key_callback(bool (*cb)(void)) {
    global_key_callback = cb;
}

static void pressEsc(void) {
    if(ui == MainMenu_UI && Map_UI != NULL) {
        ui = Map_UI;
    }
    else if(ui == Map_UI or ui == GameSetup_UI) {
        ui = MainMenu_UI;
    }
}

bool handle_global_keys(void) {
    if(key == ALLEGRO_KEY_ESCAPE) {
        pressEsc();
        return true;
    }
    if(ui == Map_UI) {
        int m = -1;
        if(key == ALLEGRO_KEY_1) m = 1;
        if(key == ALLEGRO_KEY_2) m = 2;
        if(key == ALLEGRO_KEY_3) m = 3;
        if(key == ALLEGRO_KEY_4) m = 4;
        if(key == ALLEGRO_KEY_5) m = 5;
        if(key == ALLEGRO_KEY_6) m = 6;
        if(key == ALLEGRO_KEY_7) m = 7;
        if(key == ALLEGRO_KEY_8) m = 8;
        if(m != -1)
            {
                if(Map_UI->get_current_action() == MapAction::MovingUnits) {
                    msg->add("Moving %d units.", m);
                    map->m_moving_units = m;
                }
                else if(Map_UI->get_current_action() == MapAction::BuildWalker) {
                    msg->add("Buying %d units.", m);
                    map->m_buying_units = m;
                }
            }
    }
    return true;
}

void mainloop() {
    bool redraw = true;
    bool was_mouse_down = false;

    register_global_key_callback(handle_global_keys);

    ALLEGRO_EVENT ev;

    running = true;
    ui = MainMenu_UI;

    // main loop
    while(running) {
        double frame_start;
        double frame_end;
        bool draw_hover = false;
        frame_start = al_current_time();

        al_get_mouse_state(&mouse_state);
        al_get_keyboard_state(&keyboard_state);

        mouse_x = mouse_state.x;
        mouse_y = mouse_state.y;

        // 1 - RMB
        // 2 - LMB
        // 4 - wheel
        mouse_button = mouse_state.buttons;

        if (mouse_state.buttons != 0) {
            if (!was_mouse_down) {
                // mouse button down event
                ui->mouseDownEvent();
                was_mouse_down = true;
            }
        }
        else if (was_mouse_down) {
            ui->mouseUpEvent();
            // mouse up event
            was_mouse_down = false;
        } else {
            draw_hover = true;
            // hover event
        }

        al_wait_for_event(event_queue, &ev);

        if(ev.type == ALLEGRO_EVENT_KEY_DOWN) {
            key = ev.keyboard.keycode;
            ui->keyDownEvent();
        }
        else if(ev.type == ALLEGRO_EVENT_TIMER) {
            { // logic goes here
                ui->update();
            }
            redraw = true;
        }
        else if(ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            running = false;
            break;
        }

        if(redraw && al_is_event_queue_empty(event_queue)) {
            redraw = false;
            al_clear_to_color(ui->clear_to);

            { // drawing goes here
                ui->draw();
                if(draw_hover == true)
                    ui->hoverOverEvent();
            }
            al_flip_display();
        }

        frame_end = al_current_time();
        dt = frame_end - frame_start;
    }
}

static void deinit(void) {
    if(Map_UI != NULL)
        delete_game();
    delete GameSetup_UI;
    delete MainMenu_UI;
}

int main() {
    init();
    mainloop();
    deinit();

    return 0;
}
