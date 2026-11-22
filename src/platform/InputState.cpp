#include <tuneui/platform/InputState.h>

namespace tuneui {

namespace {

std::size_t mouseIndex(MouseButton button) {
    return static_cast<std::size_t>(button);
}

bool keyInRange(int key) {
    return key >= 0 && static_cast<std::size_t>(key) < KeyTableSize;
}

// Encode a Unicode codepoint as UTF-8 and append it to `out`.
void appendUtf8(std::string& out, unsigned int cp) {
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return; // reject surrogates and out-of-range values
    }
    if (cp < 0x80u) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

} // namespace

void InputState::onMouseMove(double x, double y) {
    // The first position event (and the first after re-entering the window)
    // has no meaningful previous point; report a zero delta instead of a jump
    // from (0,0) or from the old cursor position.
    if (!hasMousePos_ || suppressNextDelta_) {
        hasMousePos_ = true;
        suppressNextDelta_ = false;
    } else {
        mouseDeltaX_ += x - lastMouseX_;
        mouseDeltaY_ += y - lastMouseY_;
    }
    lastMouseX_ = x;
    lastMouseY_ = y;
    mouseX_ = x;
    mouseY_ = y;
}

void InputState::onMouseButton(MouseButton button, bool down, unsigned mods) {
    modifiers_ = mods;
    const auto i = mouseIndex(button);
    if (i >= MouseButtonCount) {
        return;
    }
    if (down) {
        if (!mouseDown_[i]) {
            mousePressed_[i] = true;
        }
        mouseDown_[i] = true;
    } else {
        if (mouseDown_[i]) {
            mouseReleased_[i] = true;
        }
        mouseDown_[i] = false;
    }
}

void InputState::onKey(int key, bool down, unsigned mods, bool repeat) {
    modifiers_ = mods;
    if (!keyInRange(key)) {
        return;
    }
    const auto i = static_cast<std::size_t>(key);
    if (repeat) {
        keyRepeated_[i] = true;
        keyDown_[i] = true;
        return;
    }
    if (down) {
        if (!keyDown_[i]) {
            keyPressed_[i] = true;
        }
        keyDown_[i] = true;
    } else {
        if (keyDown_[i]) {
            keyReleased_[i] = true;
        }
        keyDown_[i] = false;
    }
}

void InputState::onFocusLost() {
    for (std::size_t i = 0; i < MouseButtonCount; ++i) {
        if (mouseDown_[i]) {
            mouseReleased_[i] = true;
            mouseDown_[i] = false;
        }
        mousePressed_[i] = false;
    }
    for (std::size_t i = 0; i < KeyTableSize; ++i) {
        if (keyDown_[i]) {
            keyReleased_[i] = true;
            keyDown_[i] = false;
        }
        keyPressed_[i] = false;
        keyRepeated_[i] = false;
    }

    mouseDeltaX_ = 0.0;
    mouseDeltaY_ = 0.0;
    scrollX_ = 0.0;
    scrollY_ = 0.0;
    textInput_.clear();
    modifiers_ = 0;
    suppressNextDelta_ = true;
    focusLost_ = true;
}

void InputState::onCursorEnter(bool entered) {
    cursorInside_ = entered;
    if (entered) {
        suppressNextDelta_ = true;
    }
}

void InputState::onScroll(double dx, double dy) {
    scrollX_ += dx;
    scrollY_ += dy;
}

void InputState::onChar(unsigned int codepoint) {
    appendUtf8(textInput_, codepoint);
}

void InputState::endFrame() {
    mouseDeltaX_ = 0.0;
    mouseDeltaY_ = 0.0;
    scrollX_ = 0.0;
    scrollY_ = 0.0;

    mousePressed_.fill(false);
    mouseReleased_.fill(false);
    keyPressed_.fill(false);
    keyReleased_.fill(false);
    keyRepeated_.fill(false);

    focusLost_ = false;
    textInput_.clear();
}

bool InputState::mouseDown(MouseButton button) const {
    const auto i = mouseIndex(button);
    return i < MouseButtonCount && mouseDown_[i];
}

bool InputState::mousePressed(MouseButton button) const {
    const auto i = mouseIndex(button);
    return i < MouseButtonCount && mousePressed_[i];
}

bool InputState::mouseReleased(MouseButton button) const {
    const auto i = mouseIndex(button);
    return i < MouseButtonCount && mouseReleased_[i];
}

bool InputState::keyDown(int key) const {
    return keyInRange(key) && keyDown_[static_cast<std::size_t>(key)];
}

bool InputState::keyPressed(int key) const {
    return keyInRange(key) && keyPressed_[static_cast<std::size_t>(key)];
}

bool InputState::keyReleased(int key) const {
    return keyInRange(key) && keyReleased_[static_cast<std::size_t>(key)];
}

bool InputState::keyRepeated(int key) const {
    return keyInRange(key) && keyRepeated_[static_cast<std::size_t>(key)];
}

} // namespace tuneui
