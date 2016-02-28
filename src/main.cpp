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

#include <dirent.h>

#include "./util.h"

using namespace std;

struct HexMap;
struct Game;
struct SideFlag;
struct SideButton; // TODO remove
struct MessageLog;
struct SideInfo;

struct UI;
struct MainMenuUI;
struct GameSetupUI;
struct MapEditorUI;
struct MapUI;

ALLEGRO_DISPLAY *display;
ALLEGRO_KEYBOARD_STATE keyboard_state;
ALLEGRO_MOUSE_STATE mouse_state;
ALLEGRO_EVENT_QUEUE *event_queue;
ALLEGRO_TIMER *timer;
ALLEGRO_FONT *g_font;

constexpr int font_height = 16;
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
MapEditorUI *MapEditor_UI;
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
    return (x - view_x) * scale;
}
inline float vy(float y) {
    return (y - view_y) * scale;
}

enum class GameType {
    Game,
    Editor,
};

enum class WidgetType {
    Hex,
    Button,
    SideButton, // remove
    MenuEntry,
    MapSelectButton, // remove
    Other,
};

enum class Side {
    Red,
    Blue,
    Green,
    Yellow,
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

enum class MapEditorAction {
    AddHealth,
    RemoveHealth,
    ToggleHarvester,
    ToggleArmory,
    ToggleCannon,
    ToggleCannonAmmo,
    AddUnit,
    RemoveUnit,
    PaintNeutral,
    PaintRed,
    PaintBlue,
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

    al_draw_filled_rectangle(0, display_y - font_height - 10, txtlen + 10, display_y, color_black);

    al_draw_text(g_font, color_white, 5, display_y - font_height - 5, 0, txt);
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
    bool m_pressed;
    ALLEGRO_COLOR m_color = color_red;

    explicit Button(const char *_name) {
        m_name = _name;
        m_type = WidgetType::Button;
        m_x_off = 0;
        m_y_off = 0;
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
        m_y_off = round((m_y2 - m_y1 - font_height) / 2);
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
        m_y_off = round((m_y2 - m_y1 - font_height) / 2);
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

    void save(ostream &os);
    void load(istream &is, bool prune);
    void prune(void);

    float hex_distance(Hex *h1, Hex *h2);
    void gen_neighbors(void);
    vector<Hex *> neighbors(Hex *base);
    bool is_neighbor(Hex *h1, Hex *h2);
    void harvest(void);
    void build_harvester(Hex *h);
    void destroy_harvester(Hex *h);
    void build_cannon(Hex *h);
    void build_armory(Hex *h);
    void add_cannon_ammo(Hex *h);
    void fire_cannon(Hex *from, Hex *to);
    bool cannon_in_range(Hex *from, Hex *to);
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
    SideController *m_current_controller;

    Game() {
        players.push_back(new SideController(Side::Red));
        players.push_back(new SideController(Side::Blue));
        m_current_controller = players[0];
    }
    ~Game() {
        for(auto&& player : players) delete player;
    }

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
        m_y_off = round((m_y2 - m_y1 - font_height) / 2);
    }
};

static bool marked_hexes(void);
static inline void draw_hex(float x, float y, float a, float zrot, float cr, float cg, float cb, float r, float d);

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
    bool m_ammo;
    bool m_loaded_ammo;

    int m_units_free;
    int m_units_moved;

    Side m_side;

    void save(ostream &os) {
        os << m_index << ' ' << m_level << ' ' << m_a << ' '
           << m_r << ' ' << m_active << ' ' << m_marked << ' '
           << m_cx << ' ' << m_cy << ' ' << m_contains_harvester << ' '
           << m_harvested << ' ' << m_contains_armory << ' '
           << m_contains_cannon << ' ' << m_ammo << ' '
           << m_loaded_ammo << ' ' << m_units_free << ' '
           << m_units_moved << ' ' << (int)m_side;
    }

    void load(istream &is) {
        int _side;
        is >> m_index >> m_level >> m_a
           >> m_r >> m_active >> m_marked
           >> m_cx >> m_cy >> m_contains_harvester
           >> m_harvested >> m_contains_armory
           >> m_contains_cannon >> m_ammo
           >> m_loaded_ammo >> m_units_free
           >> m_units_moved >> _side;
        m_side = (Side)_side;
    }

    Hex() {}
    Hex(float x1, float y1, float a, int level, int index) {
        m_index = index;

        // radius of inscribed circle
        float r = 0.5 * sqrt(3) * a;

        m_x1 = x1;
        m_y1 = y1;
        m_a = a;
        m_r = r;
        m_cx = m_x1 + r;
        m_cy = m_y1 + r;
        m_x2 = m_x1 + 2 * r;
        m_y2 = m_y1 + 2 * r;
        m_active = false;
        m_marked = false;

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

        InitWidget();
    }

