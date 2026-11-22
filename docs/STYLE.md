# TuneUI 代码风格

本文件是命名与代码风格的**唯一来源**。构建与运行见 [README](../README.md)，接口语义见 [API 文档](API.md)。

## 1. 命名

| 种类 | 规则 | 正例 | 反例 |
| --- | --- | --- | --- |
| 类 / 结构体 / 枚举 | PascalCase | `Window`、`QuadCommand`、`TextureFilter` | `window_t`、`quadCommand` |
| 函数 / 自由变量 | camelCase | `parseCondition`、`nextLabel`、`orthoPixels` | `ParseCondition`、`ortho_pixels` |
| 成员变量 | camelCase + 尾随 `_` | `window_`、`mouseX_`、`level_` | `m_window`、`_window`、`MouseX` |
| 常量（`constexpr` / `static constexpr` / 文件内常量） | PascalCase | `MouseButtonCount`、`MaxVerticesPerBatch`、`VertexShaderSource` | `kMaxVertices`、`MAX_VERTICES`、`maxVertices` |
| 枚举值 | PascalCase | `MouseButton::Left`、`TextureFilter::Nearest`、`QuadCommand::Kind::Solid` | `MOUSE_LEFT`、`nearest` |
| 宏 | SCREAMING_SNAKE_CASE | `CHECK` | `Check`、`checkMacro` |
| 类型别名 | PascalCase | `Vec2`、`Color`、`ModifierMask`、`ResizeCallback` | `vec2_t` |
| 命名空间 | 全小写 | `tuneui` | `TuneUI` |

### 1.1 成员变量为什么带下划线

成员用「camelCase 名字 + 尾随 `_`」，是为了和同名访问器共存：

```cpp
class Floor {
public:
    int level() const { return level_; }   // 访问器是 level()
private:
    int level_;                             // 成员是 level_
};
```

`InputState`（`mouseX()` / `mouseX_`）、`Texture`（`width()` / `width_`）、`Page`（`hasBackground()` / `hasBackground_`）都靠这条约定区分。**不要**为了去掉下划线而重命名访问器或成员。

### 1.2 不要加类型前缀

- 常量不加 `k` 前缀：写 `MaxQuadsPerBatch`，不写 `kMaxQuadsPerBatch`。
- 文件级 / 匿名命名空间变量不加 `s_` / `g_` 前缀：写 `windowCount`、`checkCount`，不写 `s_windowCount`、`g_checks`。匿名命名空间已经限定了可见性，前缀只是噪音。

历史代码里的 `k...` / `s_...` / `g_...` 已全部清理；新增代码不要重新引入。

### 1.3 布尔接口

返回布尔的查询用 `is` / `has` / `should` 等前缀或形容词形式，读起来像断言：

```cpp
bool isValid() const;
bool hasClip() const;
bool shouldClose() const;
bool frameActive() const;
bool rectEmpty(const Rect& rect);
```

事件型状态沿用既有命名（`mouseDown` / `keyPressed`），不要改成 `getMouseDown` 这类 getter 风格。

### 1.4 缩写的写法

沿用既有写法，不要自创新缩写：`id`、`uv`、`dpi`、`gl`、`rect`、`dst`、`src`、`fb`（仅局部变量）。

## 2. 排版

- 4 空格缩进，不使用 tab；行宽尽量控制在 100 列内。
- 花括号与语句同行（K&R）：`if (!renderer.init()) { return 1; }`。
- 指针 / 引用符号贴类型：`const Rect& rect`、`char* p`。
- 头文件用 `#pragma once`，不用 include guard。
- include 分三组、组间空一行：本文件的头、第三方头、标准库。涉及 GL 的实现文件（`Renderer.cpp` / `Shader.cpp` / `Texture.cpp`）按现有顺序把 glad 放在最前，避免 GL 头冲突，不要为了统一顺序去调换。
- 优先 `const` / `constexpr`；类型转换用 `static_cast`，不用 C 风格转换。
- 不使用 `using namespace`。

## 3. 注释与文档

- **代码注释用英文**，说明「为什么」和「约束」，不复述代码。
- 公开头文件的每个类 / 非平凡接口都要有一句英文注释，写清前置条件（比如「需要当前 GL 上下文」）。
- 项目文档（`README.md`、`docs/API.md`、`TODO.md`、`PROGRESS.md`）用中文。
- 注释里的旧接口名、旧路径一律改掉，不要留「见上文 oldName」这种半吊子引用。

## 4. 检查清单

改完代码前自查：

- [ ] 新增类型是 PascalCase，新增函数 / 变量是 camelCase，新增常量是 PascalCase
- [ ] 成员变量带尾随 `_`，且与访问器不冲突
- [ ] 没有重新引入 `k` / `s_` / `g_` 前缀
- [ ] 公共头改动同步了 `docs/API.md`；行为改动追加了 `PROGRESS.md`
