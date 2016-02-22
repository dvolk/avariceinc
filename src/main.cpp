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
struct HexMap;

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
UI *Map_UI;
int view_x;
int view_y;
int scale;

enum class WidgetType {
    Hex,
    Other,
};

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
        al_draw_rectangle(m_x1, m_y1, m_x2, m_y2, al_map_rgb(255,0,0), 1);
        al_draw_circle(m_x1 + m_circle_bb_radius,
                       m_y1 + m_circle_bb_radius,
                       m_circle_bb_radius,
                       al_map_rgb(255,0,0),
                       1);
    }
};

struct Button : Widget {
    void draw(void) override {
        al_draw_filled_rectangle(m_x1, m_y1, m_x2, m_y2,
                                 al_map_rgb(128,128,255));
    }
};

struct Hex;

struct HexMap {
    vector<Hex *> hexes;

    float hex_distance(Hex *h1, Hex *h2);    
    vector<Hex *> neighbors(Hex *base);
    void harvest(void);
};

static void clear_active_hex(void);

// currently just the inscribed circle of the hexagon with side a
struct Hex : Widget {
    int m_level;
    float m_a;
    float m_r;
    bool m_active;
    float m_cx;
    float m_cy;

    bool contains_harvester;
    bool harvested;
    bool contains_armory;
    bool contains_cannon;

    int units_free;
    int units_moved;

    Hex(float x1, float y1, float a, int level) {
        m_x1 = round(x1);
        m_y1 = round(y1);
        m_a = round(a);
        // radius of inscribed circle
        float r = round(0.5 * sqrt(3) * a);
        m_r = r;
        m_cx = m_x1 + r;
        m_cy = m_y1 + r;
        m_x2 = m_x1 + 2 * r;
        m_y2 = m_y1 + 2 * r;
        debug("Hex::Hex(): m_x2 = %f, m_y2 = %f\n", m_x2, m_y2);
        m_circle_bb = true;
        m_circle_bb_radius = r;

        m_active = false;
        m_type = WidgetType::Hex;

        m_level = level;
        contains_harvester = false;
        harvested = false;
        contains_armory = false;
        contains_cannon = false;
        units_free = 3;
        units_moved = 3;
    }

    void update(void) override {
        if(m_level < 1)
            return;
    };

    void draw(void) override {
        if(m_level < 1)
            return;
        
        ALLEGRO_COLOR c = m_active ? al_map_rgb(250, 50, 50) : al_map_rgb(240, 128, 128);
            
        
        al_draw_filled_circle(m_cx,
                              m_cy,
                              m_r,
                              c);
        al_draw_textf(g_font,
                      al_map_rgb(255,255,255),
                      m_cx - 5,
                      m_cy - 7,
                      0,
                      "%d", m_level);

        if(contains_harvester == true)
            al_draw_text(g_font, al_map_rgb(255,255,255),
                         m_cx - 25, m_cy - 25,
                         0, "H");

        if(contains_armory == true)
            al_draw_text(g_font, al_map_rgb(255,255,255),
                         m_cx - 5, m_cy - 25,
                         0, "A");

        if(contains_cannon == true)
            al_draw_text(g_font, al_map_rgb(255,255,255),
                         m_cx + 15, m_cy - 25,
                         0, "C");

        if(units_free > 0 or units_moved > 0)
            al_draw_textf(g_font, al_map_rgb(255,255,255),
                         m_cx - 12, m_cy + 10,
                          0, "%d/%d", units_free, units_moved);
            

        draw_bb();
    };

    void mouseDownEvent(void) override {
        if(m_level < 1)
            return;
        
        m_level--;

        clear_active_hex();
        m_active = true;
        for(auto&& neighbor : map->neighbors(this)) {
            neighbor->m_active = true;
        }
    }

    void harvest(void) {
        m_level -= 1;
        harvested = true;
    }
};

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

struct UI {
    std::vector<Widget *> widgets;

    ALLEGRO_COLOR clear_to;

