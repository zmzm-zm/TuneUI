cmake_minimum_required(VERSION 3.21)

project(TuneUI
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ============================================================
# 1. Compiler configuration
# ============================================================

if(MSVC)
    message(FATAL_ERROR
        "TuneUI currently supports MinGW-w64 GCC only."
    )
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 2. Installation helpers
# ============================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

# ============================================================
# 3. Project paths
# ============================================================

set(TUNEUI_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(THIRD_PARTY_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
)

set(THIRD_PARTY_INCLUDE_DIR
    "${THIRD_PARTY_DIR}/include"
)

set(THIRD_PARTY_SRC_DIR
    "${THIRD_PARTY_DIR}/src"
)

# ============================================================
# 4. Dependencies
# ============================================================

# GLFW is built from source and installed alongside TuneUI.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw)

# ============================================================
# 5. TuneUI library
# ============================================================

add_library(TuneUI STATIC
    # Third-party C sources
    "${THIRD_PARTY_SRC_DIR}/glad.c"
    "${THIRD_PARTY_SRC_DIR}/stb_image.c"
    "${THIRD_PARTY_SRC_DIR}/stb_truetype.c"

    # Platform
    src/platform/InputState.cpp
    src/platform/Window.cpp

    # Rendering
    src/render/Shader.cpp
    src/render/Texture.cpp
    src/render/Renderer.cpp
    src/render/Page.cpp
)

add_library(TuneUI::TuneUI ALIAS TuneUI)

target_compile_features(TuneUI
    PUBLIC
        cxx_std_20
)

# ============================================================
# 6. Include directories
# ============================================================

target_include_directories(TuneUI
    PUBLIC
        $<BUILD_INTERFACE:${TUNEUI_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# GLM is used by public headers. The current public include root
# also exposes GLAD, stb and nlohmann/json.
target_include_directories(TuneUI SYSTEM
    PUBLIC
        $<BUILD_INTERFACE:${THIRD_PARTY_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# ============================================================
# 7. Link dependencies
# ============================================================

# Window.cpp uses GLFW internally. Because TuneUI is static,
# GLFW remains a final-link dependency of consuming applications.
target_link_libraries(TuneUI
    PRIVATE
        glfw
)

if(WIN32)
    target_link_libraries(TuneUI
        PRIVATE
            opengl32
            gdi32
            user32
            shell32
            winmm
    )
endif()

# ============================================================
# 8. Compiler warnings
# ============================================================

target_compile_options(TuneUI
    PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
)

# ============================================================
# 9. Install TuneUI
# ============================================================

install(
    TARGETS TuneUI
    EXPORT TuneUITargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Install TuneUI public headers.
install(
    DIRECTORY "${TUNEUI_INCLUDE_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Install third-party headers required by the current public
# include surface (GLM, GLAD, stb and nlohmann/json).
install(
    DIRECTORY "${THIRD_PARTY_INCLUDE_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Export the namespaced target TuneUI::TuneUI.
install(
    EXPORT TuneUITargets
    FILE TuneUITargets.cmake
    NAMESPACE TuneUI::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TuneUI"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TuneUIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TuneUIConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TuneUI"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TuneUIConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/TuneUIConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/TuneUIConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TuneUI"
)

# GLFW_INSTALL is ON above, so GLFW's own install rules are also
# included in this install step and use the same install prefix.

# ============================================================
# 10. Examples
# ============================================================

option(
    TUNEUI_BUILD_EXAMPLES
    "Build TuneUI example programs"
    OFF
)

if(TUNEUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================
# 11. Tests
# ============================================================

include(CTest)

option(
    TUNEUI_BUILD_TESTS
    "Build TuneUI tests"
    OFF
)

if(TUNEUI_BUILD_TESTS AND BUILD_TESTING)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()
