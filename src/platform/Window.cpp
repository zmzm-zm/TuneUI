#include <tuneui/platform/Window.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace tuneui {

namespace {

// GLFW is initialized once and torn down when the last Window is destroyed.
// A reference counter keeps this correct even with multiple Window instances.
int windowCount = 0;
bool glfwInitialized = false;

// Tears GLFW down once no live window remains.
void releaseGlfwIfIdle() {
    if (windowCount <= 0 && glfwInitialized) {
        windowCount = 0;
        glfwTerminate();
        glfwInitialized = false;
    }
}

// Raw GLFW window size in screen coordinates. Kept separate from
// Window::windowWidth(), which returns the logical size.
int rawWindowWidth(GLFWwindow* window) {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    return width;
}

int rawWindowHeight(GLFWwindow* window) {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    return height;
}

} // namespace

Window::~Window() {
    // Only a window that was successfully created bumped the counter, so only
    // that one decrements it. A failed init() must not unbalance the count of
    // other live windows.
    if (cursor_ != nullptr) {
        glfwDestroyCursor(cursor_);
        cursor_ = nullptr;
    }
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        --windowCount;
        releaseGlfwIfIdle();
    }
}

bool Window::init(int width, int height, const std::string& title) {
    if (window_ != nullptr) {
        return true; // already initialized; never overwrite a live handle
    }
    input_ = InputState{};

    if (!glfwInitialized) {
        if (glfwInit() != GLFW_TRUE) {
            return false;
        }
        glfwInitialized = true;
    }

    // OpenGL 3.3 core is the baseline the render layer is written against.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    // Treat init(width, height) as logical units: on Windows/X11 the content
    // area is scaled by the monitor DPI, so an 800x600 request is 800x600
    // logical pixels at any DPI. macOS already behaves this way.
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window_ == nullptr) {
        // Nothing to roll back: the process-wide GLFW init stays alive for a
        // later retry; the shared teardown happens in the destructor.
        return false;
    }
    ++windowCount;

    glfwMakeContextCurrent(window_);

    // Load GL 3.3 core entry points through GLFW's resolver. The cast is the
    // glad-sanctioned idiom for bridging GLFWglproc to GLADloadproc.
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        --windowCount;
        releaseGlfwIfIdle();
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, &Window::framebufferSizeCallback);
    glfwSetCursorPosCallback(window_, &Window::cursorPosCallback);
    glfwSetCursorEnterCallback(window_, &Window::cursorEnterCallback);
    glfwSetMouseButtonCallback(window_, &Window::mouseButtonCallback);
    glfwSetKeyCallback(window_, &Window::keyCallback);
    glfwSetScrollCallback(window_, &Window::scrollCallback);
    glfwSetCharCallback(window_, &Window::charCallback);
    glfwSetWindowFocusCallback(window_, &Window::focusCallback);
    glfwSetWindowContentScaleCallback(window_, &Window::contentScaleCallback);

    glfwSwapInterval(1); // vsync on by default

    // Apply the initial viewport once the framebuffer is known. Later frames
    // set it in Renderer::beginFrame; callbacks never touch GL state.
    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    lastPollTime_ = glfwGetTime();
    deltaTime_ = 0.0;
    return true;
}

bool Window::shouldClose() const {
    return window_ != nullptr && glfwWindowShouldClose(window_) != 0;
}

void Window::pollEvents() {
    if (window_ == nullptr) {
        return;
    }
    const double now = glfwGetTime();
    if (lastPollTime_ > 0.0) {
        deltaTime_ = std::max(0.0, now - lastPollTime_);
    } else {
        deltaTime_ = 0.0;
    }
    lastPollTime_ = now;
    glfwPollEvents();
}

void Window::swapBuffers() {
    if (window_ != nullptr) {
        glfwSwapBuffers(window_);
    }
}

int Window::windowWidth() const {
    if (window_ == nullptr) {
        return 0;
    }
    const float scale = contentScaleX();
    const int pixels = framebufferWidth();
    if (!(scale > 0.0f) || pixels <= 0) {
        return pixels > 0 ? pixels : 0;
    }
    return static_cast<int>(std::lround(static_cast<float>(pixels) / scale));
}

int Window::windowHeight() const {
    if (window_ == nullptr) {
        return 0;
    }
    const float scale = contentScaleY();
    const int pixels = framebufferHeight();
    if (!(scale > 0.0f) || pixels <= 0) {
        return pixels > 0 ? pixels : 0;
    }
    return static_cast<int>(std::lround(static_cast<float>(pixels) / scale));
}

int Window::framebufferWidth() const {
    int width = 0;
    int height = 0;
    if (window_ != nullptr) {
        glfwGetFramebufferSize(window_, &width, &height);
    }
    return width;
}

int Window::framebufferHeight() const {
    int width = 0;
    int height = 0;
    if (window_ != nullptr) {
        glfwGetFramebufferSize(window_, &width, &height);
    }
    return height;
}

float Window::contentScaleX() const {
    if (window_ == nullptr) {
        return 1.0f;
    }
    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window_, &xscale, &yscale);
    if (!std::isfinite(xscale) || xscale <= 0.0f) {
        // Some drivers report 0; derive the scale from the raw GLFW sizes.
        // (Use the raw helpers: windowWidth() itself depends on this scale.)
        const int rawWidth = rawWindowWidth(window_);
        xscale = rawWidth > 0
                     ? static_cast<float>(framebufferWidth()) / static_cast<float>(rawWidth)
                     : 1.0f;
    }
    return xscale > 0.0f ? xscale : 1.0f;
}

