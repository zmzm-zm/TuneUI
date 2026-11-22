// Renderer test / demo for TODO 1.3.
//
// Covers: ortho projection, shader wrapper, texture upload (including
// premultiplied input), batching, blending, scissor clipping, DPI scaling.
//
// Default run: automated checks with glReadPixels verification, then exit.
// --live: keep a window open with animated quads (Esc to quit).

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <stb/stb_truetype.h>

#include <tuneui/render/Math.h>
#include <tuneui/render/Page.h>
#include <tuneui/render/Renderer.h>
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

struct Pixel {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
};

Pixel readPixel(int framebufferHeight, int x, int y) {
    unsigned char rgba[4] = {0, 0, 0, 0};
    glReadPixels(x, framebufferHeight - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return Pixel{rgba[0], rgba[1], rgba[2], rgba[3]};
}

void checkPixel(const char* label, const Pixel& actual, int r, int g, int b, int tolerance = 3) {
    const bool ok = std::abs(actual.r - r) <= tolerance &&
                    std::abs(actual.g - g) <= tolerance &&
                    std::abs(actual.b - b) <= tolerance;
    check(ok, label, __FILE__, __LINE__);
    if (!ok) {
        std::printf("  %s: got (%d,%d,%d), expected (%d,%d,%d) +-%d\n",
                    label, actual.r, actual.g, actual.b, r, g, b, tolerance);
    }
}

struct Projected {
    float x = 0.0f;
    float y = 0.0f;
};

Projected project(const glm::mat4& matrix, float x, float y) {
    const glm::vec4 clip = matrix * glm::vec4(x, y, 0.0f, 1.0f);
    return Projected{clip.x, clip.y};
}

bool nearly(float a, float b, float epsilon = 1e-5f) {
    return std::fabs(a - b) <= epsilon;
}

void testOrtho() {
    section("Math: pixel-space ortho projection");

    const glm::mat4 m = tuneui::orthoPixels(800.0f, 600.0f);
    const Projected topLeft = project(m, 0.0f, 0.0f);
    const Projected bottomRight = project(m, 800.0f, 600.0f);
    const Projected center = project(m, 400.0f, 300.0f);

    CHECK(nearly(topLeft.x, -1.0f) && nearly(topLeft.y, 1.0f));
    CHECK(nearly(bottomRight.x, 1.0f) && nearly(bottomRight.y, -1.0f));
    CHECK(nearly(center.x, 0.0f) && nearly(center.y, 0.0f));

    // DPI scale 2: a 1600x1200 framebuffer means an 800x600 logical canvas.
    const glm::mat4 dpi = tuneui::orthoPixels(1600.0f, 1200.0f, 2.0f);
    const Projected dpiTopLeft = project(dpi, 0.0f, 0.0f);
    const Projected dpiBottomRight = project(dpi, 800.0f, 600.0f);
    CHECK(nearly(dpiTopLeft.x, -1.0f) && nearly(dpiTopLeft.y, 1.0f));
    CHECK(nearly(dpiBottomRight.x, 1.0f) && nearly(dpiBottomRight.y, -1.0f));

    // Degenerate sizes fall back to identity instead of producing NaNs.
    const glm::mat4 degenerate = tuneui::orthoPixels(0.0f, 600.0f);
    CHECK(degenerate == glm::mat4(1.0f));
}

void testBatchAndBlending(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Renderer: 100 quads, batching, blending");

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});

    for (int i = 0; i < 100; ++i) {
        const float x = 20.0f + static_cast<float>(i % 10) * 36.0f;
        const float y = 20.0f + static_cast<float>(i / 10) * 24.0f;
        const tuneui::Color color{
            static_cast<float>(i % 10) / 9.0f,
            static_cast<float>(i / 10) / 9.0f,
            0.5f,
            0.35f,
        };
        renderer.drawRect({x, y, 30.0f, 20.0f}, color);
    }

    CHECK(renderer.pendingVertexCount() == 600); // 100 quads * 6 vertices
    renderer.endFrame();
    CHECK(renderer.drawCallCount() == 1);        // same texture -> one batch
    CHECK(renderer.pendingVertexCount() == 0);

    checkPixel("outside quads is clear color", readPixel(fbHeight, 5, 5), 0, 0, 0);
    // Quad 0 covers (20,20)-(50,40): color (0, 0, 0.5) at alpha 0.35 over black.
    checkPixel("first quad blended over black", readPixel(fbHeight, 30, 30), 0, 0, 45);

    section("Renderer: alpha blending order");

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.drawRect({10.0f, 10.0f, 100.0f, 100.0f}, {1.0f, 0.0f, 0.0f, 0.5f});
    renderer.endFrame();
    checkPixel("50% red over black", readPixel(fbHeight, 50, 50), 128, 0, 0);

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.drawRect({10.0f, 10.0f, 100.0f, 100.0f}, {1.0f, 0.0f, 0.0f, 0.5f});
    renderer.drawRect({10.0f, 10.0f, 100.0f, 100.0f}, {0.0f, 0.0f, 1.0f, 0.5f});
    renderer.endFrame();
    // blue 0.5 over (red 0.5 over black) = (0.25, 0, 0.5)
    checkPixel("50% blue over 50% red", readPixel(fbHeight, 50, 50), 64, 0, 128);
}

