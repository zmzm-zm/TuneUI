#pragma once

#include <cstddef>

namespace tuneui {

enum class TextureFilter {
    Nearest,
    Linear,
};

enum class TextureWrap {
    ClampToEdge,
    Repeat,
    MirroredRepeat,
};

// RGBA8 2D texture. Requires a current GL context with glad loaded; the
// context must outlive the texture.
//
// Pixel data is stored top-row-first, matching the renderer's UV convention
// (v = 0 is the top edge). File/memory decoding goes through stb_image.
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Uploads tightly packed RGBA8 pixels (row 0 is the top row of the image).
    //
    // sourcePremultiplied: true when the pixels already have RGB multiplied by
    // alpha. The renderer blends with GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA, so
    // premultiplied input is converted back to straight alpha on upload.
    //
    // Replaces any texture previously owned by this object.
    bool create(const void* rgbaPixels, int width, int height,
                bool sourcePremultiplied = false,
                TextureFilter filter = TextureFilter::Linear,
                TextureWrap wrap = TextureWrap::ClampToEdge);

    // Decodes an image file (PNG, JPG, BMP, TGA, ...) with stb_image and
    // uploads it. flipVertically flips the decoded rows before upload; leave it
    // off for images whose first row is the top of the picture (the usual case),
    // turn it on for formats written bottom-up.
    bool createFromFile(const char* path,
                        TextureFilter filter = TextureFilter::Linear,
                        TextureWrap wrap = TextureWrap::ClampToEdge,
                        bool flipVertically = false);

    // Same as createFromFile but decodes an in-memory encoded image, which is
    // what embedded resources use.
    bool createFromMemory(const void* encodedBytes, std::size_t byteCount,
                          TextureFilter filter = TextureFilter::Linear,
                          TextureWrap wrap = TextureWrap::ClampToEdge,
                          bool flipVertically = false);

    // Allocates an uninitialized RGBA8 texture (or fills it from initialRgba,
    // tightly packed, row 0 = top). Used for glyph/icon atlases that are filled
    // incrementally with updateRegion(). Replaces any previous storage.
    bool createBlank(int width, int height,
                     TextureFilter filter = TextureFilter::Linear,
                     TextureWrap wrap = TextureWrap::ClampToEdge,
                     const void* initialRgba = nullptr);

    // Uploads a tightly packed RGBA8 sub-rectangle into an existing texture.
    // Does not destroy the texture on failure. x/y are in texels, row 0 is the
    // top row. The region must lie completely inside the texture.
    bool updateRegion(int x, int y, int width, int height, const void* rgbaPixels);

    void destroy();

    bool isValid() const { return id_ != 0; }
    unsigned int id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    bool upload(const void* rgbaPixels, int width, int height,
                TextureFilter filter, TextureWrap wrap);

    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace tuneui
