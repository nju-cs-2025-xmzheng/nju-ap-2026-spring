#include "gui/InputBox.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace Synera::gui {

namespace {

#if defined(__APPLE__)
bool IsCtrlOrCmdDown() {
    return IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}
#else
bool IsCtrlOrCmdDown() {
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
}
#endif

// Returns true if the byte is a UTF-8 continuation byte (starts with 10xxxxxx
// in binary)
bool IsContinuationByte(char ch) {
    return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
}

// Check if character is a "word" character for double click selection
bool IsWordChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' ||
           (static_cast<unsigned char>(ch) & 0x80) != 0;
}

} // namespace

InputBox::InputBox() : bounds_({0.0f, 0.0f, 0.0f, 0.0f}), placeholder_("") {}

InputBox::InputBox(Rectangle bounds, const std::string &placeholder)
    : bounds_(bounds), placeholder_(placeholder) {}

InputBox &InputBox::SetValue(const std::string &value) {
    // Check validation if any
    if (valueValidator_ && !valueValidator_(value)) {
        return *this;
    }
    value_ = value;
    cursor_pos_ = (int)value_.length();
    select_start_ = -1;
    select_end_ = -1;
    selection_anchor_ = -1;
    scroll_offset_ = 0.0f;
    ScrollToCursor();
    return *this;
}

InputBox &InputBox::SetFocused(bool focused) {
    if (focused_ != focused) {
        focused_ = focused;
        if (!focused_) {
            select_start_ = -1;
            select_end_ = -1;
            selection_anchor_ = -1;
            active_repeat_key_ = 0;
        }
    }
    return *this;
}

float InputBox::GetTextWidth(const std::string &text) const {
    if (style_.font.texture.id == 0) {
        return (float)MeasureText(text.c_str(), (int)style_.fontSize);
    } else {
        Vector2 size = MeasureTextEx(style_.font, text.c_str(), style_.fontSize,
                                     style_.fontSpacing);
        return size.x;
    }
}

float InputBox::GetTextWidthUpTo(const std::string &text, int index) const {
    if (type_ == InputType::Password) {
        int chars = ByteIndexToCharIndex(index);
        std::string masked(chars, '*');
        return GetTextWidth(masked);
    } else {
        return GetTextWidth(value_.substr(0, index));
    }
}

int InputBox::GetCharIndexAtPos(float mouse_x) const {
    std::string display_text = GetDisplayText();
    int len = (int)display_text.length();

    float viewport_x = bounds_.x + style_.paddingLeft;
    int best_idx = 0;
    float min_dist = std::numeric_limits<float>::max();

    // We want to map back to byte index in value_
    // So we iterate through the byte indices of value_
    int byte_idx = 0;
    while (true) {
        float char_x = viewport_x + GetTextWidthUpTo(display_text, byte_idx) -
                       scroll_offset_;
        float dist = std::abs(mouse_x - char_x);
        if (dist < min_dist) {
            min_dist = dist;
            best_idx = byte_idx;
        }

        if (byte_idx >= (int)value_.length()) {
            break;
        }
        byte_idx = GetNextCharIndex(byte_idx);
    }

    return best_idx;
}

void InputBox::SelectWordAt(int index) {
    if (value_.empty())
        return;
    int len = (int)value_.length();
    if (index < 0)
        index = 0;
    if (index > len)
        index = len;

    int start = index;
    int end = index;

    // Find left bound
    if (index > 0) {
        int prev = GetPreviousCharIndex(index);
        bool word_mode = IsWordChar(value_[prev]);
        while (start > 0) {
            int p = GetPreviousCharIndex(start);
            if (IsWordChar(value_[p]) != word_mode) {
                break;
            }
            start = p;
        }
    }

    // Find right bound
    if (index < len) {
        bool word_mode = IsWordChar(value_[index]);
        while (end < len) {
            if (IsWordChar(value_[end]) != word_mode) {
                break;
            }
            end = GetNextCharIndex(end);
        }
    }

    select_start_ = start;
    select_end_ = end;
    cursor_pos_ = end;
    selection_anchor_ = start;
}

void InputBox::DeleteSelection() {
    if (select_start_ != -1 && select_end_ != -1 &&
        select_start_ != select_end_) {
        value_.erase(select_start_, select_end_ - select_start_);
        cursor_pos_ = select_start_;
        select_start_ = -1;
        select_end_ = -1;
        selection_anchor_ = -1;
    }
}

