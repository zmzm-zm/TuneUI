// Pure (no window, no GL) GoogleTest unit tests for the layers Stage 2 builds on.
// Registered with CTest as individual tests via gtest_discover_tests().

#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include <tuneui/platform/InputState.h>
#include <tuneui/render/Math.h>

namespace tuneui {
namespace {

constexpr int KeyA = 65; // GLFW_KEY_A

using ModifierMask = unsigned;

ModifierMask mask(Modifier bit) {
    return static_cast<ModifierMask>(bit);
}

} // namespace

// ---------------------------------------------------------------------------
// InputState
// ---------------------------------------------------------------------------

TEST(InputState, InitialStateIsEmpty) {
    InputState input;

    EXPECT_DOUBLE_EQ(input.mouseX(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseY(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 0.0);
    EXPECT_DOUBLE_EQ(input.scrollX(), 0.0);
    EXPECT_DOUBLE_EQ(input.scrollY(), 0.0);
    EXPECT_TRUE(input.textInput().empty());

    EXPECT_FALSE(input.mouseDown(MouseButton::Left));
    EXPECT_FALSE(input.mousePressed(MouseButton::Left));
    EXPECT_FALSE(input.mouseReleased(MouseButton::Left));
    EXPECT_FALSE(input.keyDown(KeyA));
    EXPECT_FALSE(input.keyPressed(KeyA));
    EXPECT_FALSE(input.keyReleased(KeyA));
    EXPECT_FALSE(input.keyRepeated(KeyA));

    EXPECT_EQ(input.modifiers(), 0u);
    EXPECT_FALSE(input.focusLost());
    EXPECT_FALSE(input.cursorInside());
}

TEST(InputState, FirstMousePositionHasZeroDelta) {
    InputState input;

    input.onMouseMove(10.0, 20.0);
    EXPECT_DOUBLE_EQ(input.mouseX(), 10.0);
    EXPECT_DOUBLE_EQ(input.mouseY(), 20.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 0.0);

    // Moves within one frame accumulate into one delta.
    input.onMouseMove(15.0, 18.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 5.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), -2.0);
}

TEST(InputState, EndFrameClearsDeltaButKeepsPosition) {
    InputState input;
    input.onMouseMove(10.0, 20.0);
    input.onMouseMove(15.0, 18.0);

    input.endFrame();
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseX(), 15.0);
    EXPECT_DOUBLE_EQ(input.mouseY(), 18.0);

    input.onMouseMove(12.0, 21.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), -3.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 3.0);
}

TEST(InputState, CursorEnterSuppressesTheJumpDelta) {
    InputState input;
    input.onMouseMove(10.0, 20.0);
    input.onMouseMove(15.0, 18.0);
    input.endFrame();

    input.onCursorEnter(false);
    EXPECT_FALSE(input.cursorInside());
    input.onCursorEnter(true);
    EXPECT_TRUE(input.cursorInside());

    input.onMouseMove(200.0, 200.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 0.0);

    input.onMouseMove(205.0, 201.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 5.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 1.0);
}

TEST(InputState, MouseButtonEdgesAndInvalidButtons) {
    InputState input;
    const auto left = MouseButton::Left;

    input.onMouseButton(left, true);
    EXPECT_TRUE(input.mouseDown(left));
    EXPECT_TRUE(input.mousePressed(left));
    EXPECT_FALSE(input.mouseReleased(left));

    // A repeated press while already held must not add another edge.
    input.onMouseButton(left, true);
    EXPECT_TRUE(input.mousePressed(left));

    input.endFrame();
    EXPECT_TRUE(input.mouseDown(left));
    EXPECT_FALSE(input.mousePressed(left));
    EXPECT_FALSE(input.mouseReleased(left));

    input.onMouseButton(left, false);
    EXPECT_FALSE(input.mouseDown(left));
    EXPECT_TRUE(input.mouseReleased(left));
    EXPECT_FALSE(input.mousePressed(left));

    input.endFrame();
    EXPECT_FALSE(input.mouseReleased(left));

    // Buttons are independent.
    input.onMouseButton(MouseButton::Right, true);
    EXPECT_TRUE(input.mouseDown(MouseButton::Right));
    EXPECT_FALSE(input.mouseDown(left));
    input.onMouseButton(MouseButton::Right, false);

    // Out-of-range button values are ignored rather than writing out of bounds.
    input.onMouseButton(static_cast<MouseButton>(42), true);
    EXPECT_FALSE(input.mouseDown(static_cast<MouseButton>(42)));
    EXPECT_EQ(static_cast<int>(MouseButton::Count), 8);
    EXPECT_EQ(MouseButtonCount, 8u);
}