void testTextures(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Renderer: texture upload, UV mapping, premultiplied alpha");

    // 2x2 image: row 0 = red, green; row 1 = blue, white.
    const unsigned char texels[2 * 2 * 4] = {
        255, 0,   0,   255,  0,   255, 0,   255,
        0,   0,   255, 255,  255, 255, 255, 255,
    };
    tuneui::Texture texture;
    CHECK(texture.create(texels, 2, 2, false,
                         tuneui::TextureFilter::Nearest, tuneui::TextureWrap::ClampToEdge));
    CHECK(texture.isValid());
    CHECK(texture.width() == 2 && texture.height() == 2);

    renderer.beginFrame(fbWidth, fbHeight, 1.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.drawTexturedRect({200.0f, 10.0f, 100.0f, 100.0f},
                              {0.0f, 0.0f, 1.0f, 1.0f},
                              {1.0f, 1.0f, 1.0f, 1.0f}, texture.id());
    renderer.endFrame();

    checkPixel("texture top-left texel", readPixel(fbHeight, 220, 30), 255, 0, 0);
    checkPixel("texture top-right texel", readPixel(fbHeight, 280, 30), 0, 255, 0);
    checkPixel("texture bottom-left texel", readPixel(fbHeight, 220, 90), 0, 0, 255);
    checkPixel("texture bottom-right texel", readPixel(fbHeight, 280, 90), 255, 255, 255);

    // Premultiplied input: (128, 0, 0, 128) is straight red at ~50% alpha.
    const unsigned char premultiplied[4] = {128, 0, 0, 128};
    tuneui::Texture premulTexture;
    CHECK(premulTexture.create(premultiplied, 1, 1, true,
                               tuneui::TextureFilter::Nearest, tuneui::TextureWrap::ClampToEdge));

    renderer.beginFrame(fbWidth, fbHeight, 1.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.drawTexturedRect({400.0f, 10.0f, 40.0f, 40.0f},
                              {0.0f, 0.0f, 1.0f, 1.0f},
                              {1.0f, 1.0f, 1.0f, 1.0f}, premulTexture.id());
    renderer.endFrame();
    checkPixel("premultiplied input converted to straight alpha",
               readPixel(fbHeight, 420, 30), 128, 0, 0);

    section("Renderer: batching across two textures");
    renderer.beginFrame(fbWidth, fbHeight, 1.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    for (int i = 0; i < 40; ++i) {
        renderer.drawTexturedRect({10.0f + static_cast<float>(i), 300.0f, 8.0f, 8.0f},
                                  {0.0f, 0.0f, 1.0f, 1.0f},
                                  {1.0f, 1.0f, 1.0f, 1.0f}, texture.id());
    }
    for (int i = 0; i < 40; ++i) {
        renderer.drawTexturedRect({10.0f + static_cast<float>(i), 320.0f, 8.0f, 8.0f},
                                  {0.0f, 0.0f, 1.0f, 1.0f},
                                  {1.0f, 1.0f, 1.0f, 1.0f}, premulTexture.id());
    }
    renderer.endFrame();
    CHECK(renderer.drawCallCount() == 2); // one flush per texture
}

// Minimal 2x2 uncompressed 32-bit TGA built at runtime, so stb_image decoding
// can be exercised without shipping a binary asset.
std::vector<unsigned char> makeTga2x2() {
    const unsigned char header[18] = {
        0, 0, 2,          // id length, no color map, uncompressed true-color
        0, 0, 0, 0, 0,    // color map spec
        0, 0, 0, 0,       // x/y origin
        2, 0, 2, 0,       // width 2, height 2 (little endian)
        32,               // bits per pixel
        0x08 | 0x20,      // 8 alpha bits, origin top-left
    };
    // TGA true-color stores BGR(A). Row 0: red, green. Row 1: blue, white.
    const unsigned char texels[16] = {
        0,   0,   255, 255,  // red
        0,   255, 0,   255,  // green
        255, 0,   0,   255,  // blue
        255, 255, 255, 255,  // white
    };
    std::vector<unsigned char> bytes(header, header + sizeof(header));
    bytes.insert(bytes.end(), texels, texels + sizeof(texels));
    return bytes;
}

void testImageDecoding(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Texture: stb_image decoding (in-memory TGA)");

    const std::vector<unsigned char> tga = makeTga2x2();
    tuneui::Texture decoded;
    CHECK(decoded.createFromMemory(tga.data(), tga.size(),
                                   tuneui::TextureFilter::Nearest,
                                   tuneui::TextureWrap::ClampToEdge));
    CHECK(decoded.isValid());
    CHECK(decoded.width() == 2 && decoded.height() == 2);

    renderer.beginFrame(fbWidth, fbHeight, 1.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.drawTexturedRect({500.0f, 10.0f, 100.0f, 100.0f},
                              {0.0f, 0.0f, 1.0f, 1.0f},
                              {1.0f, 1.0f, 1.0f, 1.0f}, decoded.id());
    renderer.endFrame();

    checkPixel("decoded TGA top-left (red)", readPixel(fbHeight, 520, 30), 255, 0, 0);
    checkPixel("decoded TGA top-right (green)", readPixel(fbHeight, 580, 30), 0, 255, 0);
    checkPixel("decoded TGA bottom-left (blue)", readPixel(fbHeight, 520, 90), 0, 0, 255);
    checkPixel("decoded TGA bottom-right (white)", readPixel(fbHeight, 580, 90), 255, 255, 255);

    // Vertical flip swaps the rows.
    tuneui::Texture flipped;
    CHECK(flipped.createFromMemory(tga.data(), tga.size(),
                                   tuneui::TextureFilter::Nearest,
                                   tuneui::TextureWrap::ClampToEdge,
                                   /*flipVertically=*/true));
    renderer.beginFrame(fbWidth, fbHeight, 1.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.drawTexturedRect({500.0f, 120.0f, 100.0f, 100.0f},
                              {0.0f, 0.0f, 1.0f, 1.0f},
                              {1.0f, 1.0f, 1.0f, 1.0f}, flipped.id());
    renderer.endFrame();
    checkPixel("flipped TGA top-left is now blue", readPixel(fbHeight, 520, 140), 0, 0, 255);
    checkPixel("flipped TGA bottom-left is now red", readPixel(fbHeight, 520, 200), 255, 0, 0);

    section("Texture: failure paths");
    tuneui::Texture missing;
    CHECK(!missing.createFromFile("does-not-exist.png"));
    CHECK(!missing.isValid());

    tuneui::Texture garbage;
    const unsigned char junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(!garbage.createFromMemory(junk, sizeof(junk)));
    CHECK(!garbage.isValid());
}

void testScissor(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Renderer: scissor clipping");

    renderer.beginFrame(fbWidth, fbHeight, 1.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.setScissor({100.0f, 100.0f, 100.0f, 100.0f});
    CHECK(renderer.scissorEnabled());
    renderer.drawRect({50.0f, 50.0f, 200.0f, 200.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    renderer.endFrame();

    checkPixel("inside scissor is drawn", readPixel(fbHeight, 150, 150), 255, 255, 255);
    checkPixel("inside quad but outside scissor is clipped",
               readPixel(fbHeight, 60, 60), 0, 0, 0);
    checkPixel("outside quad stays clear", readPixel(fbHeight, 250, 250), 0, 0, 0);

    renderer.clearScissor();
    CHECK(!renderer.scissorEnabled());

    section("Renderer: DPI scale (logical -> framebuffer)");
    renderer.beginFrame(fbWidth, fbHeight, 2.0f);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    // With dpiScale 2 the logical canvas is half the framebuffer, so a
    // quadrant of it (a quarter of each framebuffer axis) is drawn.
    const float logicalWidth = static_cast<float>(fbWidth) / 4.0f;
    const float logicalHeight = static_cast<float>(fbHeight) / 4.0f;
    renderer.drawRect({0.0f, 0.0f, logicalWidth, logicalHeight},
                      {1.0f, 1.0f, 1.0f, 1.0f});
    renderer.endFrame();

    checkPixel("logical top-left quadrant is drawn",
               readPixel(fbHeight, fbWidth / 4, fbHeight / 4), 255, 255, 255);
    checkPixel("logical bottom-right stays clear",
               readPixel(fbHeight, fbWidth * 3 / 4, fbHeight / 4), 0, 0, 0);
}

void testFrameState(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Renderer: frame state guard");

    CHECK(!renderer.frameActive());
    renderer.drawRect({0.0f, 0.0f, 10.0f, 10.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    CHECK(renderer.pendingVertexCount() == 0); // drawing outside a frame is ignored
    renderer.clear({1.0f, 0.0f, 0.0f, 1.0f});
    renderer.endFrame(); // unmatched end is safe

    CHECK(!renderer.beginFrame(0, 0, 1.0f)); // minimized window
    CHECK(renderer.beginFrame(fbWidth, fbHeight, 0.0f)); // invalid scale -> 1.0
    CHECK(renderer.frameActive());

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f)); // flushes the first frame
    CHECK(renderer.pendingVertexCount() == 0);
    CHECK(renderer.drawCallCount() == 0); // counter reset for the new frame
    renderer.endFrame();
    CHECK(!renderer.frameActive());

    renderer.drawRect({0.0f, 0.0f, 10.0f, 10.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    CHECK(renderer.pendingVertexCount() == 0); // no drawing after endFrame
}

void testCapacityFlush(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Renderer: batch capacity flush");
    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});

    for (int i = 0; i < tuneui::Renderer::MaxQuadsPerBatch + 1; ++i) {
        renderer.drawRect({0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    }
    renderer.endFrame();
    CHECK(renderer.drawCallCount() == 2);
    CHECK(renderer.pendingVertexCount() == 0);
}

void testNestedScissorAndDpi(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Renderer: nested scissor stack");

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.pushScissor({100.0f, 100.0f, 200.0f, 200.0f});
    renderer.pushScissor({150.0f, 150.0f, 200.0f, 200.0f});
    CHECK(renderer.scissorDepth() == 2);
    const tuneui::Rect current = renderer.currentScissor();
    CHECK(current.x == 150.0f && current.y == 150.0f);
    CHECK(current.width == 150.0f && current.height == 150.0f);
    renderer.drawRect({0.0f, 0.0f, 500.0f, 500.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    renderer.popScissor();
    CHECK(renderer.scissorDepth() == 1);
    renderer.popScissor();
    CHECK(renderer.scissorDepth() == 0);
    CHECK(!renderer.scissorEnabled());
    renderer.endFrame();

    checkPixel("nested clip intersection is drawn",
               readPixel(fbHeight, 160, 160), 255, 255, 255);
    checkPixel("inside outer but outside inner is clipped",
               readPixel(fbHeight, 140, 140), 0, 0, 0);
    checkPixel("outside both clips is clipped",
               readPixel(fbHeight, 320, 320), 0, 0, 0);

    section("Renderer: scissor with non-finite rect");
    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.pushScissor({0.0f, 0.0f, NAN, 10.0f});
    CHECK(renderer.scissorDepth() == 1);
    renderer.drawRect({0.0f, 0.0f, 50.0f, 50.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    renderer.endFrame();
    checkPixel("non-finite scissor clips everything", readPixel(fbHeight, 10, 10), 0, 0, 0);

    section("Renderer: scissor at dpiScale 2");
    CHECK(renderer.beginFrame(fbWidth, fbHeight, 2.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    renderer.setScissor({100.0f, 100.0f, 100.0f, 100.0f});
    renderer.drawRect({0.0f, 0.0f, 400.0f, 400.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    renderer.endFrame();
    checkPixel("logical scissor maps to physical pixels",
               readPixel(fbHeight, 250, 250), 255, 255, 255);
    checkPixel("outside logical scissor is clipped",
               readPixel(fbHeight, 150, 150), 0, 0, 0);
    checkPixel("past logical scissor is clipped",
               readPixel(fbHeight, 450, 450), 0, 0, 0);
}

void testTextureRegions(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Texture: createBlank + updateRegion + sub-rect UV");

    const unsigned char black[4 * 4 * 4] = {};
    tuneui::Texture atlas;
    CHECK(atlas.createBlank(4, 4, tuneui::TextureFilter::Nearest,
                            tuneui::TextureWrap::ClampToEdge, black));
    CHECK(atlas.isValid());
    CHECK(atlas.width() == 4 && atlas.height() == 4);

    const unsigned char green[2 * 2 * 4] = {
        0, 255, 0, 255, 0, 255, 0, 255,
        0, 255, 0, 255, 0, 255, 0, 255,
    };
    CHECK(atlas.updateRegion(2, 2, 2, 2, green));
    CHECK(!atlas.updateRegion(-1, 0, 1, 1, green));
    CHECK(!atlas.updateRegion(3, 3, 2, 2, green));
    CHECK(!atlas.updateRegion(0, 0, 1, 1, nullptr));

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    // Full texture: the updated (2,2)..(4,4) region is the bottom-right quarter.
    renderer.drawTexturedRect({200.0f, 200.0f, 100.0f, 100.0f},
                              {0.0f, 0.0f, 1.0f, 1.0f},
                              {1.0f, 1.0f, 1.0f, 1.0f}, atlas.id());
    // Sub-rect UV: stretch just the updated quarter across another quad.
    renderer.drawTexturedRect({400.0f, 200.0f, 100.0f, 100.0f},
                              {0.5f, 0.5f, 0.5f, 0.5f},
                              {1.0f, 1.0f, 1.0f, 1.0f}, atlas.id());
    renderer.endFrame();

    checkPixel("untouched atlas region stays black",
               readPixel(fbHeight, 220, 220), 0, 0, 0);
    checkPixel("updated atlas region is green",
               readPixel(fbHeight, 280, 280), 0, 255, 0);
    checkPixel("sub-rect UV samples the updated region",
               readPixel(fbHeight, 420, 220), 0, 255, 0);
}

void testFontAtlas(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    section("Font: stb_truetype -> white-alpha atlas -> tinted quad");

    const char* path = "C:/Windows/Fonts/arial.ttf";
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        std::printf("  (skipped: %s not found; system font test is optional)\n", path);
        return;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        std::printf("  (skipped: could not size %s)\n", path);
        return;
    }
    std::vector<unsigned char> fontData(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(fontData.data(), 1, fontData.size(), file);
    std::fclose(file);
    if (read != fontData.size()) {
        std::printf("  (skipped: short read from %s)\n", path);
        return;
    }

    stbtt_fontinfo font{};
    if (!stbtt_InitFont(&font, fontData.data(), 0)) {
        std::printf("  (skipped: stb_truetype rejected %s)\n", path);
        return;
    }

    const float scale = stbtt_ScaleForPixelHeight(&font, 48.0f);
    int glyphWidth = 0;
    int glyphHeight = 0;
    int xoff = 0;
    int yoff = 0;
    unsigned char* coverage = stbtt_GetCodepointBitmap(
        &font, 0.0f, scale, 'A', &glyphWidth, &glyphHeight, &xoff, &yoff);
    if (coverage == nullptr || glyphWidth <= 0 || glyphHeight <= 0 ||
        glyphWidth > 128 || glyphHeight > 128) {
        stbtt_FreeBitmap(coverage, nullptr);
        std::printf("  (skipped: glyph bitmap did not fit the test atlas)\n");
        return;
    }

    const std::vector<unsigned char> black(128 * 128 * 4, 0);
    tuneui::Texture atlas;
    CHECK(atlas.createBlank(128, 128, tuneui::TextureFilter::Nearest,
                            tuneui::TextureWrap::ClampToEdge, black.data()));
    CHECK(atlas.isValid());

    std::vector<unsigned char> rgba(static_cast<std::size_t>(glyphWidth) *
                                    static_cast<std::size_t>(glyphHeight) * 4u);
    int firstX = -1;
    int firstY = -1;
    unsigned char firstCoverage = 0;
    for (int y = 0; y < glyphHeight; ++y) {
        for (int x = 0; x < glyphWidth; ++x) {
            const unsigned char a = coverage[static_cast<std::size_t>(y) *
                                             static_cast<std::size_t>(glyphWidth) +
                                             static_cast<std::size_t>(x)];
            unsigned char* pixel = rgba.data() +
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(glyphWidth) +
                 static_cast<std::size_t>(x)) * 4u;
            pixel[0] = 255;
            pixel[1] = 255;
            pixel[2] = 255;
            pixel[3] = a;
            if (a > 32 && firstX < 0) {
                firstX = x;
                firstY = y;
                firstCoverage = a;
            }
        }
    }
    stbtt_FreeBitmap(coverage, nullptr);

    CHECK(atlas.updateRegion(0, 0, glyphWidth, glyphHeight, rgba.data()));
    if (firstX < 0) {
        std::printf("  (skipped: no opaque glyph pixel)\n");
        return;
    }

    CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    const tuneui::Rect dst{600.0f, 300.0f, 128.0f, 128.0f};
    renderer.drawTexturedRect(
        dst,
        {0.0f, 0.0f,
         static_cast<float>(glyphWidth) / 128.0f,
         static_cast<float>(glyphHeight) / 128.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}, atlas.id());
    renderer.endFrame();

    const int screenX = static_cast<int>(
        600.0f + (static_cast<float>(firstX) + 0.5f) * 128.0f /
                     static_cast<float>(glyphWidth));
    const int screenY = static_cast<int>(
        300.0f + (static_cast<float>(firstY) + 0.5f) * 128.0f /
                     static_cast<float>(glyphHeight));
    const Pixel pixel = readPixel(fbHeight, screenX, screenY);
    CHECK(pixel.r >= static_cast<int>(firstCoverage) - 8);
    if (pixel.r < static_cast<int>(firstCoverage) - 8) {
        std::printf("  glyph pixel: got (%d,%d,%d), coverage %d at atlas (%d,%d)\n",
                    pixel.r, pixel.g, pixel.b, firstCoverage, firstX, firstY);
    }
}

void testPageComposition(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    const tuneui::Color black{0.0f, 0.0f, 0.0f, 1.0f};
    const tuneui::Color white{1.0f, 1.0f, 1.0f, 1.0f};
    const tuneui::Color red{1.0f, 0.0f, 0.0f, 1.0f};
    const tuneui::Color blue{0.0f, 0.0f, 1.0f, 1.0f};
    const float pageWidth = static_cast<float>(fbWidth);
    const float pageHeight = static_cast<float>(fbHeight);

    // Every scenario: fresh frame, black backdrop, page renders, flush.
    auto renderPage = [&](tuneui::Page& page) {
        CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
        renderer.clear(black);
        page.render(renderer);
        renderer.endFrame();
    };

    section("Page: floor ordering");
    {
        tuneui::Page page;
        page.addFloor(tuneui::Floor("top", 10)
                          .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red));
        page.addFloor(tuneui::Floor("bottom", 0)
                          .addRect({100.0f, 100.0f, 100.0f, 100.0f}, blue));
        renderPage(page);
        checkPixel("higher level draws on top regardless of add order",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);

        tuneui::Page reversed;
        reversed.addFloor(tuneui::Floor("bottom", 0)
                              .addRect({100.0f, 100.0f, 100.0f, 100.0f}, blue));
        reversed.addFloor(tuneui::Floor("top", 10)
                              .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red));
        renderPage(reversed);
        checkPixel("same result when levels are added in the other order",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);
    }

    section("Page: equal levels keep insertion order");
    {
        tuneui::Page page;
        page.addFloor(tuneui::Floor("first")
                          .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red));
        page.addFloor(tuneui::Floor("second")
                          .addRect({100.0f, 100.0f, 100.0f, 100.0f}, blue));
        renderPage(page);
        checkPixel("later insertion wins at the same level",
                   readPixel(fbHeight, 150, 150), 0, 0, 255);

        tuneui::Page flipped;
        flipped.addFloor(tuneui::Floor("first")
                             .addRect({100.0f, 100.0f, 100.0f, 100.0f}, blue));
        flipped.addFloor(tuneui::Floor("second")
                             .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red));
        renderPage(flipped);
        checkPixel("insertion order follows the add order",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);
    }

    section("Page: visibility and background");
    {
        tuneui::Page page;
        page.addFloor(tuneui::Floor("visible")
                          .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red));
        page.addFloor(tuneui::Floor("hidden", 1)
                          .setVisible(false)
                          .addRect({100.0f, 100.0f, 100.0f, 100.0f}, blue));
        renderPage(page);
        checkPixel("hidden floor is skipped", readPixel(fbHeight, 150, 150), 255, 0, 0);

        tuneui::Page empty;
        empty.setBackground({0.1f, 0.2f, 0.3f, 1.0f});
        renderPage(empty);
        checkPixel("background fills the empty page", readPixel(fbHeight, 400, 300), 26, 51, 77);
        CHECK(renderer.drawCallCount() == 0);

        tuneui::Page noBackground;
        noBackground.addFloor(tuneui::Floor("hidden").setVisible(false));
        renderer.beginFrame(fbWidth, fbHeight, 1.0f);
        renderer.clear({0.0f, 1.0f, 0.0f, 1.0f});
        noBackground.render(renderer);
        renderer.endFrame();
        checkPixel("page without background leaves prior content",
                   readPixel(fbHeight, 400, 300), 0, 255, 0);
        CHECK(renderer.drawCallCount() == 0);
    }

    section("Page: clip intersection");
    {
        tuneui::Page page;
        page.addFloor(tuneui::Floor("clipped")
                          .setClip({100.0f, 100.0f, 100.0f, 100.0f})
                          .addRect({50.0f, 50.0f, 200.0f, 200.0f}, white));
        renderPage(page);
        checkPixel("inside clip is drawn", readPixel(fbHeight, 150, 150), 255, 255, 255);
        checkPixel("quad area outside clip is cut", readPixel(fbHeight, 60, 60), 0, 0, 0);
        checkPixel("outside the quad stays clear", readPixel(fbHeight, 250, 250), 0, 0, 0);

        tuneui::Page offscreen;
        offscreen.addFloor(tuneui::Floor("clipped")
                               .setClip({pageWidth - 100.0f, pageHeight - 100.0f,
                                         400.0f, 400.0f})
                               .addRect({0.0f, 0.0f, pageWidth, pageHeight}, white));
        renderPage(offscreen);
        checkPixel("clip is intersected with the page bounds",
                   readPixel(fbHeight, fbWidth - 50, fbHeight - 50), 255, 255, 255);
        checkPixel("page-side of the clip is untouched",
                   readPixel(fbHeight, fbWidth - 150, fbHeight - 50), 0, 0, 0);

        tuneui::Page fullyOffscreen;
        fullyOffscreen.addFloor(tuneui::Floor("clipped")
                                    .setClip({pageWidth + 100.0f, 100.0f, 100.0f, 100.0f})
                                    .addRect({0.0f, 0.0f, pageWidth, pageHeight}, white));
        renderPage(fullyOffscreen);
        CHECK(renderer.drawCallCount() == 0);

        tuneui::Page fullPageClip;
        fullPageClip.addFloor(tuneui::Floor("clipped")
                                  .setClip({0.0f, 0.0f, pageWidth, pageHeight})
                                  .addRect({100.0f, 100.0f, 100.0f, 100.0f}, white));
        renderPage(fullPageClip);
        checkPixel("clip covering the whole page still draws",
                   readPixel(fbHeight, 150, 150), 255, 255, 255);
        CHECK(renderer.drawCallCount() == 1); // no scissor transition needed
    }

    section("Page: shared texture ownership");
    {
        auto texture = std::make_shared<tuneui::Texture>();
        const unsigned char redTexel[4] = {255, 0, 0, 255};
        CHECK(texture->create(redTexel, 1, 1, false,
                              tuneui::TextureFilter::Nearest,
                              tuneui::TextureWrap::ClampToEdge));

        tuneui::Page page;
        page.addFloor(tuneui::Floor("textured")
                          .addTexturedRect({100.0f, 100.0f, 100.0f, 100.0f},
                                           {0.0f, 0.0f, 1.0f, 1.0f},
                                           white, texture));
        renderPage(page);
        checkPixel("textured floor draws",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);

        // The page holds its own reference; dropping the caller's shared_ptr
        // keeps the texture (and its GL id) alive.
        tuneui::Texture* held = texture.get();
        texture.reset();
        renderPage(page);
        checkPixel("floor keeps the texture alive after caller releases it",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);

        // Recreating the same Texture object changes the GL id; replay must
        // resolve the id each frame instead of replaying a stale one.
        const unsigned char blueTexel[4] = {0, 0, 255, 255};
        CHECK(held->create(blueTexel, 1, 1, false,
                           tuneui::TextureFilter::Nearest,
                           tuneui::TextureWrap::ClampToEdge));
        renderPage(page);
        checkPixel("recreated texture is resolved at replay time",
                   readPixel(fbHeight, 150, 150), 0, 0, 255);
    }

    section("Page: preserves the caller's outer scissor");
    {
        tuneui::Page page;
        page.setBackground({0.0f, 1.0f, 0.0f, 1.0f});

        CHECK(renderer.beginFrame(fbWidth, fbHeight, 1.0f));
        renderer.clear(black);
        renderer.setScissor({100.0f, 100.0f, 100.0f, 100.0f});
        CHECK(renderer.scissorDepth() == 1);
        page.render(renderer);
        CHECK(renderer.scissorDepth() == 1); // page did not pop the caller's clip
        renderer.endFrame();
        CHECK(renderer.scissorDepth() == 0);

        checkPixel("page background honors the outer scissor",
                   readPixel(fbHeight, 150, 150), 0, 255, 0);
        checkPixel("background does not clear outside the outer scissor",
                   readPixel(fbHeight, 50, 50), 0, 0, 0);
    }

    section("Page: floor offset");
    {
        tuneui::Page page;
        page.addFloor(tuneui::Floor("moved")
                          .setOffset({30.0f, 20.0f})
                          .addRect({10.0f, 10.0f, 20.0f, 20.0f}, red));
        renderPage(page);
        checkPixel("offset shifts the content", readPixel(fbHeight, 45, 35), 255, 0, 0);
        checkPixel("original position is empty", readPixel(fbHeight, 15, 15), 0, 0, 0);

        tuneui::Page clippedScroll;
        clippedScroll.addFloor(tuneui::Floor("scrolled")
                                   .setClip({0.0f, 0.0f, 100.0f, 100.0f})
                                   .setOffset({30.0f, 20.0f})
                                   .addRect({10.0f, 10.0f, 200.0f, 200.0f}, white));
        renderPage(clippedScroll);
        checkPixel("content moved by the offset", readPixel(fbHeight, 95, 95), 255, 255, 255);
        checkPixel("clip stays fixed in page space", readPixel(fbHeight, 105, 95), 0, 0, 0);
    }

    section("Page: batching across floors");
    {
        tuneui::Page unclipped;
        unclipped.addFloor(tuneui::Floor("", 0)
                               .addRect({10.0f, 10.0f, 50.0f, 50.0f}, red));
        unclipped.addFloor(tuneui::Floor("", 1)
                               .addRect({70.0f, 10.0f, 50.0f, 50.0f}, blue));
        unclipped.addFloor(tuneui::Floor("", 2)
                               .addRect({130.0f, 10.0f, 50.0f, 50.0f}, white));
        renderPage(unclipped);
        CHECK(renderer.drawCallCount() == 1); // one batch, no scissor transitions

        tuneui::Page mixed;
        mixed.addFloor(tuneui::Floor("", 0)
                           .setClip({0.0f, 0.0f, 200.0f, 200.0f})
                           .addRect({10.0f, 10.0f, 50.0f, 50.0f}, red));
        mixed.addFloor(tuneui::Floor("", 1)
                           .addRect({70.0f, 10.0f, 50.0f, 50.0f}, blue));
        renderPage(mixed);
        CHECK(renderer.drawCallCount() == 2); // clipped run, then unclipped run

        tuneui::Page sameClip;
        sameClip.addFloor(tuneui::Floor("", 0)
                              .setClip({0.0f, 0.0f, 200.0f, 200.0f})
                              .addRect({10.0f, 10.0f, 50.0f, 50.0f}, red));
        sameClip.addFloor(tuneui::Floor("", 1)
                              .setClip({0.0f, 0.0f, 200.0f, 200.0f})
                              .addRect({70.0f, 10.0f, 50.0f, 50.0f}, blue));
        renderPage(sameClip);
        CHECK(renderer.drawCallCount() == 1); // identical clip -> no flush

        tuneui::Page emptyClipped;
        emptyClipped.addFloor(tuneui::Floor("empty")
                                  .setClip({0.0f, 0.0f, 200.0f, 200.0f}));
        emptyClipped.addFloor(tuneui::Floor("visible")
                                  .addRect({10.0f, 10.0f, 50.0f, 50.0f}, red));
        renderPage(emptyClipped);
        CHECK(renderer.drawCallCount() == 1); // empty floor causes no scissor flush

        tuneui::Page differentClips;
        differentClips.addFloor(tuneui::Floor("a", 0)
                                    .setClip({0.0f, 0.0f, 100.0f, 100.0f})
                                    .addRect({10.0f, 10.0f, 50.0f, 50.0f}, red));
        differentClips.addFloor(tuneui::Floor("b", 1)
                                    .setClip({100.0f, 0.0f, 100.0f, 100.0f})
                                    .addRect({110.0f, 10.0f, 50.0f, 50.0f}, blue));
        renderPage(differentClips);
        CHECK(renderer.drawCallCount() == 2); // clip change flushes
    }

    section("Page: floor storage stays stable");
    {
        tuneui::Page page;
        tuneui::Floor kept("kept");
        kept.addRect({100.0f, 100.0f, 100.0f, 100.0f}, red);
        page.addFloor(kept);
        const tuneui::Floor* floor = page.findFloor("kept");
        for (int i = 1; i < 20; ++i) {
            page.addFloor(tuneui::Floor("", i));
        }
        CHECK(page.floorCount() == 20);
        CHECK(page.findFloor("kept") == floor);
        renderPage(page);
        checkPixel("stored floor keeps its address across later addFloor calls",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);
    }

    section("Page: stand-alone floor configuration");
    {
        tuneui::Page page;
        tuneui::Floor config("config");
        config.setLevel(5)
              .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red);

        page.addFloor(config);
        const tuneui::Floor* added = page.findFloor("config");
        CHECK(added != nullptr);
        CHECK(added != nullptr && added->name() == "config");
        CHECK(added != nullptr && added->level() == 5);
        renderPage(page);
        checkPixel("configured floor is handed over and drawn",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);

        // addFloor takes its argument by value, so later edits to the source
        // do not reach the copy stored in the page.
        config.setVisible(false).addRect({300.0f, 300.0f, 50.0f, 50.0f}, blue);
        renderPage(page);
        checkPixel("source object stays detached after the copy",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);
        checkPixel("edits on the source are not rendered",
                   readPixel(fbHeight, 320, 320), 0, 0, 0);

        // Temporaries are accepted too, and setLevel controls ordering.
        tuneui::Page ordered;
        ordered.addFloor(
            tuneui::Floor("base").setLevel(0)
                .addRect({100.0f, 100.0f, 100.0f, 100.0f}, blue));
        ordered.addFloor(
            tuneui::Floor("top").setLevel(10)
                .addRect({100.0f, 100.0f, 100.0f, 100.0f}, red));
        renderPage(ordered);
        checkPixel("setLevel orders stand-alone floors",
                   readPixel(fbHeight, 150, 150), 255, 0, 0);
    }
}

bool runAutomatedChecks(tuneui::Renderer& renderer, int fbWidth, int fbHeight) {
    if (fbWidth < 800 || fbHeight < 600) {
        std::printf("  framebuffer too small for the fixed test coordinates (%dx%d; need 800x600)\n",
                    fbWidth, fbHeight);
        return false;
    }

    testBatchAndBlending(renderer, fbWidth, fbHeight);
    testTextures(renderer, fbWidth, fbHeight);
    testImageDecoding(renderer, fbWidth, fbHeight);
    testScissor(renderer, fbWidth, fbHeight);
    testFrameState(renderer, fbWidth, fbHeight);
    testCapacityFlush(renderer, fbWidth, fbHeight);
    testNestedScissorAndDpi(renderer, fbWidth, fbHeight);
    testTextureRegions(renderer, fbWidth, fbHeight);
    testFontAtlas(renderer, fbWidth, fbHeight);
    testPageComposition(renderer, fbWidth, fbHeight);

    section("Renderer: GL error state");
    CHECK(glGetError() == GL_NO_ERROR);
    return true;
}

void runLive(tuneui::Window& window, tuneui::Renderer& renderer) {
    section("Renderer: live demo (Esc or close the window)");

    while (!window.shouldClose()) {
        window.pollEvents();
        if (window.input().keyPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        const double time = window.time();
        const int liveWidth = window.framebufferWidth();
        const int liveHeight = window.framebufferHeight();
        if (!renderer.beginFrame(liveWidth, liveHeight, window.contentScale())) {
            window.input().endFrame();
            continue; // minimized / zero-size framebuffer
        }
        renderer.clear({0.05f, 0.06f, 0.08f, 1.0f});

        for (int i = 0; i < 100; ++i) {
            const float x = 20.0f + static_cast<float>(i % 10) * 36.0f;
            const float y = 20.0f + static_cast<float>(i / 10) * 24.0f;
            const float phase = static_cast<float>(time) * 2.0f + static_cast<float>(i) * 0.3f;
            const tuneui::Color color{
                static_cast<float>(i % 10) / 9.0f,
                static_cast<float>(i / 10) / 9.0f,
                0.5f + 0.5f * std::sin(phase),
                0.5f,
            };
            renderer.drawRect({x, y, 30.0f, 20.0f}, color);
        }

        renderer.endFrame();
        window.input().endFrame();
        window.swapBuffers();
    }
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    bool live = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--live") == 0) {
            live = true;
        }
    }

    std::printf("TuneUI renderer test\n");

    testOrtho();

    tuneui::Window window;
    if (!window.init(800, 600, "TuneUI renderer test")) {
        std::printf("[FAIL] window.init(800, 600, ...)\n");
        std::printf("\n%d checks, %d failure(s)\n", checkCount, failureCount + 1);
        return 1;
    }

    const int fbWidth = window.framebufferWidth();
    const int fbHeight = window.framebufferHeight();
    std::printf("  framebuffer: %dx%d\n", fbWidth, fbHeight);

    tuneui::Renderer renderer;
    section("Renderer: init");
    CHECK(renderer.init());
    CHECK(renderer.isValid());
    CHECK(renderer.whiteTextureId() != 0);
    CHECK(renderer.shader().isValid());

    if (renderer.isValid()) {
        runAutomatedChecks(renderer, fbWidth, fbHeight);
        if (live) {
            runLive(window, renderer);
        }
    }

    renderer.shutdown();

    std::printf("\n%d checks, %d failure(s)\n", checkCount, failureCount);
    return failureCount == 0 ? 0 : 1;
}