void InputBox::InsertText(const std::string &text) {
    DeleteSelection();

    std::string filtered;
    for (size_t i = 0; i < text.length();) {
        // Find next UTF-8 character boundary to process char by char
        size_t next = i + 1;
        while (next < text.length() && IsContinuationByte(text[next])) {
            next++;
        }
        std::string single_char = text.substr(i, next - i);

        bool char_ok = true;
        // Check single character validation if ASCII
        if (single_char.length() == 1) {
            char ch = single_char[0];
            if (charValidator_ && !charValidator_(ch)) {
                char_ok = false;
            } else if (type_ == InputType::Number) {
                // Allow digits, minus, decimal point
                if (!std::isdigit(static_cast<unsigned char>(ch)) &&
                    ch != '-' && ch != '.') {
                    char_ok = false;
                }
            }
        }

        if (char_ok) {
            filtered.append(single_char);
        }

        i = next;
    }

    if (filtered.empty())
        return;

    if (maxLength_ != std::string::npos) {
        if (value_.length() + filtered.length() > maxLength_) {
            // Truncate to maximum length
            size_t allowed_bytes = maxLength_ - value_.length();
            if (allowed_bytes <= 0)
                return;

            // Align to UTF-8 character boundary
            size_t trunc_len = allowed_bytes;
            while (trunc_len > 0 && trunc_len < filtered.length() &&
                   IsContinuationByte(filtered[trunc_len])) {
                trunc_len--;
            }
            if (trunc_len == 0)
                return;
            filtered = filtered.substr(0, trunc_len);
        }
    }

    std::string new_val = value_;
    new_val.insert(cursor_pos_, filtered);

    if (valueValidator_ && !valueValidator_(new_val)) {
        return;
    }

    value_ = new_val;
    cursor_pos_ += (int)filtered.length();
}

void InputBox::HandleClipboardPaste() {
    const char *clip = GetClipboardText();
    if (clip != nullptr) {
        InsertText(std::string(clip));
    }
}

void InputBox::HandleClipboardCopy(bool cut) {
    if (select_start_ != -1 && select_end_ != -1 &&
        select_start_ != select_end_) {
        std::string selected =
            value_.substr(select_start_, select_end_ - select_start_);
        SetClipboardText(selected.c_str());
        if (cut) {
            DeleteSelection();
        }
    }
}

void InputBox::ScrollToCursor() {
    float cursor_width = GetTextWidthUpTo(GetDisplayText(), cursor_pos_);
    float visible_width =
        bounds_.width - style_.paddingLeft - style_.paddingRight;

    float cursor_vis_x = cursor_width - scroll_offset_;

    if (cursor_vis_x < 0.0f) {
        scroll_offset_ = cursor_width;
    } else if (cursor_vis_x > visible_width) {
        scroll_offset_ = cursor_width - visible_width;
    }

    float total_text_width = GetTextWidth(GetDisplayText());
    float max_scroll = std::max(0.0f, total_text_width - visible_width);
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);
}

int InputBox::GetPreviousCharIndex(int index) const {
    if (index <= 0)
        return 0;
    int prev = index - 1;
    while (prev > 0 && IsContinuationByte(value_[prev])) {
        prev--;
    }
    return prev;
}

int InputBox::GetNextCharIndex(int index) const {
    int len = (int)value_.length();
    if (index >= len)
        return len;
    int next = index + 1;
    while (next < len && IsContinuationByte(value_[next])) {
        next++;
    }
    return next;
}

int InputBox::ByteIndexToCharIndex(int byte_idx) const {
    int char_idx = 0;
    int i = 0;
    int target = std::min(byte_idx, (int)value_.length());
    while (i < target) {
        i = GetNextCharIndex(i);
        char_idx++;
    }
    return char_idx;
}

int InputBox::CharIndexToByteIndex(int char_idx) const {
    int i = 0;
    int c = 0;
    int len = (int)value_.length();
    while (c < char_idx && i < len) {
        i = GetNextCharIndex(i);
        c++;
    }
    return i;
}

std::string InputBox::GetDisplayText() const {
    if (type_ == InputType::Password) {
        return std::string(ByteIndexToCharIndex((int)value_.length()), '*');
    }
    return value_;
}