TEST(InputState, KeyEdgesAndOutOfRangeKeys) {
    InputState input;

    input.onKey(KeyA, true);
    EXPECT_TRUE(input.keyDown(KeyA));
    EXPECT_TRUE(input.keyPressed(KeyA));
    EXPECT_FALSE(input.keyReleased(KeyA));

    input.endFrame();
    EXPECT_TRUE(input.keyDown(KeyA));
    EXPECT_FALSE(input.keyPressed(KeyA));

    input.onKey(KeyA, false);
    EXPECT_FALSE(input.keyDown(KeyA));
    EXPECT_TRUE(input.keyReleased(KeyA));

    input.endFrame();
    EXPECT_FALSE(input.keyReleased(KeyA));

    // Independent keys.
    input.onKey(32 /* space */, true);
    EXPECT_TRUE(input.keyDown(32));
    EXPECT_FALSE(input.keyDown(KeyA));
    input.onKey(32, false);

    // Out-of-range values are ignored (table has KeyTableSize entries).
    input.onKey(-1, true);
    input.onKey(9999, true);
    EXPECT_FALSE(input.keyDown(-1));
    EXPECT_FALSE(input.keyDown(9999));
    EXPECT_GT(KeyTableSize, 348u); // must cover GLFW_KEY_LAST
}

TEST(InputState, KeyRepeatIsNotAPressEdge) {
    InputState input;

    input.onKey(KeyA, true, 0, false);
    EXPECT_TRUE(input.keyDown(KeyA));
    EXPECT_TRUE(input.keyPressed(KeyA));
    EXPECT_FALSE(input.keyRepeated(KeyA));

    // Consume the press edge first; a repeat must not recreate it.
    input.endFrame();
    input.onKey(KeyA, true, 0, true);
    EXPECT_TRUE(input.keyDown(KeyA));
    EXPECT_FALSE(input.keyPressed(KeyA));
    EXPECT_TRUE(input.keyRepeated(KeyA));

    input.endFrame();
    EXPECT_TRUE(input.keyDown(KeyA));
    EXPECT_FALSE(input.keyRepeated(KeyA));

    input.onKey(KeyA, false, 0, false);
    EXPECT_FALSE(input.keyDown(KeyA));
    EXPECT_TRUE(input.keyReleased(KeyA));
}

TEST(InputState, ModifiersFollowTheLatestEvent) {
    InputState input;

    input.onKey(KeyA, true, mask(Modifier::Shift), false);
    EXPECT_TRUE(input.modifier(Modifier::Shift));
    EXPECT_FALSE(input.modifier(Modifier::Control));

    input.onMouseButton(MouseButton::Left, true, mask(Modifier::Control));
    EXPECT_TRUE(input.modifier(Modifier::Control));
    EXPECT_FALSE(input.modifier(Modifier::Shift));

    input.setModifiers(mask(Modifier::Alt) | mask(Modifier::Super));
    EXPECT_TRUE(input.modifier(Modifier::Alt));
    EXPECT_TRUE(input.modifier(Modifier::Super));
    EXPECT_EQ(input.modifiers(), mask(Modifier::Alt) | mask(Modifier::Super));
}

