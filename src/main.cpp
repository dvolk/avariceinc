#include <allegro5/allegro.h>
#include <allegro5/allegro_color.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_native_dialog.h>

#include <cstdint>
#include <cmath>

#include <string>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

#include "./util.h"

using namespace std;

struct UI;
struct MapUI;
struct HexMap;
struct Game;
struct SideFlag;
struct Button;

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
MapUI *Map_UI;
float view_x = -65;
float view_y = 0;
float scale = 1;
double dt;
Game *game;
vector<Button *> opt_buttons;

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

    void (*onMouseDown)(void);
    void (*onMouseUp)(void);
    void (*onKeyDown)(void);

    virtual void update() {};
    virtual void draw() = 0;

    void draw_bb(void) {
        al_draw_rectangle(vx(m_x1), vy(m_y1), vx(m_x2), vy(m_y2), al_map_rgb(255,0,0), 1);
        al_draw_circle(vx(m_x1 + m_circle_bb_radius),
                       vy(m_y1 + m_circle_bb_radius),
                       m_circle_bb_radius * scale,
                       al_map_rgb(255,0,0),
                       1);
    }

    void setpos(float x1, float y1, float x2, float y2) {
        m_x1 = x1;
        m_y1 = y1;
        m_x2 = x2;
        m_y2 = y2;
    }
};

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

static Side get_current_side(void);

static bool is_opt_button(Button *b);
static void clear_opt_buttons(void);

struct Button : Widget {
    const char *m_name;
    bool m_pressed;

    explicit Button(const char *_name) {
        m_name = _name;
        m_type = WidgetType::Button;
        m_pressed = false;
    }

    void draw(void) override {
        int intensity = m_pressed ? 128 : 255;

        ALLEGRO_COLOR c = al_map_rgb(intensity, 0, 0);
        if(get_current_side() == Side::Blue) {
            c = al_map_rgb(0, 0, intensity);
        }

        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, c);
        if(m_name != NULL) {
            int y_off = (m_y2 - m_y2 - 14) / 2.0;
            al_draw_text(g_font, al_map_rgb(255,255,255), 5 + m_x1,
                         m_y1 - y_off, 0, m_name);
        }
        //draw_bb();
    }
    void mouseDownEvent(void) override {
        clear_opt_buttons();
        if(is_opt_button(this) == true) {
            this->m_pressed = true;
        }
    }
};

struct Hex;

struct HexMap {
    int moving_units = 100;
    const int max_units_moved = 8;
    const float cannon_min_range = 1;
    const float cannon_max_range = 3;

    vector<Hex *> hexes;
    vector<vector<Hex *>> old_states;

    HexMap() {
    }

    float hex_distance(Hex *h1, Hex *h2);
    vector<Hex *> neighbors(Hex *base);
    bool is_neighbor(Hex *h1, Hex *h2);
    void harvest(void);
    void move_or_attack(Hex *attacker, Hex *defender);
    void free_units(void);
    Hex *get_active_hex(void);

    void store_current_state(void);
    void clear_old_states(void);
    void undo(void);
};

static void clear_active_hex(void);

struct SideController {
    Side m_side;
    const char *m_name;
    int m_resources;
    int m_carriers;
    ALLEGRO_COLOR m_color;

    explicit SideController(Side s) {
        m_resources = 12;
        m_carriers = 0;
        m_side = s;
        if(s == Side::Red) {
            m_name = "Red";
            m_color = al_map_rgb(255,0,0);
        }
        else if(s == Side::Blue) {
            m_name = "Blue";
            m_color = al_map_rgb(0,0,255);
        }
        else {
            fatal_error("SideController(): invalid side %d", (int)s);
        }
    }
};

struct Game {
    vector<SideController *> players;

    Game() {
        players.push_back(new SideController(Side::Red));
        players.push_back(new SideController(Side::Blue));
        m_current_controller = players[0];
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

    explicit SideInfo(SideController *s) {
        m_s = s;
    }

    void draw(void) override {
        int y_off = (m_y2 - m_y2 - 14) / 2.0;
        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2, m_s->m_color);
        al_draw_textf(g_font, al_map_rgb(255,255,255), 5 + m_x1,
                      m_y1 - y_off, 0,
                      "%d/%d", m_s->m_resources, m_s->m_carriers);
    }
};

