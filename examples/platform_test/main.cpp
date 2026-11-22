// Platform-layer test / demo.
//
// Covers everything the first stage exposes:
//   InputState  - mouse, buttons, keyboard, scroll, UTF-8 text, frame reset
//   Window      - init/shutdown, GL 3.3 core context, poll/swap, framebuffer
//                 queries, title/vsync, resize hook, input passthrough
//
// Default run is automated: unit checks + a short windowed loop, then exit.
// Pass --live to keep the window open and watch input being reported live.
//
// Note: Window does not expose the native GLFW handle, so the resize callback
// can only be exercised by dragging the window edge (live mode prints it).

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <tuneui/platform/InputState.h>
#include <tuneui/platform/Window.h>

namespace {

int checkCount = 0;
int failureCount = 0;

void check(bool condition, const char* expression, const char* file, int line) {
    ++checkCount;
    if (!condition) {
        ++failureCount;
        std::printf("[FAIL] %s  (%s:%d)\n", expression, file, line);
    }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

void section(const char* name) {
    std::printf("\n== %s ==\n", name);
}

// ---------------------------------------------------------------------------
// InputState - no window or GL context required.
// ---------------------------------------------------------------------------

void testInputInitialState() {
    section("InputState: initial state");
    tuneui::InputState input;

    CHECK(input.mouseX() == 0.0);
    CHECK(input.mouseY() == 0.0);
    CHECK(input.mouseDeltaX() == 0.0);
    CHECK(input.mouseDeltaY() == 0.0);
    CHECK(input.scrollX() == 0.0);
    CHECK(input.scrollY() == 0.0);
    CHECK(input.textInput().empty());

    CHECK(!input.mouseDown(tuneui::MouseButton::Left));
    CHECK(!input.mousePressed(tuneui::MouseButton::Left));
    CHECK(!input.mouseReleased(tuneui::MouseButton::Left));
    CHECK(!input.keyDown(GLFW_KEY_A));
    CHECK(!input.keyPressed(GLFW_KEY_A));
    CHECK(!input.keyReleased(GLFW_KEY_A));
}

void testInputMouseMove() {
    section("InputState: mouse move + frame delta");
    tuneui::InputState input;

    input.onMouseMove(10.0, 20.0);
    CHECK(input.mouseX() == 10.0);
    CHECK(input.mouseY() == 20.0);
    CHECK(input.mouseDeltaX() == 0.0); // first position has no previous point
    CHECK(input.mouseDeltaY() == 0.0);

    // Moves in one frame accumulate into one delta.
    input.onMouseMove(15.0, 18.0);
    CHECK(input.mouseX() == 15.0);
    CHECK(input.mouseY() == 18.0);
    CHECK(input.mouseDeltaX() == 5.0);
    CHECK(input.mouseDeltaY() == -2.0);

    input.endFrame();
    CHECK(input.mouseDeltaX() == 0.0);
    CHECK(input.mouseDeltaY() == 0.0);
    CHECK(input.mouseX() == 15.0); // absolute position persists
    CHECK(input.mouseY() == 18.0);

    input.onMouseMove(12.0, 21.0);
    CHECK(input.mouseDeltaX() == -3.0);
    CHECK(input.mouseDeltaY() == 3.0);
}

void testInputMouseButtons() {
    section("InputState: mouse button edges");
    tuneui::InputState input;
    constexpr auto LeftButton = tuneui::MouseButton::Left;

    input.onMouseButton(LeftButton, true);
    CHECK(input.mouseDown(LeftButton));
    CHECK(input.mousePressed(LeftButton));
    CHECK(!input.mouseReleased(LeftButton));

    // A repeated press while already held must not produce a second edge.
    input.onMouseButton(LeftButton, true);
    CHECK(input.mousePressed(LeftButton));

    input.endFrame();
    CHECK(input.mouseDown(LeftButton));      // held state survives the frame
    CHECK(!input.mousePressed(LeftButton));  // edge does not
    CHECK(!input.mouseReleased(LeftButton));

    input.onMouseButton(LeftButton, false);
    CHECK(!input.mouseDown(LeftButton));
    CHECK(input.mouseReleased(LeftButton));
    CHECK(!input.mousePressed(LeftButton));

    input.endFrame();
    CHECK(!input.mouseReleased(LeftButton));

    // Buttons are independent.
    input.onMouseButton(tuneui::MouseButton::Right, true);
    CHECK(input.mouseDown(tuneui::MouseButton::Right));
    CHECK(!input.mouseDown(LeftButton));
    input.onMouseButton(tuneui::MouseButton::Right, false);

    // Out-of-range button values are ignored rather than writing out of bounds.
    input.onMouseButton(static_cast<tuneui::MouseButton>(42), true);
    CHECK(!input.mouseDown(static_cast<tuneui::MouseButton>(42)));

    CHECK(static_cast<int>(tuneui::MouseButton::Count) == 8);
    CHECK(tuneui::MouseButtonCount == 8);
}

void testInputKeyboard() {
    section("InputState: keyboard edges");
    tuneui::InputState input;

    input.onKey(GLFW_KEY_W, true);
    CHECK(input.keyDown(GLFW_KEY_W));
    CHECK(input.keyPressed(GLFW_KEY_W));
    CHECK(!input.keyReleased(GLFW_KEY_W));

    input.endFrame();
    CHECK(input.keyDown(GLFW_KEY_W));
    CHECK(!input.keyPressed(GLFW_KEY_W));

    input.onKey(GLFW_KEY_W, false);
    CHECK(!input.keyDown(GLFW_KEY_W));
    CHECK(input.keyReleased(GLFW_KEY_W));

    input.endFrame();
    CHECK(!input.keyReleased(GLFW_KEY_W));

    // Independent keys.
    input.onKey(GLFW_KEY_SPACE, true);
    CHECK(input.keyDown(GLFW_KEY_SPACE));
    CHECK(!input.keyDown(GLFW_KEY_W));
    input.onKey(GLFW_KEY_SPACE, false);

    // Out-of-range keys are ignored (table is KeyTableSize entries).
    input.onKey(-1, true);
    input.onKey(600, true);
    CHECK(!input.keyDown(-1));
    CHECK(!input.keyDown(600));
    CHECK(tuneui::KeyTableSize > 348); // must cover GLFW_KEY_LAST
}

void testInputFocusModifiersAndRepeat() {
    section("InputState: focus loss, modifiers, key repeat");
    tuneui::InputState input;
    const auto left = tuneui::MouseButton::Left;

    input.onKey(GLFW_KEY_A, true,
                static_cast<unsigned>(tuneui::Modifier::Shift), false);
    input.onMouseButton(left, true,
                        static_cast<unsigned>(tuneui::Modifier::Control));
    CHECK(input.modifier(tuneui::Modifier::Control));
    CHECK(!input.keyRepeated(GLFW_KEY_A));

    // Consume the press edge first; a repeat event must not synthesize it.
    input.endFrame();
    input.onKey(GLFW_KEY_A, true,
                static_cast<unsigned>(tuneui::Modifier::Control), true);
    CHECK(input.keyRepeated(GLFW_KEY_A));
    CHECK(!input.keyPressed(GLFW_KEY_A));

    input.onChar('A');
    input.onFocusLost();
    CHECK(input.focusLost());
    CHECK(!input.keyDown(GLFW_KEY_A));
    CHECK(input.keyReleased(GLFW_KEY_A));
    CHECK(!input.mouseDown(left));
    CHECK(input.mouseReleased(left));
    CHECK(input.modifiers() == 0);
    CHECK(input.textInput().empty());

    input.endFrame();
    CHECK(!input.focusLost());

    // Re-entering suppresses the first delta.
    input.onCursorEnter(true);
    CHECK(input.cursorInside());
    input.onMouseMove(200.0, 200.0);
    CHECK(input.mouseDeltaX() == 0.0);
    CHECK(input.mouseDeltaY() == 0.0);
}

void testInputScroll() {
    section("InputState: scroll accumulation");
    tuneui::InputState input;

    input.onScroll(1.0, 2.0);
    input.onScroll(0.5, -1.0);
    CHECK(input.scrollX() == 1.5);
    CHECK(input.scrollY() == 1.0);

    input.endFrame();
    CHECK(input.scrollX() == 0.0);
    CHECK(input.scrollY() == 0.0);
}

void testInputText() {
    section("InputState: UTF-8 text input");
    tuneui::InputState input;

    input.onChar('A');
    CHECK(input.textInput() == "A");

    input.onChar(0x00E9); // é -> C3 A9
    CHECK(input.textInput() == std::string("A\xC3\xA9"));

    input.onChar(0x4E2D); // 中 -> E4 B8 AD
    CHECK(input.textInput() == std::string("A\xC3\xA9\xE4\xB8\xAD"));

    input.onChar(0x1F600); // 😀 -> F0 9F 98 80
    CHECK(input.textInput() ==
          std::string("A\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"));

    input.endFrame();
    CHECK(input.textInput().empty());
}

void runInputStateTests() {
    testInputInitialState();
    testInputMouseMove();
    testInputMouseButtons();
    testInputKeyboard();
    testInputFocusModifiersAndRepeat();
    testInputScroll();
    testInputText();
}

// ---------------------------------------------------------------------------
// Window - needs a display and a GL 3.3 capable driver.
// ---------------------------------------------------------------------------

void testWindowUninitialized() {
    section("Window: uninitialized instance is inert");

    tuneui::Window window;
    CHECK(!window.isValid());
    CHECK(!window.shouldClose());
    CHECK(window.windowWidth() == 0);
    CHECK(window.windowHeight() == 0);
    CHECK(window.framebufferWidth() == 0);
    CHECK(window.framebufferHeight() == 0);
    CHECK(window.contentScale() == 1.0f);
    CHECK(window.clipboardString().empty());
    CHECK(window.time() >= 0.0);
    CHECK(window.deltaTime() == 0.0);

    // Every method must be safe before init() and after the window is gone.
    window.pollEvents();
    window.swapBuffers();
    window.makeCurrent();
    window.setVSync(true);
    window.setVSync(false);
    window.setTitle("ignored");
    window.setCursorShape(tuneui::CursorShape::IBeam);
    window.setClipboardString("ignored");
    window.setResizeCallback([](int, int) {});
    window.setContentScaleCallback([](float, float) {});
    CHECK(window.input().mouseX() == 0.0);
    CHECK(window.input().textInput().empty());

    // Const and non-const input() must expose the same object.
    tuneui::InputState* mutableInput = &window.input();
    const tuneui::Window& constWindow = window;
    CHECK(mutableInput == &constWindow.input());
}

bool testWindowContext() {
    section("Window: init + GL 3.3 core context");

    tuneui::Window window;
    if (!window.init(640, 480, "TuneUI platform test")) {
        check(false, "window.init(640, 480, ...)", __FILE__, __LINE__);
        std::printf("  (no usable OpenGL 3.3 context - check the display/driver)\n");
        return false;
    }
    CHECK(window.isValid());
    CHECK(!window.shouldClose());

    CHECK(GLVersion.major >= 3);
    CHECK(GLVersion.minor >= 0);
    CHECK(GLAD_GL_VERSION_3_3 != 0);

    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    CHECK(version != nullptr);
    std::printf("  GL_VERSION : %s\n", version ? reinterpret_cast<const char*>(version) : "null");
    std::printf("  GL_RENDERER: %s\n", renderer ? reinterpret_cast<const char*>(renderer) : "null");

    const int width = window.framebufferWidth();
    const int height = window.framebufferHeight();
    CHECK(width > 0);
    CHECK(height > 0);

    const int logicalWidth = window.windowWidth();
    const int logicalHeight = window.windowHeight();
    CHECK(logicalWidth > 0);
    CHECK(logicalHeight > 0);
    const float scaleX = window.contentScaleX();
    const float scaleY = window.contentScaleY();
    CHECK(scaleX > 0.0f && std::isfinite(scaleX));
    CHECK(scaleY > 0.0f && std::isfinite(scaleY));
    std::printf("  framebuffer %dx%d, logical %dx%d, scale %.3f\n",
                width, height, logicalWidth, logicalHeight,
                static_cast<double>(scaleX));

    // init(640, 480) is a request in logical units (GLFW_SCALE_TO_MONITOR),
    // and windowWidth/Height must stay consistent with fb / contentScale.
    CHECK(std::abs(logicalWidth - 640) <= 1);
    CHECK(std::abs(logicalHeight - 480) <= 1);
    CHECK(std::abs(logicalWidth - static_cast<int>(std::lround(
              static_cast<float>(width) / scaleX))) <= 1);
    CHECK(std::abs(logicalHeight - static_cast<int>(std::lround(
              static_cast<float>(height) / scaleY))) <= 1);

    // init() is idempotent: calling it again must not leak or replace a window.
    CHECK(window.init(640, 480, "ignored second init"));
    CHECK(window.isValid());

    // Title and vsync setters are live no-ops when unsupported, never crash.
    window.setTitle("TuneUI platform test - running");
    window.setVSync(false);
    window.makeCurrent();
    window.setCursorShape(tuneui::CursorShape::IBeam);
    window.setCursorShape(tuneui::CursorShape::Arrow);

    const std::string clipboardText = "TuneUI clipboard round-trip";
    window.setClipboardString(clipboardText);
    CHECK(window.clipboardString() == clipboardText);

    window.setResizeCallback([](int w, int h) {
        std::printf("  [resize] framebuffer %dx%d\n", w, h);
    });

    // Drive a few frames: poll -> draw -> endFrame -> swap.
    const float clearColors[][3] = {
        {0.10f, 0.12f, 0.16f},
        {0.16f, 0.12f, 0.10f},
        {0.10f, 0.16f, 0.12f},
    };
    const double startTime = window.time();
    for (int frame = 0; frame < 30 && !window.shouldClose(); ++frame) {
        window.pollEvents();
        CHECK(window.deltaTime() >= 0.0);

        const auto& input = window.input();
        const float t = logicalHeight > 0
                            ? static_cast<float>(input.mouseY()) / static_cast<float>(logicalHeight)
                            : 0.0f;
        const auto& color = clearColors[frame % 3];

        glViewport(0, 0, window.framebufferWidth(), window.framebufferHeight());
        glClearColor(color[0] + 0.2f * t, color[1], color[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        window.input().endFrame();
        window.swapBuffers();
    }
    // Closing early is an accepted outcome; just confirm the queries still work.
    CHECK(window.framebufferWidth() > 0);
    CHECK(window.framebufferHeight() > 0);
    CHECK(window.time() >= startTime);

    return true;
}

void runLiveDemo(tuneui::Window& window) {
    section("Window: live input demo (Esc or close the window to exit)");
    window.setTitle("TuneUI live input - move / click / scroll / type / resize");

    bool callbackSeen = false;
    window.setResizeCallback([&callbackSeen](int w, int h) {
        callbackSeen = true;
        std::printf("  [resize] framebuffer %dx%d\n", w, h);
    });

    for (int frame = 0; !window.shouldClose(); ++frame) {
        window.pollEvents();

        tuneui::InputState& input = window.input();
        if (input.keyPressed(GLFW_KEY_ESCAPE)) {
            std::printf("  Esc pressed, leaving live demo\n");
            break;
        }

        // All three buttons are forwarded identically; log each one.
        const struct {
            const char* name;
            tuneui::MouseButton button;
        } buttons[] = {
            {"left", tuneui::MouseButton::Left},
            {"right", tuneui::MouseButton::Right},
            {"middle", tuneui::MouseButton::Middle},
        };
        for (const auto& entry : buttons) {
            if (input.mousePressed(entry.button)) {
                std::printf("  [mouse] %s press at %.0f,%.0f\n",
                            entry.name, input.mouseX(), input.mouseY());
            }
            if (input.mouseReleased(entry.button)) {
                std::printf("  [mouse] %s release at %.0f,%.0f\n",
                            entry.name, input.mouseX(), input.mouseY());
            }
        }
        if (input.scrollY() != 0.0) {
            std::printf("  [scroll] %.1f, %.1f\n", input.scrollX(), input.scrollY());
        }
        if (!input.textInput().empty()) {
            std::printf("  [text] \"%s\"\n", input.textInput().c_str());
        }
        if (input.keyPressed(GLFW_KEY_W)) {
            std::printf("  [key] W pressed\n");
        }

        const int logicalWidth = std::max(1, window.windowWidth());
        const int logicalHeight = std::max(1, window.windowHeight());
        const float r = std::clamp(static_cast<float>(input.mouseX()) / static_cast<float>(logicalWidth), 0.0f, 1.0f);
        const float g = std::clamp(static_cast<float>(input.mouseY()) / static_cast<float>(logicalHeight), 0.0f, 1.0f);

        char title[256];
        std::snprintf(title, sizeof(title),
                      "TuneUI live | mouse %.0f,%.0f | L:%d R:%d | scroll %.1f,%.1f | text=\"%s\"",
                      input.mouseX(), input.mouseY(),
                      input.mouseDown(tuneui::MouseButton::Left) ? 1 : 0,
                      input.mouseDown(tuneui::MouseButton::Right) ? 1 : 0,
                      input.scrollX(), input.scrollY(),
                      input.textInput().c_str());
        if (frame % 15 == 0) {
            window.setTitle(title);
        }

        glViewport(0, 0, std::max(1, window.framebufferWidth()),
                   std::max(1, window.framebufferHeight()));
        glClearColor(r, g, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        input.endFrame();
        window.swapBuffers();
    }

    if (!callbackSeen) {
        std::printf("  (resize callback not triggered - fine unless you resized the window)\n");
    }
}

} // namespace

int main(int argc, char** argv) {
    // Unbuffered so progress is visible even if the process is killed while a
    // window is open (redirected/CI runs otherwise lose the whole buffer).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    bool live = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--live") == 0) {
            live = true;
        }
    }

    std::printf("TuneUI platform test\n");

    runInputStateTests();
    testWindowUninitialized();

    if (testWindowContext()) {
        if (live) {
            // The automated window above is destroyed on return; opening a new
            // one here also exercises GLFW teardown followed by re-initialisation.
            tuneui::Window liveWindow;
            if (liveWindow.init(800, 600, "TuneUI live input")) {
                runLiveDemo(liveWindow);
            } else {
                check(false, "liveWindow.init(800, 600, ...)", __FILE__, __LINE__);
            }
        }
    }

    std::printf("\n%d checks, %d failure(s)\n", checkCount, failureCount);
    return failureCount == 0 ? 0 : 1;
}
