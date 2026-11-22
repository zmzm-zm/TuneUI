#pragma once

#include <string>

#include <tuneui/render/Math.h>

namespace tuneui {

// Thin wrapper around a GLSL program: compile, link, query uniforms.
// Requires a current GL context (created by Window) with glad loaded; the
// context must outlive the shader.
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Compiles and links the two stages. On failure returns false and stores
    // the compiler/linker log in log(). Any previous program is destroyed.
    bool compile(const char* vertexSource, const char* fragmentSource);

    void destroy();

    bool isValid() const { return program_ != 0; }
    unsigned int id() const { return program_; }

    void use() const;

    int uniformLocation(const char* name) const;
    void setInt(const char* name, int value);
    void setFloat(const char* name, float value);
    void setVec2(const char* name, const glm::vec2& value);
    void setVec4(const char* name, const Color& value);
    void setMat4(const char* name, const glm::mat4& value);

    // Compiler/linker output, empty when the last compile succeeded.
    const std::string& log() const { return log_; }

private:
    unsigned int program_ = 0;
    std::string log_;
};

} // namespace tuneui
