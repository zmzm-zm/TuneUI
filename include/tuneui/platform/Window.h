#pragma once

#include <functional>
#include <string>

#include <tuneui/platform/InputState.h>

// Forward declarations keep this header free of a GLFW dependency; the
// typedefs resolve to these tag types inside Window.cpp.
struct GLFWwindow;
struct GLFWcursor;

namespace tuneui {

enum class CursorShape {
    Arrow,
    IBeam,
    Hand,
    HResize,
    VResize,
    Crosshair,
};

// Owns a GLFW window, its OpenGL 3.3 core context, and the event loop /
// input-callback plumbing. GL function pointers are loaded with glad using
// GLFW's proc-address resolver.
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Initializes GLFW (once, shared across instances), creates the window
    // and context, loads GL entry points, and registers input callbacks.
    // Returns false (and leaves the window invalid) on any failure.
    bool init(int width, int height, const std::string& title);

    bool isValid() const { return window_ != nullptr; }

    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();

    // Logical (window-content) size in 96-DPI-independent units, and physical
    // (framebuffer) size in pixels. Input coordinates are reported in the same
    // logical space; all UI layout and drawing should use it, passing
    // contentScale() to Renderer::beginFrame().
    //
    // The logical size is framebufferSize / contentScale. On Windows the
    // window is created with GLFW_SCALE_TO_MONITOR, so a requested 800x600
    // stays 800x600 logical (and becomes 1000x750 physical at 125% DPI).
    int windowWidth() const;
    int windowHeight() const;
    int framebufferWidth() const;
    int framebufferHeight() const;
    float contentScaleX() const;
    float contentScaleY() const;
    float contentScale() const; // X scale; Windows normally has X == Y

    // Makes this window's GL context current. Callbacks may fire while another
    // window's context is current, so GL work is done by the frame code.
    void makeCurrent();

    void setTitle(const std::string& title);
    void setVSync(bool enabled);

    // Clipboard access for text widgets. Returns an empty string when there is
    // no window or the clipboard cannot be read.
    std::string clipboardString() const;
    void setClipboardString(const std::string& text);

    // Seconds since GLFW init and the time spent in the last pollEvents().
    double time() const;
    double deltaTime() const { return deltaTime_; }

    void setCursorShape(CursorShape shape);

    // The input state fed by this window's callbacks. Call endFrame() once
    // per frame after consuming the input it carries.
    InputState& input() { return input_; }
    const InputState& input() const { return input_; }

    // Optional hook fired on framebuffer resize. The viewport is not changed
    // here; Renderer::beginFrame owns it so callbacks cannot disturb another
    // window's context.
    using ResizeCallback = std::function<void(int width, int height)>;
    void setResizeCallback(ResizeCallback callback);

    // Fired when the OS/content scale changes (e.g. moving across monitors).
    using ContentScaleCallback = std::function<void(float xscale, float yscale)>;
    void setContentScaleCallback(ContentScaleCallback callback);

private:
    // GLFW callbacks. Registered with glfwSetWindowUserPointer(window_, this)
    // so each static trampoline can recover the owning Window and forward the
    // event into its InputState.
    static void framebufferSizeCallback(struct GLFWwindow* window, int width, int height);
    static void cursorPosCallback(struct GLFWwindow* window, double x, double y);
    static void cursorEnterCallback(struct GLFWwindow* window, int entered);
    static void mouseButtonCallback(struct GLFWwindow* window, int button, int action, int mods);
    static void keyCallback(struct GLFWwindow* window, int key, int scancode, int action, int mods);
    static void scrollCallback(struct GLFWwindow* window, double dx, double dy);
    static void charCallback(struct GLFWwindow* window, unsigned int codepoint);
    static void focusCallback(struct GLFWwindow* window, int focused);
    static void contentScaleCallback(struct GLFWwindow* window, float xscale, float yscale);

    // GLFW reports cursor positions in screen coordinates. On some platforms
    // (Windows with a DPI-aware context) those are physical pixels while the
    // renderer works in framebuffer/contentScale logical units; on others
    // (macOS) they are already logical. These helpers normalize both cases.
    double screenToLogicalX(double x) const;
    double screenToLogicalY(double y) const;

    struct GLFWwindow* window_ = nullptr;
    struct GLFWcursor* cursor_ = nullptr;
    InputState input_;
    ResizeCallback resizeCallback_;
    ContentScaleCallback contentScaleCallback_;
    double lastPollTime_ = 0.0;
    double deltaTime_ = 0.0;
};

} // namespace tuneui