void InputBox::HandleKeyPress(int key) {
    bool is_shift_down =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool is_ctrl_down = IsCtrlOrCmdDown();

    auto move_cursor = [&](int new_pos) {
        if (is_shift_down) {
            if (selection_anchor_ == -1) {
                selection_anchor_ = cursor_pos_;
            }
            cursor_pos_ = new_pos;
            select_start_ = std::min(selection_anchor_, cursor_pos_);
            select_end_ = std::max(selection_anchor_, cursor_pos_);
        } else {
            select_start_ = -1;
            select_end_ = -1;
            selection_anchor_ = -1;
            cursor_pos_ = new_pos;
        }
    };

    if (key == KEY_LEFT) {
        if (is_ctrl_down) {
            int next_pos = cursor_pos_;
            while (next_pos > 0) {
                int prev = GetPreviousCharIndex(next_pos);
                next_pos = prev;
                // Stop at word boundary start (space to non-space transition)
                if (next_pos == 0 || (value_[next_pos] == ' ' &&
                                      next_pos + 1 < (int)value_.length() &&
                                      value_[next_pos + 1] != ' ')) {
                    break;
                }
            }
            move_cursor(next_pos);
        } else {
            move_cursor(GetPreviousCharIndex(cursor_pos_));
        }
    } else if (key == KEY_RIGHT) {
        if (is_ctrl_down) {
            int next_pos = cursor_pos_;
            int len = (int)value_.length();
            while (next_pos < len) {
                int next = GetNextCharIndex(next_pos);
                next_pos = next;
                // Stop at word boundary end (non-space to space transition)
                if (next_pos == len ||
                    (value_[next_pos] == ' ' && next_pos > 0 &&
                     value_[next_pos - 1] != ' ')) {
                    break;
                }
            }
            move_cursor(next_pos);
        } else {
            move_cursor(GetNextCharIndex(cursor_pos_));
        }
    } else if (key == KEY_BACKSPACE) {
        if (readonly_)
            return;
        if (select_start_ != -1 && select_end_ != -1 &&
            select_start_ != select_end_) {
            DeleteSelection();
        } else if (cursor_pos_ > 0) {
            if (is_ctrl_down) {
                int next_pos = cursor_pos_;
                while (next_pos > 0) {
                    int prev = GetPreviousCharIndex(next_pos);
                    next_pos = prev;
                    if (next_pos == 0 || (value_[next_pos] == ' ' &&
                                          next_pos + 1 < (int)value_.length() &&
                                          value_[next_pos + 1] != ' ')) {
                        break;
                    }
                }
                value_.erase(next_pos, cursor_pos_ - next_pos);
                cursor_pos_ = next_pos;
            } else {
                int prev = GetPreviousCharIndex(cursor_pos_);
                value_.erase(prev, cursor_pos_ - prev);
                cursor_pos_ = prev;
            }
        }
    } else if (key == KEY_DELETE) {
        if (readonly_)
            return;
        if (select_start_ != -1 && select_end_ != -1 &&
            select_start_ != select_end_) {
            DeleteSelection();
        } else if (cursor_pos_ < (int)value_.length()) {
            if (is_ctrl_down) {
                int next_pos = cursor_pos_;
                int len = (int)value_.length();
                while (next_pos < len) {
                    int next = GetNextCharIndex(next_pos);
                    next_pos = next;
                    if (next_pos == len ||
                        (value_[next_pos] == ' ' && next_pos > 0 &&
                         value_[next_pos - 1] != ' ')) {
                        break;
                    }
                }
                value_.erase(cursor_pos_, next_pos - cursor_pos_);
            } else {
                int next = GetNextCharIndex(cursor_pos_);
                value_.erase(cursor_pos_, next - cursor_pos_);
            }
        }
    }
}

