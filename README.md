# TuneUI

「代码即布局 + 外部热调试」的轻量桌面 UI 库：用 C++ 写界面，启动 debugger 热调属性，定稿为二进制配置。

- **语言 / 平台**：C++20，仅 Windows；当前使用 MinGW-w64 GCC
- **渲染**：OpenGL 3.3 Core（GLFW + glad + glm）
- **库类型**：静态库 `TuneUI`
- **集成方式**：开发时可用 `add_subdirectory()`；安装后可用 `find_package(TuneUI CONFIG REQUIRED)`
- **当前状态**：第 1 阶段（平台层 + 渲染层 + Page/Floor）完成，并完成进入第 2 阶段前的 P0/P1 加固；下一步是组件树、布局与文本

## 范围

**目标用户**：不擅长 UI / 美术的 C++ 开发者，想快速做出能看、能调的桌面软件界面。

**核心工作流（目标形态）**：写 C++ 建组件 → 生成 JSON → debugger 热调属性（即改即看）→ 定稿二进制

## 快速开始

要求 **CMake ≥ 3.21**、**MinGW-w64 GCC** 与 C++20。当前只支持 MinGW 工具链，MSVC 配置会在 CMake 阶段直接报错。

### 构建 TuneUI

```bash
cmake -S . -B build-mingw -G "MinGW Makefiles" \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

cmake --build build-mingw -j8
```

默认不构建示例和测试。需要时启用：

```bash
cmake -S . -B build-mingw -G "MinGW Makefiles" \
      -DTUNEUI_BUILD_EXAMPLES=ON \
      -DTUNEUI_BUILD_TESTS=ON

cmake --build build-mingw -j8
```

首次 configure 会通过 FetchContent 获取并构建 **GLFW 3.5.1**。启用测试时还会获取 **GoogleTest v1.15.2**。因此首次配置需要网络。离线环境可以分别使用：

```bash
-DFETCHCONTENT_SOURCE_DIR_GLFW=<GLFW 源码目录>
-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<GoogleTest 源码目录>
```

将 FetchContent 指向已有源码目录。

> 用本地 GitHub 加速工具时，如果 FetchContent 下载失败，优先检查 Git 的代理与证书配置。不要把关闭 TLS 校验作为常规方案。

### 安装 TuneUI

TuneUI 安装后提供 CMake package，可供其他 CMake 项目使用 `find_package()`。

```bash
cmake -S . -B build-mingw -G "MinGW Makefiles" \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="D:/Libraries/TuneUI"

cmake --build build-mingw -j8
cmake --install build-mingw
```

安装前缀中会安装：

- TuneUI 静态库；
- `include/tuneui/` 公共头文件；
- 当前公开头文件依赖的第三方头文件（GLM、GLAD、stb、nlohmann/json）；
- `TuneUIConfig.cmake`、`TuneUIConfigVersion.cmake` 和导出的 `TuneUI::TuneUI` target；
- GLFW 3.5.1 的库、头文件与 CMake package（由 `GLFW_INSTALL=ON` 的安装规则提供）。

**注意：** TuneUI 是静态库，最终应用链接时仍需要 GLFW。当前安装方案把 GLFW 一并安装到同一个安装前缀；不要只复制 `libTuneUI.a` 而遗漏 GLFW 和 CMake package 文件。

### 在其他项目中使用

安装后，可以这样写：

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

find_package(TuneUI CONFIG REQUIRED)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE TuneUI::TuneUI)
```

配置消费者项目时，把 TuneUI 安装前缀加入 `CMAKE_PREFIX_PATH`：

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="D:/Libraries/TuneUI"
```

`TuneUIConfig.cmake` 会查找 GLFW 3.5 的 CMake package，并加载 TuneUI 导出的 target。消费者不需要手动添加 TuneUI、GLM 或 GLFW 的 include 路径。

如果希望在同一个构建树中直接开发 TuneUI，而不是先安装，也可以使用：

```cmake
add_subdirectory(path/to/TuneUI)
target_link_libraries(MyApp PRIVATE TuneUI::TuneUI)
```

### 构建选项

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `TUNEUI_BUILD_EXAMPLES` | `OFF` | 是否构建 `examples/` |
| `TUNEUI_BUILD_TESTS` | `OFF` | 是否获取 GoogleTest 并构建 `tests/` |
| `BUILD_TESTING` | 由 CTest 提供 | CTest 总开关；测试构建还要求 `TUNEUI_BUILD_TESTS=ON` |

