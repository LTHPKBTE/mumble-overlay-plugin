# Speaking Users Overlay -- Mumble 插件

悬浮窗插件，实时显示 Mumble 服务器上当前正在说话的用户列表。

## 功能

- 实时显示说话 / 密语 / 喊话中的用户
- 可调节窗口透明度 (Alpha)
- 窗口置顶 (Always on Top)
- 鼠标穿透模式（点击穿透到后方窗口）
- 自动检测系统语言（中文 / English）
- 使用 GLFW + Dear ImGui (cimgui) 渲染

## 构建

### 1. 获取依赖

```bash
# 递归克隆（含子模块）
git clone --recurse-submodules https://github.com/YOUR_USER/mumble-overlay-plugin.git
# 或已克隆后：
git submodule update --init --recursive
```

### 2. 编译

```bash
cmake -B build
cmake --build build
```

产物：`build/plugin.dll` (Windows)、`build/libplugin.so` (Linux)、`build/libplugin.dylib` (macOS)。

### 3. 独立测试（无需 Mumble）

```bash
cmake -B build -DOVERLAY_BUILD_STANDALONE=ON
cmake --build build
./build/overlay_test
```

### 4. 安装到 Mumble

将插件二进制文件复制到 Mumble 的 `plugins/` 目录，然后在 Mumble 插件管理器中启用。

或打包为 `.mumble_plugin`：
```bash
zip SpeakingUsersOverlay.mumble_plugin plugin.dll manifest.xml
```

## 架构

```
[Mumble 主线程]                    [渲染线程]
  回调 --> speaking_users (mutex) <-- poll 回调 (只读)
  可调用 MumbleAPI                 禁止调用 MumbleAPI
```

## 协议

MIT。参见 [LICENSE](LICENSE)。

`include/MumblePlugin.h` 头文件来自 Mumble 项目 (BSD-3-Clause)。
依赖项（子模块）使用各自的宽松协议 (MIT / zlib)。
