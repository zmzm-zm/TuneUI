#include <glad/glad.h>

#include <tuneui/render/Renderer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace tuneui {

namespace {

constexpr const char* VertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
}
)glsl";

constexpr const char* FragmentShaderSource = R"glsl(
#version 330 core
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    FragColor = texture(uTexture, vUV) * vColor;
}
)glsl";

double clampDouble(double value, double low, double high) {
    return std::max(low, std::min(value, high));
}

} // namespace

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init() {
    if (valid_) {
        return true;
    }

    if (!shader_.compile(VertexShaderSource, FragmentShaderSource)) {
        std::fprintf(stderr, "TuneUI: UI shader failed to compile\n%s", shader_.log().c_str());
        return false;
    }
    shader_.use();
    shader_.setInt("uTexture", 0);

    const unsigned char white[4] = {255, 255, 255, 255};
    if (!whiteTexture_.create(white, 1, 1, false,
                              TextureFilter::Nearest, TextureWrap::ClampToEdge)) {
        std::fprintf(stderr, "TuneUI: failed to create the white texture\n");
        shader_.destroy();
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (vao_ == 0 || vbo_ == 0) {
        std::fprintf(stderr, "TuneUI: failed to create VAO/VBO\n");
        // shutdown() deletes whichever names were created and releases the
        // shader/white texture, so a retry starts from a clean slate.
        shutdown();
        return false;
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(Vertex) * MaxVerticesPerBatch),
                 nullptr, GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, r)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    valid_ = true;
    return true;
}

void Renderer::shutdown() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    whiteTexture_.destroy();
    shader_.destroy();
    vertices_.clear();
    scissorStack_.clear();
    batchTexture_ = 0;
    scissorEnabled_ = false;
    frameActive_ = false;
    valid_ = false;
}

bool Renderer::beginFrame(int framebufferWidth, int framebufferHeight, float dpiScale) {
    if (!valid_ || framebufferWidth <= 0 || framebufferHeight <= 0) {
        return false;
    }

    // A second beginFrame() must not silently discard queued geometry.
    if (frameActive_) {
        flushBatch();
    }

    framebufferWidth_ = framebufferWidth;
    framebufferHeight_ = framebufferHeight;
    dpiScale_ = (std::isfinite(dpiScale) && dpiScale > 0.0f) ? dpiScale : 1.0f;

    vertices_.clear();
    scissorStack_.clear();
    batchTexture_ = 0;
    drawCalls_ = 0;
    scissorEnabled_ = false;
    frameActive_ = true;

    glViewport(0, 0, framebufferWidth_, framebufferHeight_);
    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    shader_.use();
    shader_.setInt("uTexture", 0);
    shader_.setMat4("uProjection",
                    orthoPixels(static_cast<float>(framebufferWidth_),
                                static_cast<float>(framebufferHeight_),
                                dpiScale_));

    glBindVertexArray(vao_);
    return true;
}

void Renderer::endFrame() {
    if (!frameActive_) {
        return;
    }
    flushBatch();
    scissorStack_.clear();
    disableScissor();
    frameActive_ = false;
}

void Renderer::drawRect(const Rect& rect, const Color& color) {
    appendQuad(rect, Rect{0.0f, 0.0f, 1.0f, 1.0f}, color, whiteTexture_.id());
}

void Renderer::drawTexturedRect(const Rect& dst, const Rect& uv, const Color& tint,
                                unsigned int textureId) {
    const unsigned int texture = textureId != 0 ? textureId : whiteTexture_.id();
    appendQuad(dst, uv, tint, texture);
}

void Renderer::appendQuad(const Rect& dst, const Rect& uv, const Color& color,
                          unsigned int textureId) {
    if (!valid_ || !frameActive_) {
        return;
    }

    if (batchTexture_ != textureId && !vertices_.empty()) {
        flushBatch();
    }
    batchTexture_ = textureId;

    if (static_cast<int>(vertices_.size()) + 6 > MaxVerticesPerBatch) {
        flushBatch();
    }

    const float x0 = dst.x;
    const float y0 = dst.y;
    const float x1 = dst.x + dst.width;
    const float y1 = dst.y + dst.height;

    // uv is normalized with its origin at the texture's top-left. Texture data
    // row 0 is uploaded as the first texel row, which is what v = 0 samples,
    // so the mapping needs no flip.
    const float u0 = uv.x;
    const float v0 = uv.y;
    const float u1 = uv.x + uv.width;
    const float v1 = uv.y + uv.height;

    pushVertex(x0, y0, u0, v0, color);
    pushVertex(x0, y1, u0, v1, color);
    pushVertex(x1, y1, u1, v1, color);

    pushVertex(x0, y0, u0, v0, color);
    pushVertex(x1, y1, u1, v1, color);
    pushVertex(x1, y0, u1, v0, color);
}