// currently just the inscribed circle of the hexagon with side a
struct Hex : Widget {
    int m_level;
    float m_a;
    float m_r;
    bool m_active;
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

    Hex(float x1, float y1, float a, int level) {
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
        if(m_level < 1)
            return;
    };

    void draw(void) override {
        if(m_level < 1)
            return;

        ALLEGRO_COLOR c = al_map_rgb(128, 128, 128);

        if(m_side == Side::Red) {
            ALLEGRO_COLOR red_hex = al_map_rgb(240, 128, 128);
            ALLEGRO_COLOR red_hex_active = al_map_rgb(250, 50, 50);
            if(m_active == true)
                c = red_hex_active;
            else
                c = red_hex;
        }
        else if(m_side == Side::Blue) {
            ALLEGRO_COLOR blue_hex = al_map_rgb(128, 128, 240);
            ALLEGRO_COLOR blue_hex_active = al_map_rgb(50, 50, 250);
            if(m_active == true)
                c = blue_hex_active;
            else
                c = blue_hex;
        }

        al_draw_filled_circle(vx(m_cx),
                              vy(m_cy),
                              m_r * scale,
                              c);
        al_draw_textf(g_font,
                      al_map_rgb(255,255,255),
                      vx(m_cx) - 5,
                      vy(m_cy) - 7,
                      0,
                      "%d", m_level);

        if(m_contains_harvester == true)
            al_draw_text(g_font, al_map_rgb(255,255,255),
                         vx(m_cx) - 25, vy(m_cy) - 25,
                         0, "H");

        if(m_contains_armory == true)
            al_draw_text(g_font, al_map_rgb(255,255,255),
                         vx(m_cx) - 5, vy(m_cy) - 25,
                         0, "A");

        if(m_contains_cannon == true)
            al_draw_text(g_font, al_map_rgb(255,255,255),
                         vx(m_cx) + 15, vy(m_cy) - 25,
                         0, "C");

        if(m_ammo > 0 or m_loaded_ammo > 0)
            al_draw_textf(g_font, al_map_rgb(255,255,255),
                         vx(m_cx) + 15, vy(m_cy) - 7,
                          0, "%d", m_ammo + m_loaded_ammo);

        if(m_units_free > 0 or m_units_moved > 0)
            al_draw_textf(g_font, al_map_rgb(255,255,255),
                          vx(m_cx) - 12, vy(m_cy) + 10,
                          0, "%d/%d", m_units_free, m_units_moved);

        // draw_bb();
    }