关闭示例 / 测试无需额外操作；需要启用时在 configure 命令中设置对应选项为 `ON`。

### 运行示例

启用 `TUNEUI_BUILD_EXAMPLES=ON` 后：

```bash
./build-mingw/examples/tuneui_platform_test.exe          # 窗口 + 输入：自动断言后退出
./build-mingw/examples/tuneui_platform_test.exe --live   # 交互查看鼠标 / 键盘 / 滚轮 / 文本
./build-mingw/examples/tuneui_render_test.exe            # 渲染层：像素回读断言后退出
./build-mingw/examples/tuneui_render_test.exe --live     # 100 个动画四边形
./build-mingw/examples/tuneui_page_test.exe --frames 30  # Page / Floor 短时演示
```

示例内建断言：失败时打印 `[FAIL]` 并以非 0 退出。`tuneui_page_test` 不带 `--frames` 时会一直开着窗口。

### 运行测试

启用 `TUNEUI_BUILD_TESTS=ON` 后：

```bash
ctest --test-dir build-mingw --output-on-failure
```

测试条目数量以当前 `tests/` 注册结果为准。带 `requires-display` 标签的用例需要可用的图形显示环境；无显示环境时应明确记录跳过情况。

> 本文命令统一使用 `build-mingw/`。用 CLion 打开时 IDE 可能另建 `cmake-build-debug/`，把命令里的构建目录替换成实际目录即可。

## 最小示例

```cpp
#include <tuneui/platform/Window.h>
#include <tuneui/render/Renderer.h>

int main() {
    tuneui::Window window;
    if (!window.init(800, 600, "TuneUI")) return 1;

    tuneui::Renderer renderer;
    if (!renderer.init()) return 1;

    while (!window.shouldClose()) {
        window.pollEvents();
        if (!renderer.beginFrame(window.framebufferWidth(),
                                 window.framebufferHeight(),
                                 window.contentScale())) {
            window.input().endFrame();
            window.swapBuffers();
            continue;
        }

        renderer.clear({0.1f, 0.1f, 0.12f, 1.0f});
        renderer.drawRect({20, 20, 200, 60}, {0.2f, 0.6f, 1.0f, 1.0f});

        renderer.endFrame();
        window.input().endFrame();
        window.swapBuffers();
    }
    renderer.shutdown();
    return 0;
}
```

完整接口、语义与常见坑见 [API 文档](docs/API.md)；安装、导入与 CMake package 使用方式见 [API 文档 §1.1](docs/API.md#11-引入与链接)。

## 依赖

| 依赖 | 位置 / 管理方式 | 用途 |
| --- | --- | --- |
| GLFW 3.5.1 | FetchContent；安装时随 TuneUI 安装 | 窗口 / 输入 / GL 上下文 |
| GLAD | `third_party/include/glad`、`third_party/src/glad.c` | OpenGL 3.3 Core 函数加载 |
| GLM | `third_party/include/glm` | 数学：`vec2` / `vec4` / `mat4` / `ortho` |
| stb_image | `third_party/include/stb`、`third_party/src/stb_image.c` | 图像解码 |
| stb_truetype | `third_party/include/stb`、`third_party/src/stb_truetype.c` | 字形光栅化前置；字体排版尚未落地 |
| nlohmann/json | `third_party/include/nlohmann` | 已 vendored，后续阶段接入 |
| GoogleTest v1.15.2 | FetchContent；仅启用测试时获取 | `tests/` 单元测试框架 |

GLM、GLAD、stb 和 nlohmann/json 随 TuneUI 的公开头文件安装规则一起安装，不需要消费者另行下载。GLFW 则通过它自己的 CMake package 被 TuneUI 的 package config 找到。

## 文档

每类信息只有一个权威文件，其它文档只做链接，不复制正文：

| 文档 | 负责的内容 |
| --- | --- |
| [docs/API.md](docs/API.md) | 公开 API：类、函数、语义、坐标系、FAQ，以及 CMake 集成入口 |
| [docs/STYLE.md](docs/STYLE.md) | 命名规则与代码风格（贡献者） |
| [TODO.md](TODO.md) | 任务清单、优先级、里程碑、风险（唯一的计划来源） |
| [PROGRESS.md](PROGRESS.md) | 历史实施记录与当时的验证数字（按日期归档，不作现状依据） |
| [AGENTS.md](AGENTS.md) | 仓库地图、红线、开发流程（自动化代理入口） |