TEST(InputState, FocusLossReleasesHeldInputAndClearsTransientState) {
    InputState input;
    const auto left = MouseButton::Left;

    input.onKey(KeyA, true, mask(Modifier::Shift), false);
    input.onMouseButton(left, true, mask(Modifier::Control));
    input.onChar(0x4E2D);
    input.onScroll(1.5, -2.0);

    input.onFocusLost();
    EXPECT_TRUE(input.focusLost());
    EXPECT_FALSE(input.keyDown(KeyA));
    EXPECT_TRUE(input.keyReleased(KeyA));
    EXPECT_FALSE(input.mouseDown(left));
    EXPECT_TRUE(input.mouseReleased(left));
    EXPECT_EQ(input.modifiers(), 0u);
    EXPECT_TRUE(input.textInput().empty());
    EXPECT_DOUBLE_EQ(input.scrollX(), 0.0);
    EXPECT_DOUBLE_EQ(input.scrollY(), 0.0);

    input.endFrame();
    EXPECT_FALSE(input.focusLost());

    // The first move after focus loss is a delta reset, not a jump.
    input.onMouseMove(300.0, 300.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaX(), 0.0);
    EXPECT_DOUBLE_EQ(input.mouseDeltaY(), 0.0);
}

TEST(InputState, TextInputEncodesUtf8AndRejectsInvalidCodepoints) {
    InputState input;

    input.onChar('A');
    input.onChar(0xD800);   // lone surrogate: rejected
    input.onChar(0x110000); // beyond Unicode: rejected
    input.onChar(0x00E9);   // é -> C3 A9
    input.onChar(0x4E2D);   // 中 -> E4 B8 AD
    input.onChar(0x1F600);  // 😀 -> F0 9F 98 80

    EXPECT_EQ(input.textInput(),
              std::string("A\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"));

    input.endFrame();
    EXPECT_TRUE(input.textInput().empty());
}

TEST(InputState, ScrollAccumulatesAndResets) {
    InputState input;

    input.onScroll(1.0, 2.0);
    input.onScroll(0.5, -1.0);
    EXPECT_DOUBLE_EQ(input.scrollX(), 1.5);
    EXPECT_DOUBLE_EQ(input.scrollY(), 1.0);

    input.endFrame();
    EXPECT_DOUBLE_EQ(input.scrollX(), 0.0);
    EXPECT_DOUBLE_EQ(input.scrollY(), 0.0);
}

// ---------------------------------------------------------------------------
// Math
// ---------------------------------------------------------------------------

TEST(MathRect, NormalizeRectFlipsNegativeSizes) {
    const Rect normalized =
        normalizeRect({110.0f, 60.0f, -100.0f, -50.0f});
    EXPECT_FLOAT_EQ(normalized.x, 10.0f);
    EXPECT_FLOAT_EQ(normalized.y, 10.0f);
    EXPECT_FLOAT_EQ(normalized.width, 100.0f);
    EXPECT_FLOAT_EQ(normalized.height, 50.0f);
}

TEST(MathRect, IntersectionAndDisjointCases) {
    const Rect a{10.0f, 10.0f, 100.0f, 50.0f};

    Rect out{};
    ASSERT_TRUE(intersectRects(a, {50.0f, 0.0f, 100.0f, 100.0f}, out));
    EXPECT_FLOAT_EQ(out.x, 50.0f);
    EXPECT_FLOAT_EQ(out.y, 10.0f);
    EXPECT_FLOAT_EQ(out.width, 60.0f);
    EXPECT_FLOAT_EQ(out.height, 50.0f);

    EXPECT_FALSE(intersectRects(a, {200.0f, 200.0f, 10.0f, 10.0f}, out));
    EXPECT_FALSE(intersectRects(a, {a.x + a.width, 10.0f, 1.0f, 1.0f}, out));
    EXPECT_FALSE(intersectRects({0.0f, 0.0f, NAN, 10.0f}, a, out));

    // Negative sizes are normalized before intersecting.
    ASSERT_TRUE(intersectRects(a, {150.0f, 60.0f, -100.0f, -50.0f}, out));
    EXPECT_FLOAT_EQ(out.x, 50.0f);
    EXPECT_FLOAT_EQ(out.y, 10.0f);
    EXPECT_FLOAT_EQ(out.width, 60.0f);
    EXPECT_FLOAT_EQ(out.height, 50.0f);
}

