#include "gui/Button.hpp"

namespace Synera::gui {

namespace {

// Fill colours per variant. Borders, text and the hover highlight are shared
// across every variant so the whole UI reads as one button family.
struct Palette {
    Color fill;
    Color fill_hover;
};

Palette PaletteFor(ButtonVariant variant) {
    switch (variant) {
    case ButtonVariant::Primary:
        return {Color{30, 110, 55, 255}, Color{40, 140, 70, 255}};
    case ButtonVariant::Danger:
        return {Color{140, 45, 48, 255}, Color{185, 60, 60, 255}};
    case ButtonVariant::Info:
        return {Color{30, 70, 150, 255}, Color{45, 95, 185, 255}};
    case ButtonVariant::Warning:
        return {Color{150, 75, 30, 255}, Color{190, 100, 40, 255}};
    case ButtonVariant::Neutral:
    default:
        return {Color{30, 30, 38, 255}, Color{50, 60, 90, 255}};
    }
}

// Shared, variant-independent styling — this is what makes every button look
// like part of the same set.
constexpr Color kBorder = Color{80, 80, 100, 255};
constexpr Color kBorderHover = GOLD;
constexpr Color kFillDisabled = Color{45, 45, 52, 255};
constexpr Color kBorderDisabled = Color{55, 55, 60, 255};
constexpr Color kText = WHITE;
constexpr Color kTextDisabled = GRAY;

} // namespace

bool DrawButton(const TextRenderer &text, Rectangle bounds, const char *label,
                ButtonStyle style, bool enabled, bool block_click) {
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);

    Palette palette = PaletteFor(style.variant);
    Color fill =
        !enabled ? kFillDisabled : (hover ? palette.fill_hover : palette.fill);
    Color border =
        !enabled ? kBorderDisabled : (hover ? kBorderHover : kBorder);
    Color label_color = enabled ? kText : kTextDisabled;

    // Square corners only — no rounded rectangles anywhere.
    DrawRectangleRec(bounds, fill);
    DrawRectangleLinesEx(bounds, 1.0f, border);

    int text_w = text.measure(label, style.font_size, true);
    int tx = (int)(bounds.x + (bounds.width - text_w) / 2.0f);
    int ty = (int)(bounds.y + (bounds.height - style.font_size) / 2.0f);
    text.draw(label, tx, ty, style.font_size, label_color, true);

    return hover && !block_click && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

} // namespace Synera::gui
