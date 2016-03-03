#pragma once

#include "./button.h"

enum class Side {
    Red,
    Blue,
    Green,
    Yellow,
    Neutral,
};

struct SideButton : Button {
    bool m_outlined;
    bool m_outline_right;
    int m_outline_cond_1;
    int m_outline_cond_2;

    explicit SideButton(const char *_name);

    void draw(void) override;
    void mouseDownEvent(void) override;

    void outline(int cur);
};