void Renderer::pushVertex(float x, float y, float u, float v, const Color& color) {
    vertices_.push_back(Vertex{x, y, u, v, color.r, color.g, color.b, color.a});
}

void Renderer::flush() {
    if (!frameActive_) {
        return;
    }
    flushBatch();
}

void Renderer::flushBatch() {
    if (vertices_.empty()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, batchTexture_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const GLsizeiptr bytes = static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex));
    // Orphan the previous storage, then upload the new batch into it.
    glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices_.data());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    ++drawCalls_;

    vertices_.clear();
}

void Renderer::clear(const Color& color) {
    if (!valid_ || !frameActive_) {
        return;
    }
    // Keep draw order: queued geometry must be on screen before the clear.
    flushBatch();
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::setScissor(const Rect& rect) {
    if (!valid_ || !frameActive_) {
        return;
    }
    // Replace semantics: the old stack is irrelevant once this clip is active.
    flushBatch();
    scissorStack_.clear();
    pushScissor(rect);
}

void Renderer::pushScissor(const Rect& rect) {
    if (!valid_ || !frameActive_) {
        return;
    }

    // Quads already queued were drawn under the previous clip; emit them first.
    flushBatch();

    const Rect base = scissorStack_.empty() ? logicalBounds() : scissorStack_.back();
    Rect clipped{};
    if (!intersectRects(base, rect, clipped)) {
        // Keep push/pop symmetric even for an empty or non-finite region.
        clipped = Rect{0.0f, 0.0f, 0.0f, 0.0f};
    }
    scissorStack_.push_back(clipped);
    applyScissor(clipped);
}

void Renderer::popScissor() {
    if (!valid_ || !frameActive_ || scissorStack_.empty()) {
        return;
    }
    flushBatch();
    scissorStack_.pop_back();
    if (scissorStack_.empty()) {
        disableScissor();
    } else {
        applyScissor(scissorStack_.back());
    }
}

void Renderer::clearScissor() {
    if (!valid_ || !frameActive_) {
        return;
    }
    flushBatch();
    scissorStack_.clear();
    disableScissor();
}

Rect Renderer::currentScissor() const {
    return scissorStack_.empty() ? logicalBounds() : scissorStack_.back();
}

void Renderer::applyScissor(const Rect& rect) {
    const Rect normalized = normalizeRect(rect);
    if (!isFiniteRect(normalized) || rectEmpty(normalized)) {
        // An empty clip must still enable GL_SCISSOR_TEST so nothing draws.
        glScissor(0, 0, 0, 0);
        glEnable(GL_SCISSOR_TEST);
        scissorEnabled_ = true;
        return;
    }

    // Compute in double, clamp before the int cast: a NaN or huge layout value
    // must never reach static_cast<int> (undefined behavior).
    const double fbWidth = static_cast<double>(framebufferWidth_);
    const double fbHeight = static_cast<double>(framebufferHeight_);
    const double scale = static_cast<double>(dpiScale_);

    const double left = clampDouble(std::floor(static_cast<double>(normalized.x) * scale),
                                    0.0, fbWidth);
    const double right = clampDouble(std::ceil(static_cast<double>(normalized.x + normalized.width) * scale),
                                     0.0, fbWidth);
    const double top = clampDouble(std::floor(static_cast<double>(normalized.y) * scale),
                                   0.0, fbHeight);
    const double bottom = clampDouble(std::ceil(static_cast<double>(normalized.y + normalized.height) * scale),
                                      0.0, fbHeight);

    const int x0 = static_cast<int>(left);
    const int x1 = static_cast<int>(right);
    const int y0 = static_cast<int>(top);
    const int y1 = static_cast<int>(bottom);
    const int width = std::max(0, x1 - x0);
    const int height = std::max(0, y1 - y0);

    glScissor(x0, framebufferHeight_ - y1, width, height);
    glEnable(GL_SCISSOR_TEST);
    scissorEnabled_ = true;
}

void Renderer::disableScissor() {
    glDisable(GL_SCISSOR_TEST);
    scissorEnabled_ = false;
}

} // namespace tuneui