TEST(MathRect, ContainsUsesOuterAndInnerInTheRightOrder) {
    const Rect outer{10.0f, 10.0f, 100.0f, 50.0f};
    const Rect inner{20.0f, 20.0f, 10.0f, 10.0f};

    EXPECT_TRUE(rectContains(outer, inner));
    EXPECT_FALSE(rectContains(inner, outer));
    EXPECT_FALSE(rectContains(outer, {5.0f, 20.0f, 10.0f, 10.0f}));
    EXPECT_FALSE(rectContains({0.0f, 0.0f, NAN, 10.0f}, inner));

    // Equal rects contain each other; this is the "clip covers the page" case.
    EXPECT_TRUE(rectContains(outer, outer));
}

TEST(MathRect, EqualityEmptinessAndFiniteChecks) {
    const Rect a{10.0f, 10.0f, 100.0f, 50.0f};
    const Rect normalized = normalizeRect({110.0f, 60.0f, -100.0f, -50.0f});

    EXPECT_TRUE(rectsEqual(a, {10.0f, 10.0f, 100.0f, 50.0f}));
    EXPECT_TRUE(rectsEqual(a, normalized));
    EXPECT_TRUE(rectEmpty({0.0f, 0.0f, 0.0f, 10.0f}));
    EXPECT_TRUE(rectEmpty({0.0f, 0.0f, 10.0f, 0.0f}));
    EXPECT_FALSE(rectEmpty(a));
    EXPECT_TRUE(isFiniteRect(a));
    EXPECT_FALSE(isFiniteRect({0.0f, 0.0f, INFINITY, 1.0f}));
    EXPECT_FALSE(isFiniteRect({NAN, 0.0f, 1.0f, 1.0f}));
}

TEST(MathRect, OrthoPixelsMapsTopLeftAndBottomRight) {
    const glm::mat4 matrix = orthoPixels(800.0f, 600.0f);

    const glm::vec4 topLeft = matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 bottomRight = matrix * glm::vec4(800.0f, 600.0f, 0.0f, 1.0f);
    const glm::vec4 center = matrix * glm::vec4(400.0f, 300.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(topLeft.x, -1.0f);
    EXPECT_FLOAT_EQ(topLeft.y, 1.0f);
    EXPECT_FLOAT_EQ(bottomRight.x, 1.0f);
    EXPECT_FLOAT_EQ(bottomRight.y, -1.0f);
    EXPECT_NEAR(center.x, 0.0f, 1e-5f);
    EXPECT_NEAR(center.y, 0.0f, 1e-5f);

    // dpiScale = 2: a 1600x1200 framebuffer is an 800x600 logical canvas.
    const glm::mat4 dpi = orthoPixels(1600.0f, 1200.0f, 2.0f);
    const glm::vec4 dpiTopLeft = dpi * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 dpiBottomRight =
        dpi * glm::vec4(800.0f, 600.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(dpiTopLeft.x, -1.0f);
    EXPECT_FLOAT_EQ(dpiTopLeft.y, 1.0f);
    EXPECT_FLOAT_EQ(dpiBottomRight.x, 1.0f);
    EXPECT_FLOAT_EQ(dpiBottomRight.y, -1.0f);

    // Degenerate sizes fall back to identity instead of producing NaNs.
    EXPECT_EQ(orthoPixels(0.0f, 600.0f), glm::mat4(1.0f));
    EXPECT_EQ(orthoPixels(800.0f, 600.0f, 0.0f), glm::mat4(1.0f));
}

} // namespace tuneui