void InputBox::Update() {
    if (disabled_)
        return;

    Vector2 mouse_pos = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse_pos, bounds_);

    // Mouse focus and click
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hovered) {
            focused_ = true;

            // Multi-click detection
            float now = GetTime();
            if (now - last_click_time_ < 0.3f) {
                click_count_++;
            } else {
                click_count_ = 1;
            }
            last_click_time_ = now;

            int clicked_idx = GetCharIndexAtPos(mouse_pos.x);
            bool is_shift_down =
                IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

            if (click_count_ == 2) {
                SelectWordAt(clicked_idx);
            } else if (click_count_ >= 3) {
                select_start_ = 0;
                select_end_ = (int)value_.length();
                cursor_pos_ = select_end_;
                selection_anchor_ = 0;
            } else {
                if (is_shift_down) {
                    if (selection_anchor_ == -1) {
                        selection_anchor_ = cursor_pos_;
                    }
                    cursor_pos_ = clicked_idx;
                    select_start_ = std::min(selection_anchor_, cursor_pos_);
                    select_end_ = std::max(selection_anchor_, cursor_pos_);
                } else {
                    select_start_ = -1;
                    select_end_ = -1;
                    selection_anchor_ = -1;
                    cursor_pos_ = clicked_idx;
                    is_dragging_ = true;
                    selection_anchor_ = clicked_idx;
                }
            }
        } else {
            focused_ = false;
            is_dragging_ = false;
        }
    }

    if (focused_) {
        // Drag selecting
        if (is_dragging_) {
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                is_dragging_ = false;
                if (select_start_ == select_end_) {
                    select_start_ = -1;
                    select_end_ = -1;
                    selection_anchor_ = -1;
                }
            } else {
                float viewport_x = bounds_.x + style_.paddingLeft;
                float viewport_w =
                    bounds_.width - style_.paddingLeft - style_.paddingRight;

                // Drag auto-scroll if mouse goes past horizontally
                if (mouse_pos.x < viewport_x) {
                    scroll_offset_ -= 200.0f * GetFrameTime();
                } else if (mouse_pos.x > viewport_x + viewport_w) {
                    scroll_offset_ += 200.0f * GetFrameTime();
                }

                float display_w = GetTextWidth(GetDisplayText());
                float max_scroll = std::max(0.0f, display_w - viewport_w);
                scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);

                int drag_idx = GetCharIndexAtPos(mouse_pos.x);
                cursor_pos_ = drag_idx;
                select_start_ = std::min(selection_anchor_, cursor_pos_);
                select_end_ = std::max(selection_anchor_, cursor_pos_);
            }
        }

        bool is_ctrl_down = IsCtrlOrCmdDown();
        bool is_shift_down =
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        auto move_cursor = [&](int new_pos) {
            if (is_shift_down) {
                if (selection_anchor_ == -1) {
                    selection_anchor_ = cursor_pos_;
                }
                cursor_pos_ = new_pos;
                select_start_ = std::min(selection_anchor_, cursor_pos_);
                select_end_ = std::max(selection_anchor_, cursor_pos_);
            } else {
                select_start_ = -1;
                select_end_ = -1;
                selection_anchor_ = -1;
                cursor_pos_ = new_pos;
            }
        };

        // Handle repeating keys (Backspace, Delete, Left, Right)
        int repeat_keys[] = {KEY_BACKSPACE, KEY_DELETE, KEY_LEFT, KEY_RIGHT};
        int pressed_repeat_key = 0;
        for (int k : repeat_keys) {
            if (IsKeyPressed(k)) {
                pressed_repeat_key = k;
                break;
            }
        }

        if (pressed_repeat_key != 0) {
            active_repeat_key_ = pressed_repeat_key;
            repeat_timer_ = 0.40f; // delay before repeat starts
            HandleKeyPress(active_repeat_key_);
        } else if (active_repeat_key_ != 0) {
            if (IsKeyDown(active_repeat_key_)) {
                repeat_timer_ -= GetFrameTime();
                if (repeat_timer_ <= 0.0f) {
                    repeat_timer_ = 0.04f; // repeat speed
                    HandleKeyPress(active_repeat_key_);
                }
            } else {
                active_repeat_key_ = 0;
            }
        }

        // Keyboard Home / End navigation
        if (IsKeyPressed(KEY_HOME)) {
            move_cursor(0);
        }
        if (IsKeyPressed(KEY_END)) {
            move_cursor((int)value_.length());
        }

        // Command Shortcuts
        if (is_ctrl_down) {
            if (IsKeyPressed(KEY_A)) {
                select_start_ = 0;
                select_end_ = (int)value_.length();
                cursor_pos_ = select_end_;
                selection_anchor_ = 0;
            } else if (IsKeyPressed(KEY_C)) {
                HandleClipboardCopy(false);
            } else if (IsKeyPressed(KEY_X)) {
                if (!readonly_)
                    HandleClipboardCopy(true);
            } else if (IsKeyPressed(KEY_V)) {
                if (!readonly_)
                    HandleClipboardPaste();
            }
        }

        // Keyboard typing input
        int char_code = GetCharPressed();
        std::string typed;
        while (char_code > 0) {
            if (char_code >= 32 && char_code != 127) {
                // Encode UTF-8
                if (char_code < 0x80) {
                    typed.push_back(static_cast<char>(char_code));
                } else if (char_code < 0x800) {
                    typed.push_back(static_cast<char>(0xC0 | (char_code >> 6)));
                    typed.push_back(
                        static_cast<char>(0x80 | (char_code & 0x3F)));
                } else if (char_code < 0x10000) {
                    typed.push_back(
                        static_cast<char>(0xE0 | (char_code >> 12)));
                    typed.push_back(
                        static_cast<char>(0x80 | ((char_code >> 6) & 0x3F)));
                    typed.push_back(
                        static_cast<char>(0x80 | (char_code & 0x3F)));
                } else {
                    typed.push_back(
                        static_cast<char>(0xF0 | (char_code >> 18)));
                    typed.push_back(
                        static_cast<char>(0x80 | ((char_code >> 12) & 0x3F)));
                    typed.push_back(
                        static_cast<char>(0x80 | ((char_code >> 6) & 0x3F)));
                    typed.push_back(
                        static_cast<char>(0x80 | (char_code & 0x3F)));
                }
            }
            char_code = GetCharPressed();
        }

        if (!typed.empty() && !readonly_) {
            InsertText(typed);
        }

        ScrollToCursor();
    }
}