float Window::contentScaleY() const {
    if (window_ == nullptr) {
        return 1.0f;
    }
    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window_, &xscale, &yscale);
    if (!std::isfinite(yscale) || yscale <= 0.0f) {
        const int rawHeight = rawWindowHeight(window_);
        yscale = rawHeight > 0
                     ? static_cast<float>(framebufferHeight()) / static_cast<float>(rawHeight)
                     : 1.0f;
    }
    return yscale > 0.0f ? yscale : 1.0f;
}

float Window::contentScale() const {
    return contentScaleX();
}

double Window::screenToLogicalX(double x) const {
    if (window_ == nullptr) {
        return x;
    }
    const int rawWidth = rawWindowWidth(window_);
    const int pixels = framebufferWidth();
    const float scale = contentScaleX();
    if (rawWidth <= 0 || pixels <= 0 || !(scale > 0.0f) || !std::isfinite(scale)) {
        return x;
    }
    // framebuffer/raw-window converts screen coordinates to physical pixels;
    // dividing by contentScale converts physical pixels to logical units.
    return x * (static_cast<double>(pixels) / static_cast<double>(rawWidth)) /
           static_cast<double>(scale);
}

double Window::screenToLogicalY(double y) const {
    if (window_ == nullptr) {
        return y;
    }
    const int rawHeight = rawWindowHeight(window_);
    const int pixels = framebufferHeight();
    const float scale = contentScaleY();
    if (rawHeight <= 0 || pixels <= 0 || !(scale > 0.0f) || !std::isfinite(scale)) {
        return y;
    }
    return y * (static_cast<double>(pixels) / static_cast<double>(rawHeight)) /
           static_cast<double>(scale);
}

void Window::makeCurrent() {
    if (window_ != nullptr) {
        glfwMakeContextCurrent(window_);
    }
}

void Window::setTitle(const std::string& title) {
    if (window_ != nullptr) {
        glfwSetWindowTitle(window_, title.c_str());
    }
}

void Window::setVSync(bool enabled) {
    if (window_ != nullptr) {
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(enabled ? 1 : 0);
    }
}

std::string Window::clipboardString() const {
    if (window_ == nullptr) {
        return {};
    }
    const char* text = glfwGetClipboardString(window_);
    return text != nullptr ? std::string(text) : std::string{};
}

void Window::setClipboardString(const std::string& text) {
    if (window_ != nullptr) {
        glfwSetClipboardString(window_, text.c_str());
    }
}

double Window::time() const {
    return glfwGetTime();
}

void Window::setCursorShape(CursorShape shape) {
    if (window_ == nullptr) {
        return;
    }

    int glfwShape = GLFW_ARROW_CURSOR;
    switch (shape) {
        case CursorShape::IBeam:     glfwShape = GLFW_IBEAM_CURSOR; break;
        case CursorShape::Hand:      glfwShape = GLFW_HAND_CURSOR; break;
        case CursorShape::HResize:   glfwShape = GLFW_HRESIZE_CURSOR; break;
        case CursorShape::VResize:   glfwShape = GLFW_VRESIZE_CURSOR; break;
        case CursorShape::Crosshair: glfwShape = GLFW_CROSSHAIR_CURSOR; break;
        case CursorShape::Arrow:
        default:                     glfwShape = GLFW_ARROW_CURSOR; break;
    }

    GLFWcursor* next = glfwCreateStandardCursor(glfwShape);
    if (next == nullptr) {
        return;
    }
    if (cursor_ != nullptr) {
        glfwDestroyCursor(cursor_);
    }
    cursor_ = next;
    glfwSetCursor(window_, cursor_);
}

void Window::setResizeCallback(ResizeCallback callback) {
    resizeCallback_ = std::move(callback);
}

void Window::setContentScaleCallback(ContentScaleCallback callback) {
    contentScaleCallback_ = std::move(callback);
}

// ---- GLFW callbacks -------------------------------------------------------

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }
    // No glViewport here: the callback may fire while another window's context
    // is current. Renderer::beginFrame sets the viewport each frame.
    if (self->resizeCallback_) {
        self->resizeCallback_(width, height);
    }
}

void Window::cursorPosCallback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->input_.onMouseMove(self->screenToLogicalX(x),
                                 self->screenToLogicalY(y));
    }
}

void Window::cursorEnterCallback(GLFWwindow* window, int entered) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->input_.onCursorEnter(entered != 0);
    }
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }
    // GLFW_MOUSE_BUTTON_* are 0..7, matching MouseButton's ordering.
    self->input_.onMouseButton(static_cast<MouseButton>(button),
                               action == GLFW_PRESS,
                               static_cast<unsigned>(mods));
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }
    const unsigned modifierBits = static_cast<unsigned>(mods);
    if (action == GLFW_REPEAT) {
        self->input_.onKey(key, true, modifierBits, true);
        return;
    }
    self->input_.onKey(key, action == GLFW_PRESS, modifierBits, false);
}

void Window::focusCallback(GLFWwindow* window, int focused) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr && focused == GLFW_FALSE) {
        self->input_.onFocusLost();
    }
}

void Window::contentScaleCallback(GLFWwindow* window, float xscale, float yscale) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr && self->contentScaleCallback_) {
        self->contentScaleCallback_(xscale, yscale);
    }
}

void Window::scrollCallback(GLFWwindow* window, double dx, double dy) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->input_.onScroll(dx, dy);
    }
}

void Window::charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->input_.onChar(codepoint);
    }
}

} // namespace tuneui
