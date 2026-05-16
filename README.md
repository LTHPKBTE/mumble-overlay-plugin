# Speaking Users Overlay -- Mumble Plugin

Overlay plugin that displays a real-time list of users currently speaking on a Mumble server.

## Features

- Real-time display of talking / whispering / shouting users
- Adjustable window transparency (Alpha)
- Always on Top
- Mouse passthrough mode (clicks pass through to windows behind)
- Automatic system language detection (English / Chinese)
- Rendered with GLFW + Dear ImGui (cimgui)

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