    void InitWidget() {
        m_circle_bb = true;
        m_circle_bb_radius = m_r;
        m_type = WidgetType::Hex;
        m_x1 = m_cx - m_r;
        m_y1 = m_cy - m_r;
        m_x2 = m_cx - 2 * m_r;
        m_y2 = m_cy - 2 * m_r;
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

    inline void draw_text(float x, float y, ALLEGRO_COLOR& txt_color) {
        al_draw_textf(g_font,
                      txt_color,
                      x - 5,
                      y - 7,
                      0,
                      "%d", m_level);

        if(m_contains_harvester == true)
            al_draw_text(g_font, txt_color,
                         x - 25, y - 25,
                         0, "H");

        if(m_contains_armory == true)
            al_draw_text(g_font, txt_color,
                         x - 5, y - 25,
                         0, "A");

        if(m_contains_cannon == true)
            al_draw_text(g_font, txt_color,
                         x + 15, y - 25,
                         0, "C");

        if(m_ammo or m_loaded_ammo)
            al_draw_text(g_font, txt_color,
                         x + 15, y - 7,
                          0, "1");

        if(m_units_free > 0 or m_units_moved > 0)
            al_draw_textf(g_font, txt_color,
                          x - 12, y + 10,
                          0, "%d/%d", m_units_free, m_units_moved);
    }

    void draw(void) override {
        if(m_level == -1) return;

        float r, g, b;

        if(m_side == Side::Red) {
            if(m_active == true) { r = 0.98; g = 0.2; b = 0.2; }
            else { r = 0.94; g = 0.5; b = 0.5; }
        }
        else if(m_side == Side::Blue) {
            if(m_active == true) { r = 0.2; g = 0.2; b = 0.98; }
            else { r = 0.5; g = 0.5; b = 0.94; }
        }
        else {
            r = 0.5; g = 0.5; b = 0.5;
        }

        ALLEGRO_COLOR txt_color = color_white;

        const float x = vx(m_cx);
        const float y = vy(m_cy);
        constexpr float sin12 = sin(1.0f / 2.0f);
        constexpr float sqrt3 = sqrt(3);

        if(m_marked == true) {
            const float space = 2;
            draw_hex(x, y,
                     scale * (m_a - space), 0, 1, 1, 1,
                     0.5 * sqrt3 * (m_a - space) * scale,
                     sin12 * (m_a - space) * scale);
        }
        else if(m_active == false and marked_hexes() == true) {
            r /= 3;
            g /= 3;
            b /= 3;
            txt_color = color_grey_middle;
        }

        const float space = 4;

        draw_hex(x, y, scale * (m_a - space), 0, r, g, b,
                 0.5 * sqrt3 * (m_a - space) * scale,
                 sin12 * (m_a - space) * scale);

        if(m_level == 0) return;

        draw_text(x, y, txt_color);
    }

    void draw_editor(void) {
        float r, g, b;

        if(m_side == Side::Red) {
            r = 0.94; g = 0.5; b = 0.5;
        }
        else if(m_side == Side::Blue) {
            r = 0.5; g = 0.5; b = 0.94;
        }
        else {
            r = 0.5; g = 0.5; b = 0.5;
        }

        if(m_level <= 0) {
            r /= 3;
            g /= 3;
            b /= 3;
        }

        const int x = vx(m_cx);
        const int y = vy(m_cy);
        constexpr float sin12 = sin(1.0f / 2.0f);
        constexpr float sqrt3 = sqrt(3);

        ALLEGRO_COLOR txt_color = color_white;

        const float space = 4;

        draw_hex(x, y, scale * (m_a - space), 0, r, g, b,
                 0.5 * sqrt3 * (m_a - space) * scale,
                 sin12 * (m_a - space) * scale);

        draw_text(x, y, txt_color);
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
    debug("SideController::do_AI(): number of ai actions: %d", ret.size());
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
        if(hs.empty() == true) {
            m_marked_hexes = false;
            return;
        }
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

struct MapEditorUI : UI {
    MapEditorAction m_current_action;
    Side m_current_side;

    ~MapEditorUI() {
        for(auto&& w : widgets) delete w;
    }

    void draw(void) override;
    void update(void) override;

    void set_current_action(MapEditorAction act) {
        m_current_action = act;
    }
    void mouseDownEvent(void);
    MapEditorAction get_current_action(void) {
        return m_current_action;
    }
    void MapHexSelected(Hex *h);
};

void MapEditorUI::draw(void) {
    // draw normal hexes
    for(auto&& h : map->m_hexes) {
        h->draw_editor();
    }

    for(auto&& w : widgets) {
        if(w->m_type != WidgetType::Hex) {
            w->draw();
        }
    }
}

void MapEditorUI::update(void) {
    int d = 400 * dt;

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

void MapEditorUI::mouseDownEvent(void) {
    if(mouse_button == 2) {
        msg->hide();
        set_current_action(MapEditorAction::AddHealth);
        clear_active_hex();
    } else {
        UI::mouseDownEvent();
    }
}

void MapEditorUI::MapHexSelected(Hex *h) {
    if(get_current_action() == MapEditorAction::AddHealth) {
        h->m_level++;
    }
    else if(get_current_action() == MapEditorAction::RemoveHealth) {
        h->m_level--;
    }
    else if(get_current_action() == MapEditorAction::ToggleHarvester) {
        h->m_contains_harvester = not h->m_contains_harvester;
    }
    else if(get_current_action() == MapEditorAction::ToggleCannon) {
        h->m_contains_cannon = not h->m_contains_cannon;
    }
    else if(get_current_action() == MapEditorAction::ToggleArmory) {
        h->m_contains_armory = not h->m_contains_armory;
    }
    else if(get_current_action() == MapEditorAction::ToggleCannonAmmo) {
        h->m_ammo = not h->m_ammo;
    }
    else if(get_current_action() == MapEditorAction::AddUnit) {
        h->m_units_free += 1;
    }
    else if(get_current_action() == MapEditorAction::RemoveUnit) {
        if(h->m_units_free > 0) {
            h->m_units_free -= 1;
        }
    }
    else if(get_current_action() == MapEditorAction::PaintNeutral) {
        h->m_side = Side::Neutral;
    }
    else if(get_current_action() == MapEditorAction::PaintRed) {
        h->m_side = Side::Red;
    }
    else if(get_current_action() == MapEditorAction::PaintBlue) {
        h->m_side = Side::Blue;
    } else {
        fatal_error("MapEditorUI::MapHexSelected(): Unknown map editor action");
    }
}

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

    /* should these have checks? */
    if(a.m_act == MapAction::MovingUnits) {
        map->m_moving_units = a.m_amount;
        map->move_or_attack(a.m_src, a.m_dst);
    }
    else if(a.m_act == MapAction::BuildHarvester) {
        map->build_harvester(a.m_src);
        game->controller_pay(10);
    }
    else if(a.m_act == MapAction::DestroyHarvester) {
        map->destroy_harvester(a.m_src);
    }
    else {
        fatal_error("MapUI::ai_replay(): not implemented yet");
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
        m_rot += dt * 30;
    }

    void handlePress(const char *name);
};

struct GameSetupUI : UI {
    Button *m_selected_map;
    Button *m_selected_player;

    vector<Button *> m_player_select_btns;
    vector<Button *> m_map_select_btns;

    GameSetupUI();
    ~GameSetupUI() {
        for(auto&& w: m_map_select_btns) free((char*)w->m_name);
        for(auto&& w: widgets) delete w;
    }
};

static void new_game(GameType t);
static void btn_begin_cb(void);
static void btn_editor_cb(void);
static void btn_map_select_cb(void);
static void btn_player_select_cb(void);

GameSetupUI::GameSetupUI() {
    DIR *dpdf;
    vector<string> map_filenames;
    dpdf = opendir("./maps/");
    if (dpdf != NULL){
        string suffix = ".map";
        struct dirent *epdf;
        while ((epdf = readdir(dpdf))) {
            string filename = string(epdf->d_name);
            if(filename.size() <= suffix.size())
                continue;
            if(filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
                map_filenames.push_back(filename);
            }
        }
    } else {
        fatal_error("GameSetupUI::GameSetupUI(): Couldn't open ./maps: ", strerror(errno));
    }

    Button *btn_begin = new Button("Begin");
    btn_begin->setpos(display_x/2 - 40, display_y - 70,
                      display_x/2 + 40, display_y - 30);
    btn_begin->onMouseDown = btn_begin_cb;
    btn_begin->set_offsets();
    addWidget(btn_begin);

    Button *btn_editor = new Button("Editor");
    btn_editor->setpos(display_x/2 - 40, display_y - 115,
                       display_x/2 + 40, display_y - 75);
    btn_editor->onMouseDown = btn_editor_cb;
    btn_editor->set_offsets();
    addWidget(btn_editor);

    vector<Widget *> map_btns;
    int y = 45;

    for(auto&& map_name : map_filenames)
        {
            Button *btn_map = new Button(strdup(map_name.c_str()));
            int xsize = 10 + al_get_text_width(g_font, map_name.c_str());
            btn_map->setpos(150,  y,
                            150 + xsize, y + 30);
            btn_map->onMouseDown = btn_map_select_cb;
            btn_map->set_offsets();
            btn_map->m_type = WidgetType::MapSelectButton;
            map_btns.push_back(btn_map);
            m_map_select_btns.push_back(btn_map);

            y += 35;
        }

    addWidgets(map_btns);

    // press the first button
    Button *btn = static_cast<Button*>(map_btns.front());
    btn->m_pressed = true;
    btn->m_color = color_red_muted;
    m_selected_map = btn;

    y = 150;
    Button *btn_blue_player_ai = new Button("Blue.AI");
    int xsize = 10 + al_get_text_width(g_font, "Blue.AI");
    btn_blue_player_ai->setpos(display_x - 200 - xsize,  y,
                               display_x - 200, y + 30);
    btn_blue_player_ai->onMouseDown = btn_player_select_cb;
    btn_blue_player_ai->set_offsets();
    addWidget(btn_blue_player_ai);

    Button *btn_blue_player_human = new Button("Blue.human");
    xsize = 10 + al_get_text_width(g_font, "Blue.human");
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
    new_game(GameType::Game);
}

static void btn_editor_cb(void) {
    new_game(GameType::Editor);
}

static inline void draw_hex(float x, float y, float a, float zrot, float cr, float cg, float cb, float r, float d) {
    glPushMatrix();

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

    glPopMatrix();
}

void MainMenuUI::draw(void) {
    draw_hex(display_x/2, display_y/2, 150, m_rot, 0.8, 0.2, 0.2, 0.5*sqrt(3)*150, 150*sin(1.0/2.0));
    draw_hex(display_x/2, display_y/2, 100, -m_rot, 0.2, 0.2, 0.8, 0.5*sqrt(3)*100, 100*sin(1.0/2.0));

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
        m_y_off = round((m_y2 - m_y1 - font_height) / 2);
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
    } else {
        UI::mouseDownEvent();
    }
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
            bool hit = (not widget->m_visible) || is_hit(widget);

            if(hit == true) {
                debug("UI::mouseDownEvent(): hit!");
                widget->mouseDownEvent();
                if(widget->onMouseDown != nullptr) {
                    widget->onMouseDown();
                }
            }

            if(hit == true and widget->m_type == WidgetType::Hex) {
                Hex *h = static_cast<Hex *>(widget);
                if(ui == Map_UI)
                    Map_UI->MapHexSelected(h);
                else if(ui == MapEditor_UI)
                    MapEditor_UI->MapHexSelected(h);
                return;

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

void HexMap::build_harvester(Hex *h) {
    h->m_contains_harvester = true;
}

void HexMap::destroy_harvester(Hex *h) {
    for(auto&& n : map->neighbors(h)) {
        if(n->alive() == true) {
            n->harvest();
        }
    }
    h->harvest();
    h->m_contains_harvester = false;
}

void HexMap::build_cannon(Hex *h) {
    assert(h->m_contains_cannon == false);
    h->m_contains_cannon = true;
}

void HexMap::build_armory(Hex *h) {
    assert(h->m_contains_armory == false);
    h->m_contains_armory = true;
}

void HexMap::add_cannon_ammo(Hex *h) {
    assert(h->m_ammo == false && h->m_loaded_ammo == false);
    h->m_loaded_ammo = true;
}

void HexMap::fire_cannon(Hex *from, Hex *to) {
    from->m_ammo = false;
    to->m_level -= 1;
    to->destroy_units(8);
}

bool HexMap::cannon_in_range(Hex *from, Hex *to) {
    float dist = map->hex_distance(from, to);

    return dist < (0.4 + 2 * m_cannon_min_range) * from->m_circle_bb_radius or dist > (0.4 + 2 * m_cannon_max_range) * from->m_circle_bb_radius;
}

void HexMap::save(ostream &os) {
    os << m_hexes.size() << '\n';
    for(auto&& h : m_hexes) {
        h->save(os);
        os << '\n';
    }
}

void HexMap::load(istream &is, bool prune) {
    int size;
    is >> size;
    m_hexes.reserve(size);

    // try to make sure allocated hexes aren't fragmented
    int j = 0;
    Hex tmp;
    for(int i = 0; i < size; i++) {
        tmp.load(is);
        if(tmp.m_level >= 1 || prune == false) {
            tmp.InitWidget();
            tmp.m_index = j;
            j++;
            m_hexes.push_back(new Hex(tmp));
        }
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
        Hex *cur = q.front();
        q.pop_front();
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

    debug("HexMap::undo(): undo to %p", old_hexes);
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
        debug("HexMap::move_or_attack(): %p moves %d to %p", defender, moved, attacker);
    }
    else {
        // attacking
        int moved =
            min({
                    m_moving_units,
                    attacker->m_units_free,
                    m_max_units_moved
               });

        debug("HexMap::move_or_attack(): %p attacked %p with %d", defender, attacker, moved);
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
            if(h->m_loaded_ammo) {
                h->m_ammo = true;
            }
            h->m_loaded_ammo = false;
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

        if(map->cannon_in_range(prev, h) == false) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            msg->add("Target isn't in range.");
            return;
        }

        if(prev->m_ammo == false) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            msg->add("That cannon doesn't have any loaded ammo.");
            return;
        }

        msg->add("Firing!");
        map->store_current_state();
        map->fire_cannon(prev, h);
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildHarvester) {

        if(h->m_contains_harvester == false) {
            map->store_current_state();
            game->controller_pay(10);
            map->build_harvester(h);
        }

        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::DestroyHarvester) {
        if(h->m_contains_harvester == true) {
            map->store_current_state();
            map->destroy_harvester(h);
        }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::AddAmmoToCannon) {

        if(h->m_contains_cannon == true &&
           h->m_ammo == false &&
           h->m_loaded_ammo == false)
            {
                map->store_current_state();
                game->controller_pay(20);
                map->add_cannon_ammo(h);
            }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildArmory) {
        if(h->m_contains_armory == false) {
            map->store_current_state();
            game->controller_pay(35);
            map->build_armory(h);
        }
        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
    else if(get_current_action() == MapAction::BuildCannon) {
        if(h->m_contains_cannon == false) {
            map->store_current_state();
            game->controller_pay(30);
            map->build_cannon(h);
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

static void draw_game_over(bool m_game_lost) {
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
}

void MapUI::draw(void) {
    // draw normal hexes
    for(auto&& h : map->m_hexes) {
        h->draw();
    }

    if(m_game_won or m_game_lost) {
        draw_game_over(m_game_lost);
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

    int d = 400 * dt;

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

    g_font = al_load_font("DejaVuSans-Bold.ttf", font_height, 0);
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
    // display_x = 1280;
    // display_y = 720;

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

    if(mark.empty() == true) {
        msg->add("You don't have any harvesters.");
        clear_opt_buttons();
        return;
    }

    msg->add("Select harvester to destroy.");
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
    vector<Hex *> mark;
    for(auto&& h : map->m_hexes)
        if(h->alive() && h->m_side == game->controller()->m_side &&
           h->m_contains_armory == true)
            mark.push_back(h);

    if(mark.empty() == true) {
        msg->add("You don't have any armories.");
        clear_opt_buttons();
        return;
    }

    if(game->controller_has_resources(8)) {
        clear_active_hex();

        Map_UI->mark(mark);

        msg->add("Select hex to recruit on. (max: %d)",
                 int(floor(float(game->controller_resources()) / 8.0)));
        Map_UI->set_current_action(MapAction::BuildWalker);
    } else {
        msg->add("Insufficient resources (min: 8)");
        clear_opt_buttons();
    }
}
static void build_cannon_cb(void) {
    if(game->controller_has_resources(30)) {
        clear_active_hex();

        vector<Hex *> mark;
        for(auto&& h : map->m_hexes) {
            if(h->alive() && h->m_side == game->controller()->m_side &&
               h->m_contains_cannon == false) {
                mark.push_back(h);
            }
        }

        Map_UI->mark(mark);

        msg->add("Select hex to build on.");
        Map_UI->set_current_action(MapAction::BuildCannon);
    } else {
        msg->add("Insufficient resources (30)");
        clear_opt_buttons();
    }
}
static void build_cannon_ammo_cb(void) {
    vector<Hex *> mark;
    for(auto&& h : map->m_hexes) {
        if(h->alive() && h->m_side == game->controller()->m_side &&
           h->m_contains_cannon == true && h->m_ammo == false && h->m_loaded_ammo == false) {
            mark.push_back(h);
        }
    }

    if(mark.size() == 0) {
        msg->add("You don't have any cannons to load.");
        clear_opt_buttons();
        return;
    }

    if(game->controller_has_resources(20)) {
        clear_active_hex();

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
        fatal_error("fire_cannon_cb(): oy! that's not your cannon!");
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

void editor_add_health(void) {
    MapEditor_UI->set_current_action(MapEditorAction::AddHealth);
    msg->add("Action: add health.");
}
void editor_remove_health(void) {
    MapEditor_UI->set_current_action(MapEditorAction::RemoveHealth);
    msg->add("Action: remove health.");
}
void editor_toggle_harvester_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::ToggleHarvester);
    msg->add("Action: toggle harvester.");
}
void editor_toggle_armory_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::ToggleArmory);
    msg->add("Action: toggle armory.");
}
void editor_toggle_cannon_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::ToggleCannon);
    msg->add("Action: toggle cannon.");
}
void editor_toggle_cannon_ammo_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::ToggleCannonAmmo);
    msg->add("Action: toggle cannon ammo.");
}
void editor_add_unit_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::AddUnit);
    msg->add("Action: add unit.");
}
void editor_remove_unit_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::RemoveUnit);
    msg->add("Action: remove unit.");
}
void editor_paint_neutral_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::PaintNeutral);
    msg->add("Action: paint neutral.");
}
void editor_paint_red_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::PaintRed);
    msg->add("Action: paint red.");
}
void editor_paint_blue_cb(void) {
    MapEditor_UI->set_current_action(MapEditorAction::PaintBlue);
    msg->add("Action: paint blue.");
}
void editor_save_map_cb(void) {
    ofstream out("editing.map", ios::out);
    for(auto&& h : map->m_hexes) if(h->m_level == 0) h->m_level = -1;
    map->save(out);
    msg->add("Saved as ./editing.map. Move finished maps to ./maps/");
}