    virtual ~UI() { }

    void mouseDownEvent(void);
    void mouseUpEvent(void) {};
    void keyDownEvent(void) {};
    void hoverOverEvent(void) {};

    void update(void);
    virtual void draw(void);

    void addWidget(Widget *w) { widgets.push_back(w); }
    void addWidgets(vector<Widget *> ws) {
        for(auto w: ws)
            addWidget(w);
    }
};

void UI::mouseDownEvent(void) {
    debug("UI::mouseDownEvent()");
    for(auto& widget : widgets) {
        bool hit = false;
        if(widget->m_circle_bb == false) {
            hit =  widget->m_x1 <= mouse_x
                && widget->m_y1 <= mouse_y
                && widget->m_x1 + widget->m_x2 >= mouse_x
                && widget->m_y1 + widget->m_y2 >= mouse_y;
        }
        else {
            hit = pow((mouse_x - (widget->m_x1 + widget->m_circle_bb_radius)), 2)
                + pow((mouse_y - (widget->m_y1 + widget->m_circle_bb_radius)), 2)
                < pow(widget->m_circle_bb_radius, 2);
        }

        if(hit == true) {
            widget->mouseDownEvent();
            if(widget->onMouseDown != nullptr) {
                widget->onMouseDown();
            }
        }
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

float HexMap::hex_distance(Hex *h1, Hex *h2) {
    return sqrt(pow(h1->m_cx - h2->m_cx, 2)
                + pow(h1->m_cy - h2->m_cy, 2));
}

vector<Hex *> HexMap::neighbors(Hex *base) {
    vector<Hex *> neighbors;
    for(auto&& h : hexes) {
        if(hex_distance(base, h) < 2 * base->m_r + 10) {
            neighbors.push_back(h);
        }
    }
    
    return neighbors;
}

void HexMap::harvest(void) {
    for(auto&& h : hexes) {
        h->harvested = false;
    }
    for(auto&& h : hexes) {
        if(h->contains_harvester == true) {
            for(auto&& h_neighbor : neighbors(h)) {
                if(h_neighbor->harvested == false) {
                    h_neighbor->harvest();
                }
            }
        }
    }
}

enum class MapAction {
    MovingUnits,
};

struct MapUI : UI {
    MapAction current_action;
};

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

    g_font = al_load_font("media/fonts/DejaVuSans-Bold.ttf", 14, 0);
    if(g_font == NULL)
        fatal_error("failed to load font");
    else
        info("Loaded font");

    ret = al_init_native_dialog_addon();
    if(ret == false)
        fatal_error("Failed to initialize allegro addon: native dialogs.");
    else
        info("Initialized allegro addon: native dialogs.");

    display_x = 1280;
    display_y = 720;
    
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

Hex *addHex( float x, float y, float a, int level ) {
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
    map->harvest();
}

static void init(void) {
    allegro_init();

    map = new HexMap;
    Map_UI = new MapUI;
    Map_UI->clear_to = al_map_rgb(0, 0, 0);

    for(int x = 0; x < 10; x++) {
        for(int y = 0; y < 6; y++) {

            int level = 2 + (x * y) % 7;
            Hex *h = addHex(x, y, 50, level);

            if(level == 5) {
                h->contains_harvester = true;
            }
        }
    }

    Button *end_turn = new Button;
    end_turn->m_x1 = 1000;
    end_turn->m_y1 = 600;
    end_turn->m_x2 = 1100;
    end_turn->m_y2 = 650;
    end_turn->onMouseDown = end_turn_cb;
    Map_UI->addWidget(end_turn);
    
    ui = Map_UI;
}

static void clear_active_hex(void) {
    for(auto &&h : map->hexes) {
        h->m_active = false;
    }
}

void mainloop() {
    bool redraw = true;
    bool was_mouse_down = false;

    ALLEGRO_EVENT ev;

    running = true;

    // main loop
    while(running) {
        bool draw_hover = false;

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
