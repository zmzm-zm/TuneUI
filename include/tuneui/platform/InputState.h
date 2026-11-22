#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace tuneui {

// Mouse buttons follow GLFW's GLFW_MOUSE_BUTTON_* ordering (0..7), so the
// Window layer can forward events with a plain cast and no remap table.
enum class MouseButton : int {
    Left    = 0,
    Right   = 1,
    Middle  = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,

    Count = 8,
};

inline constexpr std::size_t MouseButtonCount =
    static_cast<std::size_t>(MouseButton::Count);

// Key codes follow GLFW's GLFW_KEY_* values. They are stored as plain ints so
// this header carries no GLFW dependency; the table is sized generously
// enough to cover every GLFW key (GLFW_KEY_LAST == 348).
inline constexpr std::size_t KeyTableSize = 512;

// Modifier bits. The values intentionally match GLFW_MOD_* so Window can pass
// the callback's `mods` argument through without a mapping table.
enum class Modifier : unsigned {
    Shift = 0x0001,
    Control = 0x0002,
    Alt = 0x0004,
    Super = 0x0008,
    CapsLock = 0x0010,
    NumLock = 0x0020,
};

// Holds the *current frame's* input state. Window's GLFW callbacks feed it
// via the on*() methods; the application reads it each frame and calls
// endFrame() once per frame to clear the one-shot (edge / delta / text) data.
class InputState {
public:
    InputState() = default;

    // ---- Event entry points (called by Window's GLFW callbacks) ----------

    void onMouseMove(double x, double y);
    void onMouseButton(MouseButton button, bool down, unsigned mods = 0);
    // repeat=true records a key-repeat event without producing a press edge.
    void onKey(int key, bool down, unsigned mods = 0, bool repeat = false);
    void onScroll(double dx, double dy);
    void onChar(unsigned int codepoint);

    // Losing focus synthesizes releases for everything held, clears text and
    // modifiers, and suppresses the next mouse delta (the cursor may jump).
    void onFocusLost();
    void onCursorEnter(bool entered);

    void setModifiers(unsigned mods) { modifiers_ = mods; }

    // ---- Frame lifecycle --------------------------------------------------

    // Clears one-shot state accumulated this frame (pressed/released edges,
    // scroll delta, mouse delta, text input). Call once per frame, after the
    // application has consumed the input it cares about.
    void endFrame();

    // ---- Mouse queries ----------------------------------------------------

    double mouseX() const { return mouseX_; }
    double mouseY() const { return mouseY_; }
    // Movement since the previous frame (accumulated across move events).
    double mouseDeltaX() const { return mouseDeltaX_; }
    double mouseDeltaY() const { return mouseDeltaY_; }

    bool mouseDown(MouseButton button) const;
    bool mousePressed(MouseButton button) const;
    bool mouseReleased(MouseButton button) const;

    double scrollX() const { return scrollX_; }
    double scrollY() const { return scrollY_; }

    // ---- Keyboard queries -------------------------------------------------

    bool keyDown(int key) const;
    bool keyPressed(int key) const;
    bool keyReleased(int key) const;
    // True when the key received at least one repeat event this frame.
    bool keyRepeated(int key) const;

    // ---- Modifiers / focus / cursor ---------------------------------------

    unsigned modifiers() const { return modifiers_; }
    bool modifier(Modifier bit) const {
        return (modifiers_ & static_cast<unsigned>(bit)) != 0;
    }
    bool focusLost() const { return focusLost_; }
    bool cursorInside() const { return cursorInside_; }

    // ---- Text -------------------------------------------------------------

    // UTF-8 text produced by the character callback this frame.
    const std::string& textInput() const { return textInput_; }

private:
    double mouseX_ = 0.0;
    double mouseY_ = 0.0;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    double mouseDeltaX_ = 0.0;
    double mouseDeltaY_ = 0.0;
    bool hasMousePos_ = false;
    bool suppressNextDelta_ = false;

    double scrollX_ = 0.0;
    double scrollY_ = 0.0;

    std::array<bool, MouseButtonCount> mouseDown_{};
    std::array<bool, MouseButtonCount> mousePressed_{};
    std::array<bool, MouseButtonCount> mouseReleased_{};

    std::array<bool, KeyTableSize> keyDown_{};
    std::array<bool, KeyTableSize> keyPressed_{};
    std::array<bool, KeyTableSize> keyReleased_{};
    std::array<bool, KeyTableSize> keyRepeated_{};

    unsigned modifiers_ = 0;
    bool focusLost_ = false;
    bool cursorInside_ = false;

    std::string textInput_;
};

} // namespace tuneui
