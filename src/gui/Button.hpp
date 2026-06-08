#pragma once

#include "raylib.h"
#include <functional>

namespace Synera::gui {

// Small adapter so the button module can draw/measure text without depending
// on GameApp's private font members. GameApp builds one of these from its
// DrawGameText / MeasureGameText helpers and hands it to every DrawButton call.
struct TextRenderer {
    std::function<int(const char *text, int font_size, bool bold)> measure;
    std::function<void(const char *text, int x, int y, int font_size,
                       Color color, bool bold)>
        draw;
};

// Semantic colour families. Every button shares the exact same structure
// (square corners, 1px border, hover brighten, centred bold label) — the
// variant only swaps the fill colour so a button's meaning stays readable.
enum class ButtonVariant {
    Neutral, // generic menu entries, back/cancel
    Primary, // confirm, buy, start combat, host
    Danger,  // exit to menu, delete, combat in progress
    Info,    // level up, join, freeze
    Warning, // refresh, cancel-ready / readied state
};

struct ButtonStyle {
    ButtonVariant variant = ButtonVariant::Neutral;
    int font_size = 16;
};

// Draws one unified button and reports whether it was clicked this frame.
//   enabled == false  greys it out and ignores hover/click.
//   block_click        suppresses the click result without changing visuals
//                      (used for menu-transition cooldowns).
bool DrawButton(const TextRenderer &text, Rectangle bounds, const char *label,
                ButtonStyle style = {}, bool enabled = true,
                bool block_click = false);

} // namespace Synera::gui