void InputBox::Draw(Camera2D camera) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds_);

    // Pick styles based on current state
    Color bg = style_.bgNormal;
    Color border = style_.borderNormal;
    Color text_color = style_.textNormal;

    if (disabled_) {
        bg = style_.bgDisabled;
        border = style_.borderDisabled;
        text_color = style_.textDisabled;
    } else if (focused_) {
        bg = style_.bgActive;
        border = style_.borderActive;
    } else if (hovered) {
        bg = style_.bgHover;
        border = style_.borderHover;
    }

    // Draw background & border (square corners only)
    DrawRectangleRec(bounds_, bg);
    DrawRectangleLinesEx(bounds_, style_.borderWidth, border);

    // Calculate visible viewport inside padding
    Rectangle viewport = {
        bounds_.x + style_.paddingLeft, bounds_.y + style_.paddingTop,
        bounds_.width - style_.paddingLeft - style_.paddingRight,
        bounds_.height - style_.paddingTop - style_.paddingBottom};

    // Clip text content so it doesn't leak outside viewport boundaries. The
    // scissor is in real framebuffer pixels and ignores the active Camera2D, so
    // map the viewport corners through the camera first.
    Vector2 clip_tl = GetWorldToScreen2D({viewport.x, viewport.y}, camera);
    Vector2 clip_br = GetWorldToScreen2D(
        {viewport.x + viewport.width, viewport.y + viewport.height}, camera);
    BeginScissorMode((int)clip_tl.x, (int)clip_tl.y,
                     (int)(clip_br.x - clip_tl.x),
                     (int)(clip_br.y - clip_tl.y));

    std::string display_text = GetDisplayText();
    bool show_placeholder = value_.empty() && !focused_;

    if (show_placeholder) {
        if (style_.font.texture.id == 0) {
            DrawText(placeholder_.c_str(), (int)viewport.x, (int)viewport.y,
                     (int)style_.fontSize, style_.textPlaceholder);
        } else {
            DrawTextEx(style_.font, placeholder_.c_str(),
                       {viewport.x, viewport.y}, style_.fontSize,
                       style_.fontSpacing, style_.textPlaceholder);
        }
    } else {
        // Draw selection highlight if any
        if (focused_ && select_start_ != -1 && select_end_ != -1 &&
            select_start_ != select_end_) {
            float sel_x1 = viewport.x +
                           GetTextWidthUpTo(display_text, select_start_) -
                           scroll_offset_;
            float sel_x2 = viewport.x +
                           GetTextWidthUpTo(display_text, select_end_) -
                           scroll_offset_;

            float x1 = std::max(viewport.x, sel_x1);
            float x2 = std::min(viewport.x + viewport.width, sel_x2);
            if (x2 > x1) {
                DrawRectangle((int)x1, (int)viewport.y, (int)(x2 - x1),
                              (int)viewport.height, style_.selectionBg);
            }
        }

        // Draw input text
        Vector2 text_pos = {viewport.x - scroll_offset_, viewport.y};
        if (style_.font.texture.id == 0) {
            DrawText(display_text.c_str(), (int)text_pos.x, (int)text_pos.y,
                     (int)style_.fontSize, text_color);
        } else {
            DrawTextEx(style_.font, display_text.c_str(), text_pos,
                       style_.fontSize, style_.fontSpacing, text_color);
        }

        // Draw blinking cursor
        if (focused_ && !readonly_) {
            // Toggle every 0.5s
            if (((int)(GetTime() * 2.0f) % 2) == 0) {
                float cursor_x = viewport.x +
                                 GetTextWidthUpTo(display_text, cursor_pos_) -
                                 scroll_offset_;
                if (cursor_x >= viewport.x &&
                    cursor_x <= viewport.x + viewport.width) {
                    DrawRectangle((int)cursor_x, (int)viewport.y, 2,
                                  (int)viewport.height, style_.cursorColor);
                }
            }
        }
    }

    EndScissorMode();
}

} // namespace Synera::gui
