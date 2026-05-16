# Speaking Users Overlay -- Mumble Plugin

Overlay plugin that displays a real-time list of users currently speaking on a Mumble server.

## Features

- Real-time display of talking / whispering / shouting users
- Adjustable window transparency (Alpha)
- Always on Top
- Mouse passthrough mode (clicks pass through to windows behind)
- Recent speakers first — sorting by speaking recency
- Configurable visible count — limit shown speakers, scroll for the rest (default 8)
- Auto-snap to top — list returns to most recent speakers after 10s idle or passthrough
- Screen edge clamping — window cannot be dragged off-screen
- Automatic system language detection (English / Chinese)
- No taskbar entry — won't clutter your taskbar
- Settings persist across restarts — window position, transparency, visible count, etc. are saved
- Rendered with GLFW + Dear ImGui (cimgui)

## Usage

### Basic Controls

Once connected to a server, a small overlay window appears showing speaking users.

| Control | Action |
|---|---|
| **Drag the title area ("Speaking Users" text)** | Move the overlay window |
| **Settings button** | Open settings panel |
| **X button** (top-right) | Hide the overlay (plugin keeps running) |

### Settings Panel

Click the Settings button to open:

| Setting | Description |
|---|---|
| **Transparency** | Slider to adjust window opacity (0.1 ~ 1.0) |
| **Always on top** | Keep window above other windows |
| **Mouse passthrough** | Let mouse clicks pass through the window |
| **Visible speakers** | Number of recent speakers shown at top (1~64) |

### Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl + Shift + P` | Disable mouse passthrough (escape hatch) |
| `Ctrl + Shift + H` | Show a hidden window (if you closed it with X) |

### Speaker List Behavior

- Recent speakers first: The list is sorted so the most recently active user is always at the top.
- Visible limit: Only the most recent N speakers are shown in the top section (default 8, configurable in Settings). Additional speakers appear below a `--- more ---` separator when you scroll down.
- Scroll is preserved: Scrolling through the list won't reset when new users start or stop speaking.
- Auto-snap to top: The list automatically scrolls back to the top when:
  - Mouse passthrough is enabled, or
  - The mouse has been away from the window for 10+ seconds.
  Once you manually scroll, auto-snap is suspended until the next idle period.

### If the Window Is Lost or Off-Screen

The window cannot be dragged off-screen — edge clamping keeps at least 20% of it visible on your monitor.

If it's hidden:
- Reconnect to the Mumble server — the overlay window will reappear automatically.
- Or use Settings > Show Window (opens when the window is hidden).
- Or press Ctrl+Shift+H to show a hidden window.

All configurable settings (position, size, transparency, passthrough, always-on-top, visible speakers count) are automatically saved to disk when Mumble exits and restored next time the plugin loads.

Saved config location:
- Windows: `%APPDATA%\Mumble\SpeakingOverlay.cfg`
- Linux:   `~/.config/mumble-overlay-plugin.cfg`
- macOS:   `~/.config/mumble-overlay-plugin.cfg`

## Build

### 1. Get Dependencies

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/YOUR_USER/mumble-overlay-plugin.git
# Or if already cloned:
git submodule update --init --recursive
```

### 2. Compile

```bash
cmake -B build
cmake --build build
```

Output: `build/plugin.dll` (Windows), `build/libplugin.so` (Linux), or `build/libplugin.dylib` (macOS).

### 3. Standalone Test (No Mumble Required)

```bash
cmake -B build -DOVERLAY_BUILD_STANDALONE=ON
cmake --build build
./build/overlay_test
```

### 4. Install to Mumble

Copy the plugin binary to Mumble's `plugins/` directory, then enable it in Mumble's plugin manager.

Or bundle as `.mumble_plugin`:
```bash
zip SpeakingUsersOverlay.mumble_plugin plugin.dll manifest.xml
```

## Architecture

```
[Mumble main thread]                [Render thread]
  callbacks --> speaking_users (mutex) <-- poll callback (read only)
  MumbleAPI allowed                  MumbleAPI forbidden
```

## License

MIT. See [LICENSE](LICENSE).

The `include/MumblePlugin.h` header is from the Mumble project (BSD-3-Clause).
Dependencies (submodules) use their own permissive licenses (MIT / zlib).
