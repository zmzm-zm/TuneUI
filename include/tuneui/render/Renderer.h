#pragma once

#include <cstddef>
#include <vector>

#include <tuneui/render/Math.h>
#include <tuneui/render/Shader.h>
#include <tuneui/render/Texture.h>

namespace tuneui {

// Interleaved UI vertex: position (logical pixels), texture coordinate,
// straight-alpha RGBA color.
struct Vertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

// Batched 2D renderer for UI quads.
//
// Quads are accumulated into one dynamic vertex buffer and flushed when the
// bound texture changes or the batch fills up, so N quads that share a texture
// cost one draw call. Requires a current GL context with glad loaded.
//
// State assumed at flush time: blending on (SRC_ALPHA, ONE_MINUS_SRC_ALPHA),
// depth test off, culling off. beginFrame() sets these.
//
// The GL context must outlive this object: destroy the Renderer (and any
// Texture/Shader) before the Window that owns the context.
class Renderer {
public:
    // Six vertices per quad. 65532 is divisible by 6 (10922 quads) and stays
    // comfortably below driver limits.
    static constexpr int MaxVerticesPerBatch = 65532;
    static constexpr int MaxQuadsPerBatch = MaxVerticesPerBatch / 6;

    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init();
    void shutdown();
    bool isValid() const { return valid_; }

    // Starts a frame: sets viewport, projection, GL state, and resets the
    // batch, scissor stack, and draw-call counter. dpiScale maps logical pixels
    // to framebuffer pixels.
    //
    // Returns false (and leaves the previous frame state untouched) when the
    // renderer is not initialized or the framebuffer is empty (minimized
    // window). Drawing and state changes outside an active frame are no-ops.
    // A second beginFrame() flushes the pending batch first.
    bool beginFrame(int framebufferWidth, int framebufferHeight, float dpiScale = 1.0f);

    // Flushes any pending quads and ends the frame. Does not swap buffers.
    // Safe to call without a matching beginFrame().
    void endFrame();
    bool frameActive() const { return frameActive_; }

    // Solid-color quad in logical pixels.
    void drawRect(const Rect& rect, const Color& color);

    // Textured quad. uv is normalized [0,1] with the origin at the texture's
    // top-left corner, so {0, 0, 1, 1} maps the whole texture. tint multiplies
    // the sampled texel (use white for the unmodified image). textureId 0 falls
    // back to the built-in 1x1 white texture.
    void drawTexturedRect(const Rect& dst, const Rect& uv, const Color& tint,
                          unsigned int textureId);

    // Clears the framebuffer to the given color, honoring an active scissor.
    // Pending quads are flushed first so draw order stays correct.
    void clear(const Color& color);

    // Scissor rectangle in logical pixels. Converts to framebuffer coordinates
    // (y flipped) and enables GL_SCISSOR_TEST. Changing the scissor flushes the
    // current batch first, so earlier quads are never re-clipped.
    //
    // setScissor() replaces the whole stack (one level); pushScissor() nests and
    // intersects with the current clip, which is what ScrollView and clipped
    // children need. Empty/non-finite intersection still pushes an empty clip
    // so every push has a matching pop. The initial clip is logicalBounds().
    void setScissor(const Rect& rect);
    void pushScissor(const Rect& rect);
    void popScissor();
    void clearScissor();
    int scissorDepth() const { return static_cast<int>(scissorStack_.size()); }
    // Current logical clip; logicalBounds() when the stack is empty.
    Rect currentScissor() const;
    bool scissorEnabled() const { return scissorEnabled_; }

    // Emits the pending batch immediately. No-op outside an active frame.
    void flush();

    // Logical size of the drawable area (framebuffer size divided by the
    // dpiScale passed to beginFrame). Page uses this to clip to the screen.
    Rect logicalBounds() const {
        return Rect{0.0f, 0.0f,
                    static_cast<float>(framebufferWidth_) / dpiScale_,
                    static_cast<float>(framebufferHeight_) / dpiScale_};
    }

    // Number of glDrawArrays calls issued since beginFrame().
    int drawCallCount() const { return drawCalls_; }
    int pendingVertexCount() const { return static_cast<int>(vertices_.size()); }
    unsigned int whiteTextureId() const { return whiteTexture_.id(); }
    const Shader& shader() const { return shader_; }

private:
    void pushVertex(float x, float y, float u, float v, const Color& color);
    void appendQuad(const Rect& dst, const Rect& uv, const Color& color,
                    unsigned int textureId);
    void flushBatch();
    void applyScissor(const Rect& rect);
    void disableScissor();

    Shader shader_;
    Texture whiteTexture_;
    std::vector<Vertex> vertices_;
    std::vector<Rect> scissorStack_;

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int batchTexture_ = 0;

    int framebufferWidth_ = 0;
    int framebufferHeight_ = 0;
    float dpiScale_ = 1.0f;
    int drawCalls_ = 0;
    bool scissorEnabled_ = false;
    bool frameActive_ = false;
    bool valid_ = false;
};

} // namespace tuneui