    void mouseDownEvent(void) override {
        if(m_level < 1)
            return;


        // for(auto&& neighbor : map->neighbors(this)) {
        //     neighbor->m_active = true;
        // }
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

struct MapUI : UI {
private:
    MapAction m_current_action;
public:
    MapUI() {
        m_current_action = MapAction::MovingUnits;
    }

    void mouseDownEvent(void) override;

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


};

void MapUI::mouseDownEvent(void) {
    if(mouse_button == 2) {
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

void HexMap::undo(void) {
    if(old_states.empty() == true)
        return;

    vector<Hex *> old = old_states.back();
    debug("undo to %p", old);

    assert(old.size() == hexes.size());

    for(size_t i = 0; i < hexes.size(); i++) {
        *hexes[i] = *old[i];
    }

    for(auto&& h : old) {
        delete h;
    }
    old_states.pop_back();
    clear_active_hex();
}

void HexMap::store_current_state(void) {
    vector<Hex *> copy;

    for(auto&& h : hexes) {
        Hex *hc = new Hex(*h);
        copy.push_back(hc);
    }

    old_states.push_back(copy);

    printf("states: ");
    for(auto&& s : old_states)
        printf("%p, ", s);
    printf("\n");
}

void HexMap::clear_old_states(void) {
    for(auto&& s : old_states) {
        for(auto&& h : s) {
            delete h;
        }
    }
    old_states.clear();
}

Hex *HexMap::get_active_hex(void) {
    for(auto&& h : map->hexes) {
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
    vector<Hex *> neighbors;
    for(auto&& h : hexes) {
        if(hex_distance(base, h) < 2 * base->m_r + 10) {
            if(h != base)
                neighbors.push_back(h);
        }
    }

    return neighbors;
}

bool is_neighbor(Hex *h1, Hex *h2) {
    vector<Hex *> neighbors = map->neighbors(h1);
    return find(neighbors.begin(), neighbors.end(), h2) != neighbors.end();
}

void HexMap::harvest(void) {
    for(auto&& h : hexes) {
        h->m_harvested = false;
    }
    for(auto&& h : hexes) {
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
    if(defender->m_side == Side::Neutral or
       defender->m_side == attacker->m_side) {
        // moving units
        int moved = min(moving_units, min(attacker->m_units_free, max_units_moved));
        defender->m_units_moved += moved;
        defender->m_side = attacker->m_side;
        attacker->m_units_free -= moved;
        debug("%p moves %d to %p", defender, moved, attacker);
    }
    else {
        // attacking
        int moved = min(moving_units, min(attacker->m_units_free, max_units_moved));
        if(moved >= defender->m_units_free + defender->m_units_moved) {
            // we've conquered this hex
            defender->m_units_moved = moved - (defender->m_units_free + defender->m_units_moved);
            defender->m_side = attacker->m_side;
            defender->m_units_free = 0;
            attacker->m_units_free -= moved;
        } else {
            // attacked but not conquered
            int free_units_killed = max(defender->m_units_free, defender->m_units_free - moved);
            attacker->m_units_free -= moved;
            defender->m_units_free -= free_units_killed;
            int rest = moved - free_units_killed;
            debug("rest %d", rest);
            if(rest > 0)
                defender->m_units_moved -= rest;
        }
    }

}

void HexMap::free_units(void) {
    for(auto&& h : hexes) {
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
    for(auto&& h : map->hexes) {
        if(h->m_active == true) {
            prev = h;
            break;
        }
    }

    clear_active_hex();
    if(prev == NULL && h->m_side != game->m_current_controller->m_side)
        return;
    if(h->m_side != Side::Neutral and
       h->m_side == game->m_current_controller->m_side) {
        h->m_active = true;
    }

    if(get_current_action() == MapAction::MovingUnits) {
        if(prev != NULL && h != prev && prev->m_units_free > 0 && h->alive()) {
            bool move_ok = false;

            if(prev->m_side != h->m_side) {
                if(is_neighbor(prev, h) == true) {
                    move_ok = true;
                }
                else {
                    if(game->controller()->m_carriers >= 1) {
                        game->controller()->m_carriers -= 1;
                        move_ok = true;
                    } else {
                        return;
                    }
                }
            } else {
                move_ok = true;
            }

            if(move_ok == true) {
                map->store_current_state();
                map->move_or_attack(prev, h);
                clear_active_hex();
            }
        }
    }
    else if (get_current_action() == MapAction::FireCannon) {
        if(prev == NULL) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            debug("no hex selected");
            return;
        }

        if(prev->m_contains_cannon == false) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            debug("hex doesn't contain cannon");
            return;
        }

        float dist = map->hex_distance(prev, h);

        if(dist < (0.4 + 2 * map->cannon_min_range) * prev->m_circle_bb_radius or dist > (0.4 + 2 * map->cannon_max_range) * prev->m_circle_bb_radius) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            debug("target not in range");
            return;
        }

        if(prev->m_ammo <= 0) {
            set_current_action(MapAction::MovingUnits);
            clear_active_hex();
            debug("cannon has no ammo");
            return;
        }

        debug("Firing cannon");
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
            map->store_current_state();
            h->m_units_moved += 1;
            game->controller_pay(8);
        } else {
            debug("you can only recruit on hexes with armories");
        }

        clear_active_hex();
        set_current_action(MapAction::MovingUnits);
    }
}

void MapUI::draw(void) {
    for(auto&& w : widgets) {
        if(w->m_type == WidgetType::Hex) {
            w->draw();
        }
    }

    // UI::draw();

    Hex *act = NULL;
    for(auto&& h : map->hexes) {
        if(h->m_active == true) {
            act = h;
            break;
        }
    }

    if(act != NULL) {
        if(m_current_action == MapAction::MovingUnits) {
            al_draw_circle(vx(act->m_x1 + act->m_circle_bb_radius),
                           vy(act->m_y1 + act->m_circle_bb_radius),
                           round(2.4 * act->m_circle_bb_radius * scale),
                           al_map_rgb(255,0,0),
                           2);
        }
        else if(m_current_action == MapAction::FireCannon and
                act->m_contains_cannon == true) {
            al_draw_circle(vx(act->m_x1 + act->m_circle_bb_radius),
                           vy(act->m_y1 + act->m_circle_bb_radius),
                           round((0.4 + 2.0 * map->cannon_min_range) * act->m_circle_bb_radius * scale),
                           al_map_rgb(255,0,0),
                           2);
            al_draw_circle(vx(act->m_x1 + act->m_circle_bb_radius),
                           vy(act->m_y1 + act->m_circle_bb_radius),
                           round((0.4 + 2.0 * map->cannon_max_range) * act->m_circle_bb_radius * scale),
                           al_map_rgb(255,0,0),
                           2);
        }
    }
    for(auto&& w : widgets) {
        if(w->m_type != WidgetType::Hex) {
            w->draw();
        }
    }
}

void MapUI::update(void) {
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

static void allegro_init() {
    int ret = 0;
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

    al_set_new_display_flags(ALLEGRO_WINDOWED);

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

Hex *addHex(float x, float y, float a, int level) {
    float r  = 0.5 * sqrt(3) * a;
    float sx = 2 * r;
    float sy = 2 * r;

    float x_off = a * sin(1.0/4.0);
    float y_off = int(x) % 2 == 0 ? -r : 0;

    Hex *h = new Hex(x * (sx - x_off),
                     y * sy - y_off,
                     a,
                     level);

    Map_UI->addWidget(h);
    map->hexes.push_back(h);
    return h;
}

static void end_turn_cb(void) {
    SideController *s = game->get_next_controller();
    map->harvest();
    map->free_units();
    map->clear_old_states();
    clear_active_hex();
    debug("Now it's %s's turn", s->m_name);
}

static void build_harvester_cb(void) {
    if(game->controller_has_resources(10)) {
        clear_active_hex();
        Map_UI->set_current_action(MapAction::BuildHarvester);
    }
    else {
        debug("you don't have enough money (10)");
        clear_opt_buttons();
    }
}
static void destroy_harvester_cb(void) {
    clear_active_hex();
    Map_UI->set_current_action(MapAction::DestroyHarvester);
}
static void build_armory_cb(void) {
    if(game->controller_has_resources(35)) {
        clear_active_hex();
        Map_UI->set_current_action(MapAction::BuildArmory);
    } else {
        debug("insufficient resources (35)");
        clear_opt_buttons();
    }
}
static void build_carrier_cb(void) {
    if(game->controller_has_resources(50)) {
        game->controller_pay(50);
        game->controller()->m_carriers += 1;
        clear_active_hex();
        Map_UI->set_current_action(MapAction::MovingUnits);
    } else {
        debug("insufficient resources (50)");
        clear_opt_buttons();
    }
}
static void build_walker_cb(void) {
    if(game->controller_has_resources(8)) {
        clear_active_hex();
        Map_UI->set_current_action(MapAction::BuildWalker);
    } else {
        debug("insufficient resources (min 8)");
        clear_opt_buttons();
    }
}
static void build_cannon_cb(void) {
    if(game->controller_has_resources(30)) {
        clear_active_hex();
        Map_UI->set_current_action(MapAction::BuildCannon);
    } else {
        debug("insufficient resources (30)");
        clear_opt_buttons();
    }
}
static void build_cannon_ammo_cb(void) {
    if(game->controller_has_resources(20)) {
        clear_active_hex();
        Map_UI->set_current_action(MapAction::AddAmmoToCannon);
    } else {
        debug("insufficient resources (20)");
        clear_opt_buttons();
    }
}
static void fire_cannon_cb(void) {
    Hex *act = map->get_active_hex();
    if(act == NULL) {
        debug("select the cannon first");
        Map_UI->set_current_action(MapAction::MovingUnits);
        clear_opt_buttons();
        return;
    }
    if(act->m_contains_cannon == false) {
        debug("that hex doesn't have a cannon");
        Map_UI->set_current_action(MapAction::MovingUnits);
        clear_opt_buttons();
        return;
    }
    if(act->m_ammo == false) {
        debug("that cannon doesn't have ammo loaded");
        Map_UI->set_current_action(MapAction::MovingUnits);
        clear_opt_buttons();
        return;
    }
    if(act->m_side != get_current_side()) {
        fatal_error("oy! that's not your cannon!");
    }

    Map_UI->set_current_action(MapAction::FireCannon);
}
static void map_undo_cb(void) {
    map->undo();
}


static void init(void) {
    allegro_init();

    game = new Game;
    map = new HexMap;
    Map_UI = new MapUI;
    Map_UI->clear_to = al_map_rgb(0, 0, 0);

    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 5; y++) {

            int level = 2 + (x * y) % 7;
            Hex *h = addHex(x, y, 50, level);

            if(level == 5) {
                h->m_contains_harvester = true;
                h->m_contains_cannon = true;
                h->m_ammo = 3;
                h->m_side = Side::Red;
                h->m_units_free = 5;
                h->m_units_moved = 0;
            }
            if(level == 4) {
                h->m_contains_harvester = true;
                h->m_side = Side::Blue;
                h->m_units_free = 3;
                h->m_units_moved = 0;
            }
        }
    }

    Button *end_turn = new Button("Turn");
    end_turn->m_x1 = display_x - 70;
    end_turn->m_y1 = display_y - 50;
    end_turn->m_x2 = display_x;
    end_turn->m_y2 = display_y;
    end_turn->onMouseDown = end_turn_cb;
    Map_UI->addWidget(end_turn);

    Button *undo = new Button("Undo");
    undo->m_x1 = display_x - 70;
    undo->m_y1 = display_y - 105;
    undo->m_x2 = display_x;
    undo->m_y2 = display_y - 55;
    undo->onMouseDown = map_undo_cb;
    Map_UI->addWidget(undo);

    int btn_startx = 0;
    int btn_starty = 0;
    int btn_sx = 60;
    int btn_sy = 40;
    int spc_y = 5;

    Button *button_build_harvester = new Button("Harv +");
    button_build_harvester->m_x1 = btn_startx;
    button_build_harvester->m_y1 = btn_starty;
    button_build_harvester->m_x2 = btn_sx;
    button_build_harvester->m_y2 = btn_sy;
    button_build_harvester->onMouseDown = build_harvester_cb;
    Map_UI->addWidget(button_build_harvester);

    Button *button_destroy_harvester = new Button("Harv -");
    button_destroy_harvester->m_x1 = btn_startx;
    button_destroy_harvester->m_y1 = btn_starty + 1 * (btn_sy + spc_y);
    button_destroy_harvester->m_x2 = btn_sx;
    button_destroy_harvester->m_y2 = btn_starty + 1 * (btn_sy + spc_y) + btn_sy;
    button_destroy_harvester->onMouseDown = destroy_harvester_cb;
    Map_UI->addWidget(button_destroy_harvester);

    Button *button_build_cannon = new Button("Cann +");
    button_build_cannon->m_x1 = btn_startx;
    button_build_cannon->m_y1 = btn_starty + 2 * (btn_sy + spc_y);
    button_build_cannon->m_x2 = btn_sx;
    button_build_cannon->m_y2 = btn_starty + 2 * (btn_sy + spc_y) + btn_sy;
    button_build_cannon->onMouseDown = build_cannon_cb;
    Map_UI->addWidget(button_build_cannon);

    Button *button_build_cannon_ammo = new Button("Ammo");
    button_build_cannon_ammo->m_x1 = btn_startx;
    button_build_cannon_ammo->m_y1 = btn_starty + 3 * (btn_sy + spc_y);
    button_build_cannon_ammo->m_x2 = btn_sx;
    button_build_cannon_ammo->m_y2 = btn_starty + 3 * (btn_sy + spc_y) + btn_sy;
    button_build_cannon_ammo->onMouseDown = build_cannon_ammo_cb;
    Map_UI->addWidget(button_build_cannon_ammo);

    Button *button_fire_cannon = new Button("Fire");
    button_fire_cannon->m_x1 = btn_startx;
    button_fire_cannon->m_y1 = btn_starty + 4 * (btn_sy + spc_y);
    button_fire_cannon->m_x2 = btn_sx;
    button_fire_cannon->m_y2 = btn_starty + 4 * (btn_sy + spc_y) + btn_sy;
    button_fire_cannon->onMouseDown = fire_cannon_cb;
    Map_UI->addWidget(button_fire_cannon);

    Button *button_build_armory = new Button("Armo");
    button_build_armory->m_x1 = btn_startx;
    button_build_armory->m_y1 = btn_starty + 5 * (btn_sy + spc_y);
    button_build_armory->m_x2 = btn_sx;
    button_build_armory->m_y2 = btn_starty + 5 * (btn_sy + spc_y) + btn_sy;
    button_build_armory->onMouseDown = build_armory_cb;
    Map_UI->addWidget(button_build_armory);

    Button *button_build_walker = new Button("Walk");
    button_build_walker->m_x1 = btn_startx;
    button_build_walker->m_y1 = btn_starty + 6 * (btn_sy + spc_y);
    button_build_walker->m_x2 = btn_sx;
    button_build_walker->m_y2 = btn_starty + 6 * (btn_sy + spc_y) + btn_sy;
    button_build_walker->onMouseDown = build_walker_cb;
    Map_UI->addWidget(button_build_walker);

    Button *button_build_carrier = new Button("Carr");
    button_build_carrier->m_x1 = btn_startx;
    button_build_carrier->m_y1 = btn_starty + 7 * (btn_sy + spc_y);
    button_build_carrier->m_x2 = btn_sx;
    button_build_carrier->m_y2 = btn_starty + 7 * (btn_sy + spc_y) + btn_sy;
    button_build_carrier->onMouseDown = build_carrier_cb;
    Map_UI->addWidget(button_build_carrier);

    opt_buttons = { button_build_harvester, button_destroy_harvester,
                    button_build_cannon, button_build_cannon_ammo,
                    button_fire_cannon, button_build_armory,
                    button_build_walker, button_build_carrier };

    SideInfo *sideinfo1 = new SideInfo(game->players[0]);
    sideinfo1->setpos(75, 0, 125, 30);

    SideInfo *sideinfo2 = new SideInfo(game->players[1]);
    sideinfo2->setpos(130, 0, 180, 30);

    Map_UI->addWidgets({ sideinfo1, sideinfo2 });

    ui = Map_UI;
}

static bool is_opt_button(Button *b) {
    return find(opt_buttons.begin(), opt_buttons.end(), b) != opt_buttons.end();
}

static void clear_opt_buttons(void) {
    for(auto&& b : opt_buttons) {
        b->m_pressed = false;
    }
}

static void clear_active_hex(void) {
    for(auto &&h : map->hexes) {
        h->m_active = false;
    }
}

void register_global_key_callback(bool (*cb)(void)) {
    global_key_callback = cb;
}

bool handle_global_keys(void) {
    if(key == ALLEGRO_KEY_ESCAPE) {
        running = false;
        return true;
    }
    return true;
}

void mainloop() {
    bool redraw = true;
    bool was_mouse_down = false;

    register_global_key_callback(handle_global_keys);

    ALLEGRO_EVENT ev;

    running = true;

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
    for(auto&& h : map->hexes)
        delete h;
    delete map;
}

int main() {
    init();
    mainloop();
    deinit();

    return 0;
}
