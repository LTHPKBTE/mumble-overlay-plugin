# Speaking Users Overlay — Mumble Plugin

悬浮窗插件，实时显示 Mumble 服务器上当前正在说话的用户列表。

## 功能

- 📋 实时显示正在说话/密语/喊话的用户
- 🎨 可调节窗口透明度 (Alpha)
- 📌 窗口置顶 (Always on Top)
- 🖱️ 鼠标穿透模式 (可选，点击穿透到后方窗口)
- 🌐 自动检测系统语言（中文 / English）
- ⚡ 使用 GLFW + Dear ImGui (cimgui) 渲染

## 构建

### 1. 获取依赖

```bash
# 克隆 cimgui (内含 imgui 子模块)
git clone https://github.com/cimgui/cimgui.git 3rdparty/cimgui
cd 3rdparty/cimgui
git submodule update --init --recursive
cd ../..

# 克隆 GLFW
git clone https://github.com/glfw/glfw.git 3rdparty/glfw
```

目录结构应为：
```
mumble-overlay-plugin/
├── 3rdparty/
│   ├── cimgui/        # cimgui + imgui 源码
│   └── glfw/          # GLFW 源码
├── include/
│   └── MumblePlugin.h # Mumble 插件 API 头文件
├── src/
│   ├── plugin.c
│   ├── speaking_users.c / .h
│   ├── overlay_window.c / .h
│   └── render_thread.c / .h
├── test/
│   └── test_standalone.c
└── CMakeLists.txt
```

### 2. 编译

```bash
cmake -B build
cmake --build build
```

产物：`build/plugin.dll` (Windows) 或 `build/libplugin.so` (Linux)。

### 3. 独立测试 (无需 Mumble)

```bash
cmake -B build -DOVERLAY_BUILD_STANDALONE=ON
cmake --build build
./build/overlay_test
```

### 4. 安装到 Mumble

将 `plugin.dll` 复制到 Mumble 的 `plugins/` 目录，然后在 Mumble 插件管理器中启用。

或打包为 `.mumble_plugin`：
```bash
# 将 plugin.dll + manifest.xml 打包
zip SpeakingUsersOverlay.mumble_plugin plugin.dll manifest.xml
```

## 架构

```
[Mumble 主线程]                    [渲染线程]
  回调 ──→ speaking_users (mutex)  ←── poll 回调 (只读)
  可调用 MumbleAPI                  禁止调用 MumbleAPI
```

