# TuneUI API 参考

面向使用 TuneUI 的应用开发者。本文件是公开接口与语义的**唯一来源**；当前覆盖第 1 阶段 + 进入第 2 阶段前的加固（平台层、渲染层、Page/Floor、DPI 与输入契约）。

- 头文件目录：`include/tuneui/`
- 命名空间：`tuneui`
- 编程标准：C++20，Windows + OpenGL 3.3 Core
- 构建、运行、依赖与已知约束：[README](../README.md)
- 命名与代码风格：[STYLE.md](STYLE.md)（贡献者）
- 后续阶段计划：[TODO.md](../TODO.md)；历史实现记录：[PROGRESS.md](../PROGRESS.md)（按日期归档，不作现状依据）

---

## 目录

1. [通用约定](#1-通用约定)
2. [Window](#2-window)
3. [InputState](#3-inputstate)
4. [Renderer](#4-renderer)
5. [Shader](#5-shader)
6. [Texture](#6-texture)
7. [Page / Floor](#7-page--floor)
8. [Math](#8-math)
9. [常见问题与坑](#9-常见问题与坑)

---

## 1. 通用约定

### 1.1 引入与链接

TuneUI 提供两种集成方式：

- **源码集成**：开发 TuneUI 或与应用同仓库开发时，使用 `add_subdirectory()`。
- **安装包集成**：将 TuneUI 安装到某个前缀后，使用 `find_package(TuneUI CONFIG REQUIRED)`。

#### 方式 A：add_subdirectory

```cmake
add_subdirectory(path/to/TuneUI)
target_link_libraries(my_app PRIVATE TuneUI::TuneUI)
```

`TuneUI::TuneUI` 是构建树中的别名目标。消费者不需要手动添加 TuneUI 的头文件目录或重复声明其传递依赖。

#### 方式 B：安装后 find_package

安装 TuneUI 后，消费者项目这样写：

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

find_package(TuneUI CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE TuneUI::TuneUI)
```

配置消费者项目时，将 TuneUI 的安装前缀加入 `CMAKE_PREFIX_PATH`：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/Libraries/TuneUI"
```

安装包提供以下 CMake 文件：

- `TuneUIConfig.cmake`：检查外部依赖并导入目标；
- `TuneUIConfigVersion.cmake`：版本兼容性检查；
- `TuneUITargets.cmake`：导出 `TuneUI::TuneUI`。

TuneUI 是静态库。其导出目标包含最终链接所需的 GLFW 依赖，因此 `TuneUIConfig.cmake` 会通过 `find_dependency(glfw3 3.5 CONFIG REQUIRED)` 查找 GLFW。当前安装方案启用 GLFW 自身的安装规则，并将 GLFW 安装到同一个前缀。消费者的 `CMAKE_PREFIX_PATH` 应能同时找到 TuneUI 与 GLFW 的 package 文件。

#### 头文件

```cpp
#include <tuneui/platform/Window.h>
#include <tuneui/platform/InputState.h>
#include <tuneui/render/Renderer.h>
#include <tuneui/render/Shader.h>
#include <tuneui/render/Texture.h>
#include <tuneui/render/Page.h>
#include <tuneui/render/Math.h>
```

#### 依赖与可见性

- TuneUI 是静态库；GLAD、stb_image、stb_truetype 编译进 `libTuneUI.a`。
- GLM 出现在公开数学接口中，因此 GLM 头文件目录属于公开 include 依赖。
- 当前 `third_party/include` 整体作为公开 include 根目录安装，因此 GLAD、stb、nlohmann/json 头文件也会被消费者访问；这是当前 include 表面的已知限制。
- GLFW 由 TuneUI 的 CMake 目标链接。静态库的最终链接阶段仍需要 GLFW；安装包通过 GLFW 3.5 的 CMake package 解析该依赖。
- Windows 系统链接依赖由 TuneUI 的导出链接接口带给最终应用。
- 需要 C++20。当前只支持 MinGW-w64 工具链，MSVC 配置会在 CMake 阶段直接报错。

如果应用自身也要直接使用 GLFW API，应额外链接 `glfw` target，并包含 `GLFW/glfw3.h`。TuneUI 对 GLFW 的依赖是 PRIVATE，不会把 GLFW 的头文件目录作为应用的编译期依赖传播。如果同时包含 glad，先定义 `GLFW_INCLUDE_NONE`，避免 GLFW 自动包含 OpenGL 头文件造成冲突。

### 1.2 坐标系与单位

| 概念 | 约定 |
| --- | --- |
| 原点 | 左上角 |
| x 轴 | 向右为正 |
| y 轴 | **向下为正** |
| 单位 | 逻辑像素（UI 像素） |
| 角度 | 无（当前不支持旋转） |

- 所有绘制 API（`Rect`、scissor、`Page`/`Floor` 布局）、`InputState` 鼠标坐标、`Window::windowWidth()/windowHeight()` 都使用**逻辑像素**。
- `Window::framebufferWidth()` / `framebufferHeight()` 返回**物理像素**（framebuffer 大小），只用于 `glViewport` 和 `beginFrame`。
- `Window::contentScale()` = 物理像素 / 逻辑像素（Windows 125% DPI 时为 1.25）。
- `Renderer::beginFrame(fbW, fbH, window.contentScale())`：传入 framebuffer 尺寸和内容缩放；逻辑画布大小 = framebuffer / scale，由 `Renderer::logicalBounds()` 返回。
- `Window::init(width, height)` 的宽高也是**逻辑像素**：内部使用 `GLFW_SCALE_TO_MONITOR`，所以请求 800×600 时，在 125% DPI 下 framebuffer 是 1000×750，逻辑尺寸仍是 800×600。
- 鼠标坐标由 `Window` 从 GLFW 屏幕坐标换算为逻辑坐标，因此可以直接与布局矩形做命中测试。
- 窗口跨显示器、DPI 变化时，`setContentScaleCallback()` 会收到新的 scale；每帧重新读取 `contentScale()` 即可。窗口尺寸回调给的是 **framebuffer 物理像素**。

> **最小化**：最小化窗口的 framebuffer 可能为 0，`beginFrame()` 返回 `false`。调用方应跳过该帧的绘制（仍要 `input().endFrame()` 和 `swapBuffers()`）。

### 1.3 颜色

```cpp
using Color = glm::vec4;  // (r, g, b, a)，分量范围 [0, 1]
```

- **直通 alpha（非预乘）**。渲染混合固定为 `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`。
- `alpha = 0` 完全透明；`alpha = 1` 不透明。
- `Color(1.0f)` 即 `(1,1,1,1)` 白色；`glm::vec4` 的 `.r/.g/.b/.a` 可用。
- 颜色分量线性参与混合，不做 sRGB 转换。

### 1.4 矩形

```cpp
struct Rect {
    float x = 0;      // 左上角 x
    float y = 0;      // 左上角 y
    float width = 0;
    float height = 0;
};
```

- `x` / `y` 是左上角，不是中心。
- 绘制 API 按 `x + width`、`y + height` 计算另一角；**宽高请传非负值**。负值不会崩溃，但会得到翻转的四边形，不建议依赖。
- 裁剪、页面边界和命中测试统一使用 `Math.h` 的辅助函数：`normalizeRect` 规范化负宽高，`intersectRects` 返回是否相交，`rectContains` / `rectEmpty` / `rectsEqual` / `isFiniteRect` 分别做包含、空矩形、相等和有限性判断。非有限值（NaN/Inf）一律按不合法处理。

### 1.5 帧生命周期

标准顺序：

```cpp
window.pollEvents();                                  // 1. 收事件，喂给 InputState

if (!renderer.beginFrame(window.framebufferWidth(),
                         window.framebufferHeight(),
                         window.contentScale())) {     // 2. 建帧；最小化时返回 false
    window.input().endFrame();
    window.swapBuffers();
    continue;
}
renderer.clear({0.1f, 0.1f, 0.12f, 1.0f});           // 3. 清屏
renderer.drawRect({20, 20, 200, 60}, {0.2f, 0.6f, 1.0f, 1.0f});
renderer.endFrame();                                 // 4. 提交剩余批次（不交换缓冲）

window.input().endFrame();                           // 5. 清掉本帧的一次性输入
window.swapBuffers();                                // 6. 显示
```

要点：

- `beginFrame` **每帧都必须调用**，它会重设 viewport、投影矩阵、GL 状态和批次；失败（未初始化、framebuffer 尺寸为 0）时返回 `false`。
- 帧外调用 `drawRect` / `drawTexturedRect` / `clear` / scissor 相关接口都是安全的 no-op；`endFrame` 在未开始帧时也安全。
- 重复调用 `beginFrame` 会先 flush 上一帧尚未提交的批，再重置状态，不会静默丢几何。
- `endFrame` 只 flush，不 swap；它会清空 scissor 栈并关闭裁剪。
- `input().endFrame()` 应在**消费完本帧输入之后**调用；它会清空按键边沿、重复标志、鼠标 delta、滚轮、文本、失焦标志。
- 使用 `Page` 时，第 3 步换成 `page.render(renderer)`（Page 在帧外直接返回），其余不变。

### 1.6 生命周期与线程

- `Window`、`Renderer`、`Shader`、`Texture` 都是**不可拷贝**的；`Shader` / `Texture` 支持移动。
- `Renderer`、`Shader`、`Texture` 依赖当前 GL 上下文：
  - 必须在 `Window::init()` 成功之后创建；
  - **必须比 `Window` 先析构**（上下文销毁后再调 GL 会崩溃/无效）。
- `Floor` 的纹理命令使用 `std::shared_ptr<const Texture>`：只要 floor 还持有引用，调用方释放自己的 `shared_ptr` 也不会销毁纹理。shared_ptr 只管理对象生命周期，不管理 GL 上下文——Page / Floor 仍必须在 Window 之前销毁。
- 所有类都不是线程安全的，请只在持有 GL 上下文的那个线程上调用。网络/文件线程应把结果投递到主线程后再改 UI。

### 1.7 错误处理约定

- 创建/初始化类接口返回 `bool`（`Window::init`、`Renderer::init`、`Texture::create*`、`Shader::compile`），失败后对象保持可析构、可重试的状态。
- 查询类接口在无效对象上返回 0 / 空字符串 / 默认 rect，不抛异常。
- `Floor::floorAt` 是唯一越界抛 `std::out_of_range` 的接口（与 `std::vector::at` 一致）；`findFloor` 返回 `nullptr`。
- 帧外绘制、空裁剪、非法矩形都是“无输出”而不是错误，方便布局系统在不可见状态下继续运行。

---

## 2. Window

`#include <tuneui/platform/Window.h>`

拥有一个 GLFW 窗口、OpenGL 3.3 Core 上下文、事件轮询和输入回调。

```cpp
tuneui::Window window;
if (!window.init(800, 600, "TuneUI")) {
    return 1;
}
```

标准光标：

```cpp
enum class CursorShape { Arrow, IBeam, Hand, HResize, VResize, Crosshair };
```

### 2.1 接口

| 接口 | 说明 |
| --- | --- |
| `bool init(int width, int height, const std::string& title)` | 初始化 GLFW（进程级只做一次）、创建窗口和 GL 3.3 Core 上下文、加载 GL 函数、注册输入/焦点/DPI 回调、默认开启垂直同步。宽高是**逻辑像素**。已初始化时再次调用直接返回 `true`，不会重建窗口。 |
| `bool isValid() const` | 是否成功创建了窗口。 |
| `bool shouldClose() const` | 用户是否关闭了窗口；无效窗口返回 `false`。 |
| `void pollEvents()` | 处理系统事件，把输入转发给 `input()`，并更新 `deltaTime()`；无效窗口时无操作。 |
| `void swapBuffers()` | 提交当前帧；无效窗口时无操作。 |
| `int windowWidth() const` / `windowHeight()` | 窗口内容区**逻辑像素**尺寸 = `framebuffer / contentScale`；无效窗口返回 0。 |
| `int framebufferWidth() const` / `framebufferHeight()` | framebuffer **物理像素**尺寸；无效窗口返回 0。 |
| `float contentScaleX() const` / `contentScaleY()` / `contentScale()` | 物理像素 / 逻辑像素；无效窗口返回 1。`contentScale()` 返回 X 方向。 |
| `void makeCurrent()` | 让本窗口的 GL 上下文成为当前上下文。 |
| `void setTitle(const std::string& title)` | 修改窗口标题。 |
| `void setVSync(bool enabled)` | 开关垂直同步（会先 `makeCurrent`）。 |
| `std::string clipboardString() const` / `void setClipboardString(const std::string&)` | 剪贴板读写；无窗口/读取失败时返回空串。 |
| `double time() const` | GLFW 启动以来的秒数。 |
| `double deltaTime() const` | 上一次 `pollEvents()` 距再上一次的秒数，保证 ≥ 0。 |
| `void setCursorShape(CursorShape)` | 切换标准光标：`Arrow` / `IBeam` / `Hand` / `HResize` / `VResize` / `Crosshair`。 |
| `InputState& input()` / `const InputState& input() const` | 本窗口的输入状态。 |
| `void setResizeCallback(ResizeCallback)` | framebuffer 尺寸变化回调；`using ResizeCallback = std::function<void(int width, int height)>`，参数是**物理像素**。 |
| `void setContentScaleCallback(ContentScaleCallback)` | DPI/内容缩放变化回调；`using ContentScaleCallback = std::function<void(float xscale, float yscale)>`。 |

### 2.2 行为说明

- GLFW 在整个进程内共享：第一个 `Window::init` 调用 `glfwInit`，最后一个创建的窗口析构时调用 `glfwTerminate`。
- `init` 是幂等的：窗口已存在时直接返回，不会覆盖句柄或泄漏窗口。
- `init` 成功时设置一次初始 viewport；之后由 `Renderer::beginFrame` 每帧重设。framebuffer 回调**不再**调用 `glViewport`，避免在别的上下文当前时改错状态。
- 窗口使用 `GLFW_SCALE_TO_MONITOR`：在 Windows 125% DPI 下请求 800×600，得到 1000×750 的 framebuffer 和 800×600 的逻辑尺寸。
- 键盘重复（`GLFW_REPEAT`）不会产生 `keyPressed` 边沿；用 `keyRepeated()` 查询。修饰键通过 `modifiers()` / `modifier()` 暴露。
- 失焦（Alt-Tab、点击别的窗口）会触发 `InputState::onFocusLost()`：所有按住状态补发 released、清空边沿/文本/滚轮/修饰键，并标记 `focusLost()`。
- `Window` 不暴露原生 `GLFWwindow*` 句柄。

---

## 3. InputState

`#include <tuneui/platform/InputState.h>`

保存**当前帧**的输入状态。正常使用时通过 `window.input()` 获取，不需要自己调用 `on*` 方法。

### 3.1 帧内状态 vs 持续状态

| 类别 | 成员 | 何时清除 |
| --- | --- | --- |
| 持续状态 | `mouseDown` / `keyDown` | 收到抬起事件或窗口失焦时 |
| 一次性状态 | `mousePressed` / `mouseReleased` / `keyPressed` / `keyReleased` / `keyRepeated` | `endFrame()` |
| 累计量 | `mouseDeltaX/Y`、`scrollX/Y`、`textInput()` | `endFrame()` |
| 窗口状态 | `focusLost()`、`cursorInside()` | `focusLost` 在 `endFrame()` 清除；`cursorInside` 跟随进入/离开事件 |

因此：同一帧内 `keyDown(W) && keyPressed(W)` 会同时为真；按下后不松开，后续帧 `keyDown` 一直为真，但 `keyPressed` 只在该次按下后的第一帧为真。系统按键重复只置 `keyRepeated()`，不会重新产生 `keyPressed`。

### 3.2 鼠标

```cpp
enum class MouseButton : int {
    Left = 0, Right = 1, Middle = 2,
    Button4 = 3, Button5 = 4, Button6 = 5, Button7 = 6, Button8 = 7,
    Count = 8,
};
```

| 接口 | 说明 |
| --- | --- |
| `double mouseX() const` / `mouseY()` | 当前光标位置，窗口内容区**逻辑坐标**，左上原点。 |
| `double mouseDeltaX() const` / `mouseDeltaY()` | 本帧累计位移（同一帧内多次移动会相加，单位逻辑像素）。 |
| `bool mouseDown(MouseButton) const` | 是否按住。 |
| `bool mousePressed(MouseButton) const` | 本帧是否刚按下（边沿）。 |
| `bool mouseReleased(MouseButton) const` | 本帧是否刚抬起（边沿）。 |
| `double scrollX() const` / `scrollY()` | 本帧滚轮累计量；普通滚轮通常只用 `scrollY`（向上为正，幅度由系统决定）。 |
| `bool cursorInside() const` | 光标当前是否在窗口内容区内。 |

- 左键 = `MouseButton::Left`，右键 = `MouseButton::Right`，中键 = `MouseButton::Middle`。
- 第一次鼠标移动的 delta 为 0；从窗口外进入后的第一次移动 delta 也为 0，避免从旧位置跳变。
- `Window` 负责把 GLFW 屏幕坐标换算成逻辑坐标；直接手工调用 `onMouseMove` 时传逻辑坐标。

### 3.3 键盘

| 接口 | 说明 |
| --- | --- |
| `bool keyDown(int key) const` | 是否按住。 |
| `bool keyPressed(int key) const` | 本帧是否刚按下（系统重复不会重新置位）。 |
| `bool keyReleased(int key) const` | 本帧是否刚抬起。 |
| `bool keyRepeated(int key) const` | 本帧是否收到系统按键重复。 |
| `unsigned modifiers() const` | GLFW 修饰键位掩码（Shift / Control / Alt / Super）。 |
| `bool modifier(Modifier bit) const` | 查询单个修饰键：`Modifier::Shift` / `Control` / `Alt` / `Super`。 |
| `bool focusLost() const` | 本帧是否发生了窗口失焦。 |
| `bool cursorInside() const` | 光标是否在窗口内容区内。 |

键码沿用 GLFW 的 `GLFW_KEY_*` 数值，以 `int` 存储（不依赖 GLFW 头）。常用值：

| 键 | 值 | 键 | 值 |
| --- | --- | --- | --- |
| `A`–`Z` | 65–90 | `0`–`9` | 48–57 |
| `Space` | 32 | `Esc` | 256 |
| `Enter` | 257 | `Tab` | 258 |
| `Backspace` | 259 | `Delete` | 261 |
| `Right` / `Left` / `Down` / `Up` | 262 / 263 / 264 / 265 | `PageUp` / `PageDown` | 266 / 267 |
| `Home` / `End` | 268 / 269 | `F1`–`F12` | 290–301 |
| `LeftShift` / `LeftControl` / `LeftAlt` | 340 / 341 / 342 | `RightShift` / `RightControl` / `RightAlt` | 344 / 345 / 346 |

越界键码（负数或 ≥ 512）会被安全忽略，查询返回 `false`。

- 系统按键重复走 `GLFW_REPEAT`，只影响 `keyRepeated()` 和按住状态，不产生新的 `keyPressed` 边沿。
- 窗口失焦时，所有按住的键和鼠标键都会补发 `released` 边沿并清除 `down`，不会出现“键卡住”。
- 修饰键状态来自最近一次键/鼠标事件；失焦时清零。

```cpp
// 使用 GLFW 常量（若同时包含 glad，需要先定义 GLFW_INCLUDE_NONE）
#include <GLFW/glfw3.h>

if (input.keyPressed(GLFW_KEY_ESCAPE)) {
    // ...
}
```

### 3.4 文本输入

| 接口 | 说明 |
| --- | --- |
| `const std::string& textInput() const` | 本帧由字符回调产生的 UTF-8 文本，可能包含多个字符（含中文、emoji）。 |

- **文本录入用 `textInput()`**：它经过系统键盘布局和输入法，`Shift`、Caps Lock、中文输入法都正确。
- **快捷键 / 游戏式控制用 `keyDown/keyPressed`**：它反映物理键，不经过输入法。
- 字母键通常**两者都会产生**：按 `W` 会同时出现 `keyPressed(87)` 和 `textInput() == "w"`（开了 Shift 则是 `"W"`）。
- 方向键、`Esc`、`Ctrl`、`F1` 等特殊键只产生键事件，不产生文本。
- 中文输入法组合输入只产生文本事件，不会给你一个「中文字符的键码」。

### 3.5 手工驱动（测试 / 自动化）

以下方法由 `Window` 的回调调用，也可以手工调用以模拟输入：

```cpp
void onMouseMove(double x, double y);
void onMouseButton(MouseButton button, bool down, unsigned mods = 0);
void onKey(int key, bool down, unsigned mods = 0, bool repeat = false);
void onScroll(double dx, double dy);
void onChar(unsigned int codepoint);      // 非法码点（孤代理、> 0x10FFFF）被丢弃
void onCursorEnter(bool entered);
void onFocusLost();
void setModifiers(unsigned mods);
void endFrame();
```

---

## 4. Renderer

`#include <tuneui/render/Renderer.h>`

批处理的 2D 四边形渲染器。同纹理的四边形会合并进一个动态顶点缓冲，一次 `glDrawArrays` 画完。

```cpp
tuneui::Renderer renderer;
if (!renderer.init()) {
    return 1;
}
// 每帧
if (!renderer.beginFrame(window.framebufferWidth(), window.framebufferHeight(),
                         window.contentScale())) {
    // 最小化：跳过绘制
}
renderer.clear({0.1f, 0.1f, 0.12f, 1.0f});
renderer.drawRect({20, 20, 200, 60}, {0.2f, 0.6f, 1.0f, 1.0f});
renderer.endFrame();
window.swapBuffers();
```

### 4.1 接口

| 接口 | 说明 |
| --- | --- |
| `bool init()` | 编译内置 UI 着色器、创建 1×1 白纹理和 VAO/VBO。需要当前 GL 上下文；重复调用直接返回 `true`。 |
| `void shutdown()` | 释放 GL 资源；可重复调用。析构函数会自动调用。 |
| `bool isValid() const` | 是否初始化成功。 |
| `bool beginFrame(int fbWidth, int fbHeight, float dpiScale = 1.0f)` | 开始一帧：设置 viewport、正交投影、混合/深度/剔除状态，关闭 scissor，清空批次和 draw call 计数。返回 `false` 表示未初始化或 framebuffer 尺寸为 0（最小化），此时后续绘制均为 no-op。第二次 `beginFrame` 会先 flush 上一帧。 |
| `bool frameActive() const` | 当前是否处于一帧内。 |
| `void endFrame()` | 提交尚未发出的四边形、清空 scissor 栈并关闭裁剪。不交换缓冲；未开始帧时安全。 |
| `void drawRect(const Rect& rect, const Color& color)` | 纯色四边形（内部使用 1×1 白纹理）。 |
| `void drawTexturedRect(const Rect& dst, const Rect& uv, const Color& tint, unsigned int textureId)` | 纹理四边形；`tint` 与采样值相乘；`textureId == 0` 时退回白纹理。 |
| `void clear(const Color& color)` | 先提交已入批的四边形，再 `glClear`；**遵守当前 scissor**。 |
| `void setScissor(const Rect& rect)` | 用 `rect` **替换**整个 scissor 栈（单层）；会先提交当前批次。 |
| `void pushScissor(const Rect& rect)` | 压入新裁剪，并与当前裁剪（栈空时为 `logicalBounds()`）求交；空交集也会压入，保证 push/pop 对称。 |
| `void popScissor()` | 弹出并恢复上一层；栈空时无操作。 |
| `void clearScissor()` | 清空整个栈并关闭裁剪。 |
| `int scissorDepth() const` | scissor 栈深度。 |
| `Rect currentScissor() const` | 当前生效裁剪；栈空时为 `logicalBounds()`。 |
| `bool scissorEnabled() const` | 当前是否开启裁剪。 |
| `void flush()` | 立即提交当前批次。 |
| `Rect logicalBounds() const` | 当前逻辑画布 `{0, 0, fbW/dpiScale, fbH/dpiScale}`。 |
| `int drawCallCount() const` | 自 `beginFrame` 起的 `glDrawArrays` 次数。 |
| `int pendingVertexCount() const` | 尚未提交的顶点数。 |
| `unsigned int whiteTextureId() const` | 内置白纹理 ID。 |
| `const Shader& shader() const` | 内置 UI 着色器（只读，供内省）。 |

帧外调用绘制、清屏或 scissor 接口不会崩溃，也不会产生输出；`beginFrame(0, 0)` 返回 `false`。`init()` 的部分失败会走 `shutdown()` 回滚，不会泄漏 VAO/VBO。

### 4.2 纹理坐标

- `Texture` 与 `Renderer` 统一约定：**UV 原点在纹理左上角**，`v = 0` 是图片顶部。
- 整张纹理用 `Rect{0, 0, 1, 1}`。
- 头像裁剪、图标表等只需调整 `uv`；例如只取左半边：`{0, 0, 0.5f, 1}`。

### 4.3 批处理与 draw call

- 相同纹理的连续四边形合并成一次 draw call。
- **纹理切换会 flush**：`drawRect` 使用白纹理，所以在纯色和贴图之间来回切换会产生大量 draw call。按纹理分组绘制可避免。
- 单批上限 `MaxVerticesPerBatch = 65532` 个顶点（10922 个四边形；按 6 的倍数取整），超过会自动 flush。
- 任何 scissor 变化（`setScissor` / `pushScissor` / `popScissor` / `clearScissor`）和 `clear` 都会 flush；`Page` 的裁剪变化同样会打断批次。
- 提交顺序即绘制顺序（深度测试关闭，后画的覆盖先画的）。
- 顶点格式：`{x, y, u, v, r, g, b, a}`，6 顶点 / 四边形，`GL_TRIANGLES`。

### 4.4 GL 状态

`beginFrame` 会设置：

- `glViewport(0, 0, fbWidth, fbHeight)`
- `GL_BLEND` 开启，`glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
- 深度测试关闭、面剔除关闭
- scissor 关闭
- 绑定内置着色器和 VAO

如果你在帧中间直接写裸 GL 并改动了这些状态，需要自己恢复；库暂不提供状态保存/恢复（见 TODO 1.3 / 6.2）。

### 4.5 裁剪

```cpp
renderer.setScissor({100, 100, 300, 200});   // 逻辑坐标，替换整个栈
renderer.drawRect({0, 0, 800, 600}, color);  // 只有裁剪区内的部分可见
renderer.clearScissor();

// 嵌套裁剪：父 clip ∩ 子 clip
renderer.pushScissor({100, 100, 300, 200});
renderer.pushScissor({150, 120, 100, 100});
renderer.drawRect({0, 0, 800, 600}, color);
renderer.popScissor();                       // 回到父 clip
renderer.popScissor();                       // 关闭裁剪
```

- 逻辑坐标自动转换成 framebuffer 坐标（含 y 翻转），并在转换为 int 前用 double 夹到 framebuffer 范围内；NaN/Inf 得到空裁剪。
- 宽高为 0、交集为空或完全在屏幕外时，得到空裁剪区：之后的内容不会出现。空交集仍会压栈，pop 次数保持对称。
- `pushScissor` 与当前裁剪求交；栈的初始裁剪是 `logicalBounds()`，因此裁剪永远不会超出窗口。
- `setScissor` 是“替换整个栈”的便捷接口（等价于清栈后压入一层）。
- 裁剪区可以比屏幕大，超出部分被忽略。
- `clear()` 也遵守 scissor，因此可以用「先设裁剪区再 clear」清出局部区域。

---

## 5. Shader

`#include <tuneui/render/Shader.h>`

GLSL 程序的编译、链接与 uniform 设置。需要当前 GL 上下文。

```cpp
tuneui::Shader shader;
if (!shader.compile(vertexSource, fragmentSource)) {
    std::fprintf(stderr, "%s\n", shader.log().c_str());
}
shader.use();
shader.setVec4("uColor", {0.2f, 0.6f, 1.0f, 1.0f});
```

| 接口 | 说明 |
| --- | --- |
| `bool compile(const char* vertexSource, const char* fragmentSource)` | 编译并链接；失败返回 `false`，日志写入 `log()`。会先销毁旧程序。 |
| `void destroy()` | 删除 program；可重复调用。 |
| `bool isValid() const` | 是否有可用的 program。 |
| `unsigned int id() const` | GL program 名称。 |
| `void use() const` | `glUseProgram`。 |
| `int uniformLocation(const char* name) const` | 查询位置；不存在或未链接返回 `-1`。 |
| `void setInt(const char* name, int value)` | 设置 `int` uniform。 |
| `void setFloat(const char* name, float value)` | 设置 `float` uniform。 |
| `void setVec2(const char* name, const glm::vec2& value)` | 设置 `vec2` uniform。 |
| `void setVec4(const char* name, const Color& value)` | 设置 `vec4` uniform。 |
| `void setMat4(const char* name, const glm::mat4& value)` | 设置 `mat4` uniform（列主序，不做转置）。 |
| `const std::string& log() const` | 最近一次编译/链接的日志；成功时为空。 |

注意：

- `set*` **不会自动 `use()`**，请先调用 `use()` 或确保目标 program 已绑定。
- uniform 名不存在时 `set*` 静默忽略（与 GL 的 `glGetUniformLocation == -1` 行为一致）。
- 移动语义、不可拷贝；析构时删除 program。上下文必须先于 Shader 存在。

---

## 6. Texture

`#include <tuneui/render/Texture.h>`

RGBA8 2D 纹理。文件/内存解码走 `stb_image`。需要当前 GL 上下文。

```cpp
tuneui::Texture icon;
if (!icon.createFromFile("assets/icon.png")) {
    // 失败原因已打印到 stderr
}

// 或上传裸 RGBA8
tuneui::Texture atlas;
atlas.create(pixels, 256, 256, /*sourcePremultiplied=*/false,
             tuneui::TextureFilter::Nearest);
```

| 接口 | 说明 |
| --- | --- |
| `bool create(const void* rgba, int width, int height, bool sourcePremultiplied = false, TextureFilter filter = Linear, TextureWrap wrap = ClampToEdge)` | 上传紧密排列的 RGBA8，行 0 是图片顶部。 |
| `bool createFromFile(const char* path, TextureFilter filter = Linear, TextureWrap wrap = ClampToEdge, bool flipVertically = false)` | 用 stb_image 解码 PNG/JPG/BMP/TGA 等并上传；失败时把原因打印到 stderr。 |
| `bool createFromMemory(const void* encodedBytes, std::size_t byteCount, ...)` | 同上，输入是内存中的编码数据（内嵌资源）。 |
| `bool createBlank(int width, int height, TextureFilter filter = Linear, TextureWrap wrap = ClampToEdge, const void* initialRgba = nullptr)` | 创建未初始化（或给定初值）的 RGBA8 纹理，供字体/图标图集使用。 |
| `bool updateRegion(int x, int y, int width, int height, const void* rgbaPixels)` | 用 `glTexSubImage2D` 更新子矩形；越界/空指针返回 `false` 且不破坏原纹理。 |
| `void destroy()` | 删除 GL 纹理；可重复调用。 |
| `bool isValid() const` | 是否持有纹理。 |
| `unsigned int id() const` | GL 纹理 ID（可直接传给 `drawTexturedRect`）。 |
| `int width() const` / `height() const` | 尺寸；无效时为 0。 |

枚举：

```cpp
enum class TextureFilter { Nearest, Linear };
enum class TextureWrap { ClampToEdge, Repeat, MirroredRepeat };
```

### 6.1 行序与翻转

- 解码结果和 `create` 的输入都按**第一行是图片顶部**处理，与本库 `v = 0 在顶部` 的 UV 约定一致。
- 只有源数据是**自下而上**存放时才需要 `flipVertically = true`。翻转在本地缓冲完成，不修改 stb 的全局状态。

### 6.2 预乘 alpha

`sourcePremultiplied = true` 表示输入像素的 RGB 已经乘过 alpha。渲染器使用直通 alpha 混合，因此上传时会自动转回直通 alpha。

- 普通 PNG/JPEG 解出来的都是直通 alpha，保持默认 `false`。
- 只有图像处理管线明确输出预乘数据时才传 `true`。

### 6.3 图集约定（字体/图标）

- 图集是固定尺寸的 RGBA8 纹理，用 `createBlank()` 一次性分配，再用 `updateRegion()` 逐字形/逐图标写入；不支持动态扩容或重打包。
- 字形像素约定：**RGB = 白色 (255,255,255)，A = 覆盖度**。渲染时用 `drawTexturedRect` 的 `tint` 染色，标准直通 alpha 混合即可。
- 每个字形之间留 1px padding，UV 内缩半个 texel，避免 `Linear` 采样在边缘渗色。
- `createBlank` / `updateRegion` 会保存并恢复 `GL_TEXTURE_BINDING_2D` 和 `GL_UNPACK_ALIGNMENT`，不会污染调用方状态。
- 系统字体不随仓库分发；自动化测试使用 Windows 字体（如 `C:/Windows/Fonts/arial.ttf`），缺失时跳过。中文可手动用 `msyh.ttc` 验证，结果依赖操作系统，不可复现。

### 6.4 注意

- `create*` 会**替换**已有纹理：调用失败时旧纹理也会被销毁，`isValid()` 变 `false`。`updateRegion` 失败则保留原纹理。
- 没有 mipmap，`Nearest` / `Linear` 同时用于放大和缩小。
- 移动语义、不可拷贝；析构时删除 GL 纹理。上下文必须先于 Texture 存在。

---

## 7. Page / Floor

`#include <tuneui/render/Page.h>`

- **Page**：一屏场景，负责背景、图层排序和绘制。
- **Floor**：图层，类似绘图软件的图层。每个 floor 持有一组保留模式的四边形命令。

配置一次，每帧只调用 `page.render(renderer)`：

```cpp
#include <tuneui/render/Page.h>

tuneui::Page page;
page.setBackground({0.10f, 0.11f, 0.13f, 1.0f});

auto icon = std::make_shared<tuneui::Texture>();
icon->createFromFile("assets/icon.png");

// Floor 是一次性配置对象：先构造、配置，再整体交给 page
// 内容层：固定裁剪窗口 + 随滚动移动的内容
tuneui::Floor content("content", 0);
content.setClip({0, 64, 800, 480})
       .setOffset({0, -scrollY})
       .addRect({20, 80, 200, 60}, {0.2f, 0.6f, 1.0f, 1.0f})
       .addTexturedRect({240, 80, 48, 48}, {0, 0, 1, 1},
                        {1, 1, 1, 1}, icon);
page.addFloor(content);           // 复制进页面；std::move(content) 可避免复制

// HUD 层：永远在内容之上
tuneui::Floor hud("hud", 10);
hud.addRect({0, 0, 800, 64}, {0, 0, 0, 0.6f});
page.addFloor(hud);
```

```cpp
// 每帧
renderer.beginFrame(window.framebufferWidth(), window.framebufferHeight(),
                    window.contentScale());
page.render(renderer);          // 不负责 endFrame / swapBuffers
renderer.endFrame();
window.swapBuffers();
```

### 7.1 Floor 接口

| 接口 | 说明 |
| --- | --- |
| `Floor(std::string name, int level = 0)` | 独立构造配置对象；名字在构造时传入（目前用于 `findFloor`，以后用于内部显示）。 |
| `int level() const` | 层级，越大越靠上。 |
| `const std::string& name() const` | 名称（用于 `findFloor`）。 |
| `bool visible() const` | 是否参与绘制。 |
| `bool hasClip() const` / `Rect clip() const` | 裁剪状态与矩形（页面坐标）。 |
| `Vec2 offset() const` | 内容偏移。 |
| `Floor& setLevel(int level)` | 修改层级。 |
| `Floor& setVisible(bool visible)` | 显示/隐藏。 |
| `Floor& setClip(const Rect& rect)` | 设置裁剪窗口，页面逻辑坐标。 |
| `Floor& clearClip()` | 取消裁剪。 |
| `Floor& setOffset(Vec2 offset)` | 平移内容；**裁剪窗口不动**。 |
| `Floor& addRect(const Rect& dst, const Color& color)` | 追加纯色四边形。 |
| `Floor& addTexturedRect(const Rect& dst, const Rect& uv, const Color& tint, std::shared_ptr<const Texture> texture = nullptr)` | 追加纹理四边形；空指针为白纹理。Floor 持有 shared_ptr，纹理在 floor 存活期间不会被销毁。 |
| `void clearCommands()` | 清空本层所有四边形。 |
| `std::size_t commandCount() const` | 四边形数量。 |
| `const std::vector<QuadCommand>& commands() const` | 只读命令列表。 |

所有 setter 与 `add*` 都返回 `Floor&`，可以链式调用。

### 7.2 Page 接口

| 接口 | 说明 |
| --- | --- |
| `void setBackground(const Color& color)` | 设置背景色；用 `renderer.clear()` 绘制。 |
| `void clearBackground()` | 取消背景（不清屏）。 |
| `bool hasBackground() const` / `Color background() const` | 查询背景。 |
| `void addFloor(Floor floor)` | 唯一添加入口：移交一个已配置好的 floor。按值接收：左值复制、临时对象移动；`std::move(floor)` 可避免复制。不返回值，之后用 `findFloor` / `floorAt` 获取。 |
| `std::size_t floorCount() const` | 图层数量。 |
| `Floor& floorAt(std::size_t index)` | 按添加顺序取图层；越界抛 `std::out_of_range`。 |
| `Floor* findFloor(const std::string& name)` / `const 版本` | 按名称查找；找不到返回 `nullptr`。 |
| `void render(Renderer& renderer)` | 绘制整页。 |

> 没有 `addFloor(level, name)` 这类入口：**不能先添加空 floor 再配置**。必须先构造 `Floor`、完成全部配置，再整体交给 Page。

### 7.3 渲染顺序与语义

1. 若 `renderer.frameActive()` 为 false，直接返回（不 clear、不绘制）。有背景则 `renderer.clear(background)`：会先 flush 调用方已排队的几何，并遵守调用方已有的外层 scissor。
2. 过滤掉 `visible(false)` 的 floor，按 `level` **升序**绘制；`level` 相同则保持**添加顺序**（后添加的在上）。
3. 每个 floor 的生效裁剪 = `clip` ∩ 页面逻辑边界：
   - 交集为空 → 整层跳过，不产生 GL 调用；
   - 裁剪覆盖整页 → 当作不裁剪，保证相邻层能合批；
   - 命令为空的 floor 直接跳过，不触发 scissor 切换 / flush。
4. 生效裁剪变化时用 `pushScissor` / `popScissor` 切换；push 会再与调用方外层 scissor 求交。相同裁剪的连续 floor 合在同一批里。
5. 每个命令的 `dst` 加上 floor 的 `offset` 后提交；纹理命令在重放时解析 `texture->id()`，因此重新 `create()` 同一对象后下一帧自动生效。
6. 结束时只弹出本页压入的裁剪，调用方的 scissor 栈深度保持不变。

### 7.4 使用规则

- 调用 `page.render(renderer)` 前必须先 `renderer.beginFrame(...)`；Page 不会自己建帧，帧外调用是 no-op。
- 背景走 `glClear`：覆盖**整个 framebuffer**（有外层 scissor 时只清裁剪区），且**不参与混合**（alpha < 1 时直接写 alpha 通道，而不是与已有内容合成）。
- `Page::render` **保留**调用方 scissor：floor 裁剪与外层 scissor 求交，退出时恢复原栈深度。如果调用方在 `render` 前画了东西，背景仍会把它们清掉（这是有序清屏，不是状态污染）。
- 页面内的 floor 地址在后续添加图层后保持稳定（floor 由 `unique_ptr` 持有）；用 `floorAt` / `findFloor` 获取指针。但不要保存到 Page 被销毁之后。
- `Floor` 是配置对象：`addFloor(floor)` 按值接收，左值会被**复制**、临时对象会被移动；移交后原对象与页面内对象互不影响。`addFloor` 不返回引用；需要后续修改时用 `findFloor(name)`。
- 修改 floor 的配置或命令后，下一帧自动生效；不需要重建 Page。
- 纹理命令保存 `std::shared_ptr<const Texture>`。floor 持有引用即可保证对象存活，重放时解析当前 `id()`；调用方仍必须保证 GL 上下文先于 Page/Floor 销毁。
- 目前单个 Floor 只有一个裁剪矩形；**渲染器已支持 scissor 栈**，但“floor 内嵌套子层”的组件树要到第 2/3 阶段。ScrollView 可以用「父 clip push + 自身 clip push」实现。
- 目前只支持**轴对齐四边形**：没有旋转、缩放、透明度组、混合模式、裸 GL 回调。
- 还没有稳定 ID 和 JSON 序列化；`level` 与添加顺序是跨运行不稳定的排序依据。序列化只能写资源名/路径，不能写 `shared_ptr` 或 GL ID。

### 7.5 命令结构

```cpp
struct QuadCommand {
    enum class Kind { Solid, Textured };

    Kind kind = Kind::Solid;
    Rect dst{};
    Rect uv{0.0f, 0.0f, 1.0f, 1.0f}; // 仅 Textured
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    std::shared_ptr<const Texture> texture; // 仅 Textured；null = 白纹理
};
```

每个命令对应未来 JSON 里的一条对象；`Floor::commands()` 可以只读遍历。

---

## 8. Math

`#include <tuneui/render/Math.h>`

```cpp
namespace tuneui {
using Vec2 = glm::vec2;
using Color = glm::vec4;          // 直通 alpha

struct Rect { float x, y, width, height; };

glm::mat4 orthoPixels(float framebufferWidth, float framebufferHeight,
                      float dpiScale = 1.0f);

Rect normalizeRect(const Rect&);                    // 负宽高翻正
bool isFiniteRect(const Rect&);                     // 拒绝 NaN/Inf
bool rectEmpty(const Rect&);                        // width/height <= 0
bool intersectRects(const Rect& a, const Rect& b, Rect& out); // 相交才返回 true
bool rectContains(const Rect& outer, const Rect& inner);
bool rectsEqual(const Rect& a, const Rect& b);
}
```

- `orthoPixels` 把逻辑像素坐标（左上原点、+y 向下）映射到 NDC。
- 参数是**物理像素**的 framebuffer 尺寸和 `dpiScale`；返回列主序 `glm::mat4`。
- 尺寸或 `dpiScale` 非法（≤ 0）时返回单位矩阵，避免 NaN。
- 矩形辅助函数都先检查有限性；非有限输入按“空/不相交/不包含”处理，可直接用于布局与命中测试。
- `Renderer::beginFrame` 内部已经使用 `orthoPixels`；只有需要自己写 GL 且想对齐坐标系时才需要直接调用。

---

## 9. 常见问题与坑

### 图片上下颠倒 / UV 不对

本库约定 `v = 0` 对应纹理顶部。stb_image 解出的 PNG/JPEG 第一行就是顶部，所以默认不需要翻转。只有自下而上的原始数据才传 `flipVertically = true`。

### 右键没反应

`MouseButton::Right` 是 1（对应 GLFW 的顺序），不是 0。另外 `mousePressed` 只在按下的那一帧为真，记得在 `input().endFrame()` 之前查询。

### 按 W 和按方向键表现不一样

- `W` 是字符键：既有键事件，也有 `textInput()`。
- 方向键 / `Esc` / `Ctrl`：只有键事件。
- 中文输入法：通常只有 `textInput()`，没有对应键码。

文本录入用 `textInput()`，快捷键和游戏控制用 `keyPressed()`。

### scissor 改了但没效果

- 裁剪只影响**之后提交**的四边形；请先 `setScissor` / `pushScissor` 再绘制。
- `setScissor` 是替换整个栈；要嵌套用 `pushScissor` + 配对的 `popScissor`。
- 空交集会得到空裁剪，之后的内容全部不可见；检查矩形的宽高和当前 scissor 深度。
- `endFrame` 会清空 scissor 栈并关闭裁剪；下一帧 `beginFrame` 也会重置。

### draw call 比预期多

按成本从高到低排查：

1. 纹理来回切换（纯色四边形也使用白纹理）。
2. scissor 频繁变化（每次变化都 flush）。
3. 单批超过 65532 顶点。
4. 调用了 `flush()` / `clear()`。

用 `drawCallCount()` 和 `pendingVertexCount()` 观察。

### 程序退出时崩溃 / GL 调用无效

`Renderer`、`Shader`、`Texture` 必须在 `Window` 之前销毁。把它们的定义放在 `window` 之后（同一作用域中后定义先析构），或显式调用 `shutdown()` / `destroy()`。

### 窗口缩放后画面错位

每帧都要用**当前**的 `window.framebufferWidth()` / `framebufferHeight()` 调用 `beginFrame`；它会同步 viewport 和投影矩阵。

### Page 背景设了半透明但没混合

`setBackground` 走 `glClear`，不参与混合。需要半透明底板时，加一个 floor 并 `addRect` 一个覆盖全屏的半透明矩形。

### `Texture::create` 失败后原来的纹理没了

这是当前语义：`create*` 会先销毁旧纹理再上传。需要保留旧纹理时，先创建到临时 `Texture`，成功后再移动赋值。

### 鼠标位置和绘制矩形对不上（125% / 150% 缩放）

- 布局用**逻辑像素**；`windowWidth()/windowHeight()` 已经是逻辑尺寸。
- `beginFrame` 必须传 `window.contentScale()`，否则逻辑画布会被当成 framebuffer 尺寸。
- 鼠标坐标由 `Window` 换算成逻辑像素；手工注入 `onMouseMove` 时不要传物理像素。
- 跨显示器拖动后 scale 会变；每帧重新查询 `contentScale()`，或监听 `setContentScaleCallback`。

### 窗口最小化后画面异常 / 除零

最小化时 framebuffer 为 0，`beginFrame` 返回 `false`。跳过该帧的 `clear/draw/render`，仍然调用 `input().endFrame()` 和 `swapBuffers()`。

### 纹理被提前销毁 / 重放画错

- 旧版 Floor 存裸 GL ID；现在 `addTexturedRect` 接收 `std::shared_ptr<const Texture>`，floor 会持有引用。
- 同一 `Texture` 对象重新 `create()` 后，下一帧重放会解析新的 `id()`，不会复用旧 ID。
- shared_ptr 不保证 GL 上下文存活：Page/Floor 仍必须在 Window 之前销毁。

### 修改了外部 GL 状态后画面异常

`beginFrame` 每帧重设它需要的状态；但同一帧内如果你自己调用了裸 GL 并改动混合/深度/裁剪，后续绘制可能不符合预期。库暂不提供状态保存/恢复。

---

## 相关文档

- [README](../README.md)：项目定位、构建与运行、依赖、已知约束
- [STYLE.md](STYLE.md)：命名与代码风格（贡献者）
- [PROGRESS.md](../PROGRESS.md)：历史实施记录与当时的验证结果（快照，不作现状依据）
- [TODO.md](../TODO.md)：任务清单、优先级与里程碑
