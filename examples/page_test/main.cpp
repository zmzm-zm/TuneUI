#include <tuneui/platform/Window.h>
#include <tuneui/render/Renderer.h>
#include <tuneui/render/Page.h>

#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    // --frames N exits after N frames, which lets CTest smoke-test the demo.
    int maxFrames = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            maxFrames = std::atoi(argv[++i]);
        }
    }

    tuneui::Window window;
    if (!window.init(800, 600, "TEST")) {
        return 1;
    }
    tuneui::Renderer renderer;
    if (!renderer.init()) {
        return 1;
    }

    // UI coordinates are logical pixels (see Window::windowWidth), so the page
    // geometry stays correct at 100%, 125%, 150%, ...
    const float logicalWidth = static_cast<float>(window.windowWidth());
    const float logicalHeight = static_cast<float>(window.windowHeight());

    tuneui::Page page1;
    page1.setBackground({0.10f, 0.11f, 0.13f, 1.0f});

    // Floor as a stand-alone configuration object: configure it, then hand it
    // over to the page (this copies; use std::move(content) to avoid the copy).
    tuneui::Floor content("content", 2);
    content.setClip({0, 0, logicalWidth, logicalHeight})
           .addRect({20, 50, 200, 60}, {0.9f, 0.6f, 1.0f, 1.0f});
    page1.addFloor(content);

    tuneui::Floor hud("hud", 10);
    hud.addRect({0, 0, logicalWidth, 64}, {0.4, 0, 0, 0.2f});
    page1.addFloor(hud);

    for (int frame = 0;
         !window.shouldClose() && (maxFrames <= 0 || frame < maxFrames);
         ++frame) {
        window.pollEvents();
        // A minimized window has a 0-sized framebuffer: skip drawing.
        if (!renderer.beginFrame(
                window.framebufferWidth(),
                window.framebufferHeight(),
                window.contentScale())) {
            window.input().endFrame();
            window.swapBuffers();
            continue;
        }
        page1.render(renderer);
        renderer.endFrame();

        window.input().endFrame();
        window.swapBuffers();
    }
    renderer.shutdown();
}