static void init_buttons() {
    /*
      Map buttons
     */
    SideButton *end_turn = new SideButton("Turn");
    end_turn->m_x1 = display_x - 70;
    end_turn->m_y1 = display_y - 50;
    end_turn->m_x2 = display_x;
    end_turn->m_y2 = display_y;
    end_turn->set_offsets();
    end_turn->onMouseDown = end_turn_cb;
    Map_UI->addWidget(end_turn);

    SideButton *undo = new SideButton("Undo");
    undo->m_x1 = display_x - 70;
    undo->m_y1 = display_y - 105;
    undo->m_x2 = display_x;
    undo->m_y2 = display_y - 55;
    undo->set_offsets();
    undo->onMouseDown = map_undo_cb;
    Map_UI->addWidget(undo);

    int btn_startx = 0;
    int btn_starty = 0;
    int btn_sx = 60;
    int btn_sy = 40;
    int spc_y = 5;

    SideButton *button_build_harvester = new SideButton("Harv+");
    button_build_harvester->m_x1 = btn_startx;
    button_build_harvester->m_y1 = btn_starty;
    button_build_harvester->m_x2 = btn_sx;
    button_build_harvester->m_y2 = btn_sy;
    button_build_harvester->set_offsets();
    button_build_harvester->onMouseDown = build_harvester_cb;
    Map_UI->addWidget(button_build_harvester);

    SideButton *button_destroy_harvester = new SideButton("Harv-");
    button_destroy_harvester->m_x1 = btn_startx;
    button_destroy_harvester->m_y1 = btn_starty + 1 * (btn_sy + spc_y);
    button_destroy_harvester->m_x2 = btn_sx;
    button_destroy_harvester->m_y2 = btn_starty + 1 * (btn_sy + spc_y) + btn_sy;
    button_destroy_harvester->set_offsets();
    button_destroy_harvester->onMouseDown = destroy_harvester_cb;
    Map_UI->addWidget(button_destroy_harvester);

    SideButton *button_build_cannon = new SideButton("Cann");
    button_build_cannon->m_x1 = btn_startx;
    button_build_cannon->m_y1 = btn_starty + 2 * (btn_sy + spc_y);
    button_build_cannon->m_x2 = btn_sx;
    button_build_cannon->m_y2 = btn_starty + 2 * (btn_sy + spc_y) + btn_sy;
    button_build_cannon->set_offsets();
    button_build_cannon->onMouseDown = build_cannon_cb;
    Map_UI->addWidget(button_build_cannon);

    SideButton *button_build_cannon_ammo = new SideButton("Ammo");
    button_build_cannon_ammo->m_x1 = btn_startx;
    button_build_cannon_ammo->m_y1 = btn_starty + 3 * (btn_sy + spc_y);
    button_build_cannon_ammo->m_x2 = btn_sx;
    button_build_cannon_ammo->m_y2 = btn_starty + 3 * (btn_sy + spc_y) + btn_sy;
    button_build_cannon_ammo->set_offsets();
    button_build_cannon_ammo->onMouseDown = build_cannon_ammo_cb;
    Map_UI->addWidget(button_build_cannon_ammo);

    SideButton *button_fire_cannon = new SideButton("Fire");
    button_fire_cannon->m_x1 = btn_startx;
    button_fire_cannon->m_y1 = btn_starty + 4 * (btn_sy + spc_y);
    button_fire_cannon->m_x2 = btn_sx;
    button_fire_cannon->m_y2 = btn_starty + 4 * (btn_sy + spc_y) + btn_sy;
    button_fire_cannon->set_offsets();
    button_fire_cannon->onMouseDown = fire_cannon_cb;
    Map_UI->addWidget(button_fire_cannon);

    SideButton *button_build_armory = new SideButton("Armo");
    button_build_armory->m_x1 = btn_startx;
    button_build_armory->m_y1 = btn_starty + 5 * (btn_sy + spc_y);
    button_build_armory->m_x2 = btn_sx;
    button_build_armory->m_y2 = btn_starty + 5 * (btn_sy + spc_y) + btn_sy;
    button_build_armory->set_offsets();
    button_build_armory->onMouseDown = build_armory_cb;
    Map_UI->addWidget(button_build_armory);

    SideButton *button_build_walker = new SideButton("Walk");
    button_build_walker->m_x1 = btn_startx;
    button_build_walker->m_y1 = btn_starty + 6 * (btn_sy + spc_y);
    button_build_walker->m_x2 = btn_sx;
    button_build_walker->m_y2 = btn_starty + 6 * (btn_sy + spc_y) + btn_sy;
    button_build_walker->set_offsets();
    button_build_walker->onMouseDown = build_walker_cb;
    Map_UI->addWidget(button_build_walker);

    SideButton *button_build_carrier = new SideButton("Carr");
    button_build_carrier->m_x1 = btn_startx;
    button_build_carrier->m_y1 = btn_starty + 7 * (btn_sy + spc_y);
    button_build_carrier->m_x2 = btn_sx;
    button_build_carrier->m_y2 = btn_starty + 7 * (btn_sy + spc_y) + btn_sy;
    button_build_carrier->set_offsets();
    button_build_carrier->onMouseDown = build_carrier_cb;
    Map_UI->addWidget(button_build_carrier);

    opt_buttons = { button_build_harvester, button_destroy_harvester,
                    button_build_cannon, button_build_cannon_ammo,
                    button_fire_cannon, button_build_armory,
                    button_build_walker, button_build_carrier };

    /*
      Editor buttons
     */
    btn_startx = 0;
    btn_starty = 0;
    btn_sx = 60;
    btn_sy = 40;
    spc_y = 5;

    SideButton *button_add_health = new SideButton("He +");
    button_add_health->m_x1 = btn_startx;
    button_add_health->m_y1 = btn_starty;
    button_add_health->m_x2 = btn_sx;
    button_add_health->m_y2 = btn_sy;
    button_add_health->set_offsets();
    button_add_health->onMouseDown = editor_add_health;
    MapEditor_UI->addWidget(button_add_health);

    SideButton *button_remove_health = new SideButton("He -");
    button_remove_health->m_x1 = btn_startx;
    button_remove_health->m_y1 = btn_starty + 1 * (btn_sy + spc_y);
    button_remove_health->m_x2 = btn_sx;
    button_remove_health->m_y2 = btn_starty + 1 * (btn_sy + spc_y) + btn_sy;
    button_remove_health->set_offsets();
    button_remove_health->onMouseDown = editor_remove_health;
    MapEditor_UI->addWidget(button_remove_health);

    SideButton *button_toggle_harvester = new SideButton("T H");
    button_toggle_harvester->m_x1 = btn_startx;
    button_toggle_harvester->m_y1 = btn_starty + 2 * (btn_sy + spc_y);
    button_toggle_harvester->m_x2 = btn_sx;
    button_toggle_harvester->m_y2 = btn_starty + 2 * (btn_sy + spc_y) + btn_sy;
    button_toggle_harvester->set_offsets();
    button_toggle_harvester->onMouseDown = editor_toggle_harvester_cb;
    MapEditor_UI->addWidget(button_toggle_harvester);

    SideButton *button_toggle_armory = new SideButton("T A");
    button_toggle_armory->m_x1 = btn_startx;
    button_toggle_armory->m_y1 = btn_starty + 3 * (btn_sy + spc_y);
    button_toggle_armory->m_x2 = btn_sx;
    button_toggle_armory->m_y2 = btn_starty + 3 * (btn_sy + spc_y) + btn_sy;
    button_toggle_armory->set_offsets();
    button_toggle_armory->onMouseDown = editor_toggle_armory_cb;
    MapEditor_UI->addWidget(button_toggle_armory);

    SideButton *button_toggle_cannon = new SideButton("T C");
    button_toggle_cannon->m_x1 = btn_startx;
    button_toggle_cannon->m_y1 = btn_starty + 4 * (btn_sy + spc_y);
    button_toggle_cannon->m_x2 = btn_sx;
    button_toggle_cannon->m_y2 = btn_starty + 4 * (btn_sy + spc_y) + btn_sy;
    button_toggle_cannon->set_offsets();
    button_toggle_cannon->onMouseDown = editor_toggle_cannon_cb;
    MapEditor_UI->addWidget(button_toggle_cannon);

    SideButton *button_toggle_cannon_ammo = new SideButton("T CA");
    button_toggle_cannon_ammo->m_x1 = btn_startx;
    button_toggle_cannon_ammo->m_y1 = btn_starty + 5 * (btn_sy + spc_y);
    button_toggle_cannon_ammo->m_x2 = btn_sx;
    button_toggle_cannon_ammo->m_y2 = btn_starty + 5 * (btn_sy + spc_y) + btn_sy;
    button_toggle_cannon_ammo->set_offsets();
    button_toggle_cannon_ammo->onMouseDown = editor_toggle_cannon_ammo_cb;
    MapEditor_UI->addWidget(button_toggle_cannon_ammo);

    SideButton *button_add_unit = new SideButton("U +");
    button_add_unit->m_x1 = btn_startx;
    button_add_unit->m_y1 = btn_starty + 6 * (btn_sy + spc_y);
    button_add_unit->m_x2 = btn_sx;
    button_add_unit->m_y2 = btn_starty + 6 * (btn_sy + spc_y) + btn_sy;
    button_add_unit->set_offsets();
    button_add_unit->onMouseDown = editor_add_unit_cb;
    MapEditor_UI->addWidget(button_add_unit);

    SideButton *button_remove_unit = new SideButton("U -");
    button_remove_unit->m_x1 = btn_startx;
    button_remove_unit->m_y1 = btn_starty + 7 * (btn_sy + spc_y);
    button_remove_unit->m_x2 = btn_sx;
    button_remove_unit->m_y2 = btn_starty + 7 * (btn_sy + spc_y) + btn_sy;
    button_remove_unit->set_offsets();
    button_remove_unit->onMouseDown = editor_remove_unit_cb;
    MapEditor_UI->addWidget(button_remove_unit);

    btn_startx = 65;
    btn_sx = 125;

    SideButton *button_paint_neutral = new SideButton("Neu");
    button_paint_neutral->m_x1 = btn_startx;
    button_paint_neutral->m_y1 = btn_starty + 0 * (btn_sy + spc_y);
    button_paint_neutral->m_x2 = btn_sx;
    button_paint_neutral->m_y2 = btn_starty + 0 * (btn_sy + spc_y) + btn_sy;
    button_paint_neutral->set_offsets();
    button_paint_neutral->onMouseDown = editor_paint_neutral_cb;
    MapEditor_UI->addWidget(button_paint_neutral);

    SideButton *button_paint_red = new SideButton("Red");
    button_paint_red->m_x1 = btn_startx;
    button_paint_red->m_y1 = btn_starty + 1 * (btn_sy + spc_y);
    button_paint_red->m_x2 = btn_sx;
    button_paint_red->m_y2 = btn_starty + 1 * (btn_sy + spc_y) + btn_sy;
    button_paint_red->set_offsets();
    button_paint_red->onMouseDown = editor_paint_red_cb;
    MapEditor_UI->addWidget(button_paint_red);

    SideButton *button_paint_blue = new SideButton("Blu");
    button_paint_blue->m_x1 = btn_startx;
    button_paint_blue->m_y1 = btn_starty + 2 * (btn_sy + spc_y);
    button_paint_blue->m_x2 = btn_sx;
    button_paint_blue->m_y2 = btn_starty + 2 * (btn_sy + spc_y) + btn_sy;
    button_paint_blue->set_offsets();
    button_paint_blue->onMouseDown = editor_paint_blue_cb;
    MapEditor_UI->addWidget(button_paint_blue);

    SideButton *button_save_map = new SideButton("Save");
    button_save_map->m_x1 = display_x - 70;
    button_save_map->m_y1 = display_y - 50;
    button_save_map->m_x2 = display_x;
    button_save_map->m_y2 = display_y;
    button_save_map->set_offsets();
    button_save_map->onMouseDown = editor_save_map_cb;
    MapEditor_UI->addWidget(button_save_map);
}

