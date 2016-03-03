#pragma once

enum class WidgetType {
    Hex,
    Button,
    Other,
};

struct Widget {
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

    // void draw_bb(void) {
    //     al_draw_rectangle(vx(m_x1), vy(m_y1), vx(m_x2), vy(m_y2), colors.very_red, 1);
    //     al_draw_circle(vx(m_x1 + m_circle_bb_radius),
    //                    vy(m_y1 + m_circle_bb_radius),
    //                    m_circle_bb_radius * scale,
    //                    colors.very_red,
    //                    1);
    // }

    void setpos(float x1, float y1, float x2, float y2);
};
