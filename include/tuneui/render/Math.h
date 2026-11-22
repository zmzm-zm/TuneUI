#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace tuneui {

using Vec2 = glm::vec2;

// Straight (non-premultiplied) RGBA color, components in [0, 1].
// White is Color(1.0f). glm::vec4 supplies .r / .g / .b / .a accessors.
using Color = glm::vec4;

// Axis-aligned rectangle in logical (UI) pixels, origin at the top-left.
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Returns the same rectangle with non-negative width/height. A negative size
// means "extend left/up from x/y"; callers should normalize before comparing.
inline Rect normalizeRect(const Rect& rect) {
    Rect out;
    out.x = std::min(rect.x, rect.x + rect.width);
    out.y = std::min(rect.y, rect.y + rect.height);
    out.width = std::fabs(rect.width);
    out.height = std::fabs(rect.height);
    return out;
}

inline bool isFiniteRect(const Rect& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.width) && std::isfinite(rect.height);
}

inline bool rectEmpty(const Rect& rect) {
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

// Intersects two rectangles after normalizing negative sizes. The result is
// normalized. Returns false when the intersection is empty or either input is
// non-finite (NaN/Inf would otherwise poison layout math).
inline bool intersectRects(const Rect& a, const Rect& b, Rect& out) {
    if (!isFiniteRect(a) || !isFiniteRect(b)) {
        return false;
    }
    const Rect na = normalizeRect(a);
    const Rect nb = normalizeRect(b);
    const float x0 = std::max(na.x, nb.x);
    const float y0 = std::max(na.y, nb.y);
    const float x1 = std::min(na.x + na.width, nb.x + nb.width);
    const float y1 = std::min(na.y + na.height, nb.y + nb.height);
    if (x1 <= x0 || y1 <= y0) {
        return false;
    }
    out = Rect{x0, y0, x1 - x0, y1 - y0};
    return true;
}

// True when `inner` lies completely inside `outer`. Both inputs are normalized
// first, matching the rest of the Rect helpers.
inline bool rectContains(const Rect& outer, const Rect& inner) {
    if (!isFiniteRect(outer) || !isFiniteRect(inner)) {
        return false;
    }
    const Rect no = normalizeRect(outer);
    const Rect ni = normalizeRect(inner);
    return ni.x >= no.x && ni.y >= no.y &&
           ni.x + ni.width <= no.x + no.width &&
           ni.y + ni.height <= no.y + no.height;
}

inline bool rectsEqual(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

// Maps logical pixel coordinates (origin top-left, +y down) to clip space.
//
// framebufferWidth/Height are physical pixels (what Window reports);
// dpiScale converts logical units to physical ones (1.0 = no scaling).
// For example with a 1600x1200 framebuffer and dpiScale 2.0, the logical
// canvas is 800x600, and logical (0,0) maps to NDC (-1, +1).
//
// Implemented as glm::ortho with a flipped bottom/top pair, so no extra
// matrix multiply or hand-rolled matrix layout is involved.
inline glm::mat4 orthoPixels(float framebufferWidth, float framebufferHeight,
                             float dpiScale = 1.0f) {
    if (framebufferWidth <= 0.0f || framebufferHeight <= 0.0f || dpiScale <= 0.0f) {
        return glm::mat4(1.0f);
    }

    const float logicalWidth = framebufferWidth / dpiScale;
    const float logicalHeight = framebufferHeight / dpiScale;

    // left, right, bottom, top: bottom > top flips the y axis so that
    // logical y = 0 lands at the top of the screen.
    // near/far = -1/1 keeps z = 0 inside the clip range (depth test is off).
    return glm::ortho(0.0f, logicalWidth, logicalHeight, 0.0f, -1.0f, 1.0f);
}

} // namespace tuneui
