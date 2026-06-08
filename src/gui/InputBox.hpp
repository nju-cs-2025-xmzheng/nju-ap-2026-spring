#pragma once

#include "raylib.h"
#include <functional>
#include <string>

namespace Synera::gui {

enum class InputType { Text, Password, Number };

struct InputBoxStyle {
    Font font = {0}; // Defaults to Raylib default font if font.texture.id == 0
    float fontSize = 16.0f;
    float fontSpacing = 1.0f;
    float paddingLeft = 10.0f;
    float paddingRight = 10.0f;
    float paddingTop = 8.0f;
    float paddingBottom = 8.0f;
    float borderWidth = 1.0f;

    // Colors for the various states
    Color bgNormal = Color{28, 28, 34, 255};
    Color bgHover = Color{34, 34, 42, 255};
    Color bgActive = Color{42, 48, 64, 255};
    Color bgDisabled = Color{24, 24, 30, 230};

    Color borderNormal = Color{110, 110, 120, 255};
    Color borderHover = GOLD;
    Color borderActive = GOLD;
    Color borderDisabled = DARKGRAY;

    Color textNormal = WHITE;
    Color textPlaceholder = Color{150, 150, 160, 255};
    Color textDisabled = Color{205, 205, 215, 255};

    Color selectionBg =
        Color{0, 120, 215, 100}; // Semi-transparent selection highlight
    Color cursorColor = GOLD;
};

class InputBox {
  public:
    InputBox();
    InputBox(Rectangle bounds, const std::string &placeholder = "");
    ~InputBox() = default;

    // Core loops
    void Update();
    void Draw();

    // Getters and Setters
    const std::string &GetValue() const { return value_; }
    InputBox &SetValue(const std::string &value);

    const std::string &GetPlaceholder() const { return placeholder_; }
    InputBox &SetPlaceholder(const std::string &placeholder) {
        placeholder_ = placeholder;
        return *this;
    }

    Rectangle GetBounds() const { return bounds_; }
    InputBox &SetBounds(Rectangle bounds) {
        bounds_ = bounds;
        return *this;
    }

    bool IsFocused() const { return focused_; }
    InputBox &SetFocused(bool focused);

    bool IsDisabled() const { return disabled_; }
    InputBox &SetDisabled(bool disabled) {
        disabled_ = disabled;
        return *this;
    }

    bool IsReadOnly() const { return readonly_; }
    InputBox &SetReadOnly(bool readonly) {
        readonly_ = readonly;
        return *this;
    }

    InputType GetType() const { return type_; }
    InputBox &SetType(InputType type) {
        type_ = type;
        return *this;
    }

    size_t GetMaxLength() const { return maxLength_; }
    InputBox &SetMaxLength(size_t maxLength) {
        maxLength_ = maxLength;
        return *this;
    }

    InputBox &SetCharValidator(std::function<bool(char)> validator) {
        charValidator_ = validator;
        return *this;
    }
    InputBox &
    SetValueValidator(std::function<bool(const std::string &)> validator) {
        valueValidator_ = validator;
        return *this;
    }

    const InputBoxStyle &GetStyle() const { return style_; }
    InputBox &SetStyle(const InputBoxStyle &style) {
        style_ = style;
        return *this;
    }

  private:
    // Layout and measurement helper methods
    std::string GetDisplayText() const;
    float GetTextWidth(const std::string &text) const;
    float GetTextWidthUpTo(const std::string &text, int index) const;
    int GetCharIndexAtPos(float mouse_x) const;
    void SelectWordAt(int index);
    void DeleteSelection();
    void InsertText(const std::string &text);
    void HandleClipboardPaste();
    void HandleClipboardCopy(bool cut);
    void ScrollToCursor();
    void HandleKeyPress(int key);

    // UTF-8 Helper methods
    int GetPreviousCharIndex(int index) const;
    int GetNextCharIndex(int index) const;
    int ByteIndexToCharIndex(int byte_idx) const;
    int CharIndexToByteIndex(int char_idx) const;

  private:
    Rectangle bounds_;
    std::string placeholder_;
    std::string value_;

    // Configuration and state flags
    bool focused_ = false;
    bool disabled_ = false;
    bool readonly_ = false;
    InputType type_ = InputType::Text;
    size_t maxLength_ = std::string::npos;

    // Cursor position and selection range (indices in bytes)
    int cursor_pos_ = 0;
    int select_start_ = -1;
    int select_end_ = -1;
    int selection_anchor_ = -1;
    float scroll_offset_ = 0.0f;

    // Key repeat state
    int active_repeat_key_ = 0;
    float repeat_timer_ = 0.0f;

    // Mouse interaction states
    float last_click_time_ = -100.0f;
    int click_count_ = 0;
    bool is_dragging_ = false;

    // Input constraints
    std::function<bool(char)> charValidator_;
    std::function<bool(const std::string &)> valueValidator_;

    // Styling properties
    InputBoxStyle style_;
};

} // namespace Synera::gui