static void init(void) {
    allegro_init();

    init_colors();
    MainMenu_UI = new MainMenuUI;
    GameSetup_UI = new GameSetupUI;

    Map_UI = NULL;
}

static void new_game(GameType t) {
    game = new Game;
    map = new HexMap;
    msg = new MessageLog;
    Map_UI = new MapUI;
    MapEditor_UI = new MapEditorUI;

    init_buttons();

    if(t == GameType::Game) {

        ui = Map_UI;
        Map_UI->addWidget(msg);
        Map_UI->clear_to = color_black;

        game->players[0]->m_ai_control = false;
        if(strcmp(GameSetup_UI->m_selected_player->m_name, "vs. Human") == 0) {
            game->players[1]->m_ai_control = false;
        }

        const char *map_name = GameSetup_UI->m_selected_map->m_name;
        debug("new_game(): Map name %s selected", map_name);

        sideinfo1 = new SideInfo(game->players[0]);
        sideinfo1->setpos(75, 0, 125, 30);
        sideinfo1->set_offsets();

        sideinfo2 = new SideInfo(game->players[1]);
        sideinfo2->setpos(130, 0, 180, 30);
        sideinfo2->set_offsets();

        Map_UI->addWidgets({ sideinfo1, sideinfo2 });

        ifstream in(string("maps/") + map_name, ios::in);
        if(in.fail() == true) fatal_error("new_game(): Couldn't load %s", map_name);
        map->load(in, true);
        for(auto&& h : map->m_hexes) Map_UI->addWidget(h);
        msg->add("It's %s's turn", game->controller()->m_name);

    }
    else if (t == GameType::Editor) {

        ui = MapEditor_UI;
        MapEditor_UI->addWidget(msg);

        ifstream in("./editing.map", ios::in);
        if(in.fail() == false) {
            map->load(in, false);
            msg->add("Loaded previous file (./editing.map). Move finished maps to ./maps/");
        }
        else {
            int i = 0;
            for(int y = 0; y < 7; y++) {
                for(int x = 1; x < 12; x++) {
                    addHex(x, y, 50, 0, i++);
                }
            }
        }

        for(auto&& h : map->m_hexes) MapEditor_UI->addWidget(h);

    }

    map->gen_neighbors();
}

static void delete_game(void) {
    assert(MainMenu_UI);
    ui = MainMenu_UI;

    delete map;
    delete Map_UI;
    delete MapEditor_UI;
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
    else if(ui == Map_UI or ui == GameSetup_UI or ui == MapEditor_UI) {
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
        // add button shortcuts
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
