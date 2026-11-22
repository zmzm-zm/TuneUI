#include <glad/glad.h>

#include <tuneui/render/Shader.h>

#include <glm/gtc/type_ptr.hpp>

#include <utility>
#include <vector>

namespace tuneui {

namespace {

GLuint compileStage(GLenum type, const char* source, std::string& log) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        log += "glCreateShader failed\n";
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> buffer(static_cast<size_t>(length > 1 ? length : 1), '\0');
        glGetShaderInfoLog(shader, length, nullptr, buffer.data());
        log += (type == GL_VERTEX_SHADER) ? "vertex shader: " : "fragment shader: ";
        log += buffer.data();
        log += '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : program_(other.program_), log_(std::move(other.log_)) {
    other.program_ = 0;
    other.log_.clear();
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        program_ = other.program_;
        log_ = std::move(other.log_);
        other.program_ = 0;
        other.log_.clear();
    }
    return *this;
}

bool Shader::compile(const char* vertexSource, const char* fragmentSource) {
    destroy();
    log_.clear();

    if (vertexSource == nullptr || fragmentSource == nullptr) {
        log_ = "null shader source\n";
        return false;
    }

    const GLuint vertex = compileStage(GL_VERTEX_SHADER, vertexSource, log_);
    if (vertex == 0) {
        return false;
    }

    const GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource, log_);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        log_ += "glCreateProgram failed\n";
        return false;
    }

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> buffer(static_cast<size_t>(length > 1 ? length : 1), '\0');
        glGetProgramInfoLog(program, length, nullptr, buffer.data());
        log_ += "link: ";
        log_ += buffer.data();
        log_ += '\n';
        glDeleteProgram(program);
        return false;
    }

    program_ = program;
    return true;
}

void Shader::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

void Shader::use() const {
    glUseProgram(program_);
}

int Shader::uniformLocation(const char* name) const {
    if (program_ == 0 || name == nullptr) {
        return -1;
    }
    return glGetUniformLocation(program_, name);
}

void Shader::setInt(const char* name, int value) {
    const int location = uniformLocation(name);
    if (location >= 0) {
        glUniform1i(location, value);
    }
}

void Shader::setFloat(const char* name, float value) {
    const int location = uniformLocation(name);
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

void Shader::setVec2(const char* name, const glm::vec2& value) {
    const int location = uniformLocation(name);
    if (location >= 0) {
        glUniform2f(location, value.x, value.y);
    }
}

void Shader::setVec4(const char* name, const Color& value) {
    const int location = uniformLocation(name);
    if (location >= 0) {
        glUniform4f(location, value.r, value.g, value.b, value.a);
    }
}

void Shader::setMat4(const char* name, const glm::mat4& value) {
    const int location = uniformLocation(name);
    if (location >= 0) {
        // glm is column-major, matching GL's expectation with transpose = false.
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}

} // namespace tuneui
