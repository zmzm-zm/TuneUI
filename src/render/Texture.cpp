#include <glad/glad.h>

#include <tuneui/render/Texture.h>

#include <stb/stb_image.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace tuneui {

namespace {

GLint toGlFilter(TextureFilter filter) {
    return filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
}

GLint toGlWrap(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Repeat:
            return GL_REPEAT;
        case TextureWrap::MirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge:
        default:
            return GL_CLAMP_TO_EDGE;
    }
}

// Converts premultiplied RGBA8 to straight alpha so the renderer's
// SRC_ALPHA / ONE_MINUS_SRC_ALPHA blending produces correct results.
std::vector<uint8_t> toStraightAlpha(const uint8_t* pixels, size_t pixelCount) {
    std::vector<uint8_t> straight(pixels, pixels + pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t* p = straight.data() + i * 4;
        const unsigned int alpha = p[3];
        if (alpha == 0) {
            p[0] = p[1] = p[2] = 0;
            continue;
        }
        for (int c = 0; c < 3; ++c) {
            const unsigned int value = (static_cast<unsigned int>(p[c]) * 255u + alpha / 2u) / alpha;
            p[c] = static_cast<uint8_t>(std::min(value, 255u));
        }
    }
    return straight;
}

// Reverses row order in place. Done here rather than through
// stbi_set_flip_vertically_on_load so no global stb state is touched.
void flipRows(std::vector<uint8_t>& pixels, int width, int height) {
    const size_t rowBytes = static_cast<size_t>(width) * 4u;
    std::vector<uint8_t> row(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        uint8_t* top = pixels.data() + static_cast<size_t>(y) * rowBytes;
        uint8_t* bottom = pixels.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
        std::copy(top, top + rowBytes, row.begin());
        std::copy(bottom, bottom + rowBytes, top);
        std::copy(row.begin(), row.end(), bottom);
    }
}

// Decodes a file with stb_image into a top-row-first RGBA buffer.
bool decodeFile(const char* path, int& width, int& height,
                std::vector<uint8_t>& out) {
    int channels = 0;
    stbi_uc* data = stbi_load(path, &width, &height, &channels, 4);
    if (data == nullptr) {
        return false;
    }
    out.assign(data, data + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    stbi_image_free(data);
    return true;
}

// Decodes with stb_image into a top-row-first RGBA buffer.
bool decode(const uint8_t* encoded, int length, int& width, int& height,
            std::vector<uint8_t>& out) {
    int channels = 0;
    stbi_uc* data = stbi_load_from_memory(encoded, length, &width, &height, &channels, 4);
    if (data == nullptr) {
        return false;
    }
    out.assign(data, data + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    stbi_image_free(data);
    return true;
}

} // namespace

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : id_(other.id_), width_(other.width_), height_(other.height_) {
    other.id_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        id_ = other.id_;
        width_ = other.width_;
        height_ = other.height_;
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

bool Texture::create(const void* rgbaPixels, int width, int height,
                     bool sourcePremultiplied, TextureFilter filter,
                     TextureWrap wrap) {
    if (rgbaPixels == nullptr || width <= 0 || height <= 0) {
        destroy();
        return false;
    }

    const auto* pixels = static_cast<const uint8_t*>(rgbaPixels);

    std::vector<uint8_t> converted;
    if (sourcePremultiplied) {
        converted = toStraightAlpha(pixels, static_cast<size_t>(width) * static_cast<size_t>(height));
        pixels = converted.data();
    }

    return upload(pixels, width, height, filter, wrap);
}

bool Texture::createFromFile(const char* path, TextureFilter filter,
                             TextureWrap wrap, bool flipVertically) {
    if (path == nullptr) {
        destroy();
        return false;
    }

    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
    if (!decodeFile(path, width, height, pixels)) {
        std::fprintf(stderr, "TuneUI: failed to load image '%s': %s\n",
                     path, stbi_failure_reason());
        destroy();
        return false;
    }

    if (flipVertically) {
        flipRows(pixels, width, height);
    }
    return upload(pixels.data(), width, height, filter, wrap);
}

bool Texture::createFromMemory(const void* encodedBytes, std::size_t byteCount,
                               TextureFilter filter, TextureWrap wrap,
                               bool flipVertically) {
    if (encodedBytes == nullptr || byteCount == 0 || byteCount > INT_MAX) {
        destroy();
        return false;
    }

    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
    if (!decode(static_cast<const uint8_t*>(encodedBytes),
                static_cast<int>(byteCount), width, height, pixels)) {
        std::fprintf(stderr, "TuneUI: failed to decode image from memory: %s\n",
                     stbi_failure_reason());
        destroy();
        return false;
    }

    if (flipVertically) {
        flipRows(pixels, width, height);
    }
    return upload(pixels.data(), width, height, filter, wrap);
}

bool Texture::createBlank(int width, int height, TextureFilter filter,
                          TextureWrap wrap, const void* initialRgba) {
    if (width <= 0 || height <= 0) {
        destroy();
        return false;
    }
    // glTexImage2D accepts nullptr, which leaves the texel storage undefined.
    return upload(initialRgba, width, height, filter, wrap);
}

bool Texture::updateRegion(int x, int y, int width, int height,
                           const void* rgbaPixels) {
    if (id_ == 0 || rgbaPixels == nullptr || width <= 0 || height <= 0 ||
        x < 0 || y < 0 || x > width_ - width || y > height_ - height) {
        return false;
    }

    GLint previousTexture = 0;
    GLint previousAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);

    glBindTexture(GL_TEXTURE_2D, id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    // Do not leak upload state into user-drawn floors or later uploads.
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    return true;
}

bool Texture::upload(const void* rgbaPixels, int width, int height,
                     TextureFilter filter, TextureWrap wrap) {
    destroy();

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return false;
    }

    GLint previousTexture = 0;
    GLint previousAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);

    const GLint glFilter = toGlFilter(filter);
    const GLint glWrap = toGlWrap(wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrap);

    glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    id_ = texture;
    width_ = width;
    height_ = height;
    return true;
}

void Texture::destroy() {
    if (id_ != 0) {
        const GLuint texture = id_;
        glDeleteTextures(1, &texture);
        id_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace tuneui
