/*
 * overlay_window.c — GLFW + Dear ImGui overlay window implementation
 *
 * Creates a floating overlay with:
 *   - Transparency control (alpha slider)
 *   - Optional mouse passthrough
 *   - Always-on-top
 *   - Live list of currently speaking users
 *   - System language detection for UI strings
 *
 * Uses Dear ImGui C++ API directly.
 */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define GL_SILENCE_DEPRECATION
#endif

#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
/* Windows SDK provides GL/gl.h via glfw3.h auto-include */
#else
/* Minimal GL 1.1 declarations (link with -lGL / OpenGL.framework) */
#define GL_COLOR_BUFFER_BIT 0x00004000
extern "C" {
void glClear(unsigned int mask);
void glClearColor(float r, float g, float b, float a);
void glViewport(int x, int y, int w, int h);
}
#endif

/* ---- Overlay internal header ---- */
#include "overlay_window.h"

/* ========================================================================
 * Localisation: detect system language
 * ======================================================================== */
static int g_lang_is_chinese = 0;

static void detect_system_language(void) {
#ifdef _WIN32
    LANGID langID = GetUserDefaultUILanguage();
    WORD primary  = PRIMARYLANGID(langID);
    g_lang_is_chinese = (primary == LANG_CHINESE
                         || primary == LANG_CHINESE_SIMPLIFIED
                         || primary == LANG_CHINESE_TRADITIONAL);
#else
    const char *lang = getenv("LANG");
    if (lang != NULL) {
        g_lang_is_chinese = (strncmp(lang, "zh", 2) == 0);
    }
#endif
}

#define LOC(chinese, english)  (g_lang_is_chinese ? (chinese) : (english))

/* ========================================================================
 * Internal state
 * ======================================================================== */
static GLFWwindow      *g_window         = NULL;
static overlay_config_t g_config;
static bool             g_settings_open  = false;
static bool             g_first_frame    = true;

/* ========================================================================
 * Configuration defaults
 * ======================================================================== */
overlay_config_t overlay_config_default(void) {
    overlay_config_t cfg;
    cfg.window_x          = 100;
    cfg.window_y          = 100;
    cfg.window_width      = 320;
    cfg.window_height     = 480;
    cfg.alpha             = 0.85f;
    cfg.mouse_passthrough = false;
    cfg.always_on_top     = true;
    return cfg;
}

/* ========================================================================
 * GLFW error callback
 * ======================================================================== */
static void glfw_error_callback(int error, const char *description) {
    (void)error;
    fprintf(stderr, "[OverlayWindow] GLFW error: %s\n", description);
}

/* ========================================================================
 * Initialize
 * ======================================================================== */
int overlay_window_init(const overlay_config_t *cfg) {
    g_config = *cfg;
    detect_system_language();

    /* --- GLFW --- */
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return OW_ERR_GLFW;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, g_config.always_on_top ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

#ifdef _WIN32
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH,
                   g_config.mouse_passthrough ? GLFW_TRUE : GLFW_FALSE);
#endif

    g_window = glfwCreateWindow(g_config.window_width, g_config.window_height,
                                "Mumble Speaking Overlay", NULL, NULL);
    if (g_window == NULL) {
        glfwTerminate();
        return OW_ERR_GLFW;
    }

    glfwSetWindowPos(g_window, g_config.window_x, g_config.window_y);
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    /* --- Dear ImGui --- */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.Alpha = g_config.alpha;

    /* --- Backend init --- */
    if (!ImGui_ImplGlfw_InitForOpenGL(g_window, true)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return OW_ERR_IMGUI;
    }

#ifdef __APPLE__
    const char *glsl_version = "#version 150";
#else
    const char *glsl_version = "#version 130";
#endif

    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return OW_ERR_IMGUI;
    }

    g_first_frame = true;
    return OW_OK;
}

/* ========================================================================
 * Apply current config to the GLFW window
 * ======================================================================== */
static void apply_config_to_window(void) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = g_config.alpha;

    glfwSetWindowAttrib(g_window, GLFW_FLOATING,
                        g_config.always_on_top ? GLFW_TRUE : GLFW_FALSE);
#ifdef _WIN32
    glfwSetWindowAttrib(g_window, GLFW_MOUSE_PASSTHROUGH,
                        g_config.mouse_passthrough ? GLFW_TRUE : GLFW_FALSE);
#endif
}

/* ========================================================================
 * Render one frame
 * ======================================================================== */
bool overlay_window_frame(overlay_poll_speakers_fn poll, void *userdata) {
    if (glfwWindowShouldClose(g_window)) {
        return false;
    }

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    /* ================================================================
     * Settings panel
     * ================================================================ */
    if (g_settings_open) {
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
        bool show = true;
        if (ImGui::Begin(LOC("设置", "Settings"), &show, ImGuiWindowFlags_None)) {
            ImGui::SliderFloat(LOC("透明度", "Transparency"), &g_config.alpha,
                          0.1f, 1.0f, "%.2f", ImGuiSliderFlags_None);
            ImGui::Checkbox(LOC("鼠标穿透", "Mouse passthrough"), &g_config.mouse_passthrough);
            ImGui::Checkbox(LOC("窗口置顶", "Always on top"), &g_config.always_on_top);
            apply_config_to_window();

            ImGui::Separator();

            ImGui::TextWrapped(
                LOC("拖拽窗口标题栏来移动位置。\n"
                    "关闭此窗口不会停止插件。",
                    "Drag the title bar to move.\n"
                    "Closing this window does not stop the plugin.")
            );
        }
        ImGui::End();

        if (!show) {
            g_settings_open = false;
        }
    }

    /* ================================================================
     * Main overlay panel — fills the entire GLFW window
     * ================================================================ */
    int display_w, display_h;
    glfwGetWindowSize(g_window, &display_w, &display_h);

    ImGuiCond pos_cond = g_first_frame ? ImGuiCond_FirstUseEver : ImGuiCond_Always;
    ImGui::SetNextWindowPos(ImVec2(0, 0), pos_cond, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)display_w, (float)display_h), ImGuiCond_Always);

    ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoTitleBar
                                | ImGuiWindowFlags_NoResize
                                | ImGuiWindowFlags_NoMove
                                | ImGuiWindowFlags_NoCollapse
                                | ImGuiWindowFlags_NoBringToFrontOnFocus
                                | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("SpeakingOverlayMain", NULL, main_flags);

    /* ---- Header line: title + settings button ---- */
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), LOC("说话列表", "Speaking Users"));

    ImGui::SameLine(0.0f, -1.0f);
    float cursor_x = ImGui::GetCursorPosX();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosX(cursor_x + avail.x - 80.0f);

    if (ImGui::SmallButton(LOC("设置", "Settings"))) {
        g_settings_open = !g_settings_open;
    }

    ImGui::Separator();

    /* ---- Speaking users list ---- */
    uint32_t user_ids[64];
    char     names[64][128];
    int      states[64];
    int user_count = poll ? poll(userdata, user_ids, names, states, 64) : 0;

    if (user_count == 0) {
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f),
                      LOC("  当前没人说话...", "  Nobody is speaking..."));
    } else {
        for (int i = 0; i < user_count; i++) {
            ImVec4 col;
            const char *status_text;
            switch (states[i]) {
                case 1: /* SU_TALKING */
                    col = ImVec4(0.2f, 1.0f, 0.3f, 1.0f);
                    status_text = LOC("说话", "Talking");
                    break;
                case 2: /* SU_WHISPERING */
                    col = ImVec4(1.0f, 0.9f, 0.2f, 1.0f);
                    status_text = LOC("密语", "Whisper");
                    break;
                case 3: /* SU_SHOUTING */
                    col = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
                    status_text = LOC("喊话", "Shout");
                    break;
                default:
                    col = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    status_text = "";
                    break;
            }

            /* User row: colored bullet + name, status aligned right */
            ImGui::TextColored(col, "  \xe2\x97\x8f  %s", names[i]);

            ImGui::SameLine(0.0f, -1.0f);
            float px = ImGui::GetCursorPosX();
            ImVec2 av = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPosX(px + av.x - 60.0f);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", status_text);
        }
    }

    ImGui::End();

    /* ================================================================
     * Render
     * ================================================================ */
    ImGui::Render();

    {
        int fb_w, fb_h;
        glfwGetFramebufferSize(g_window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ImDrawData *draw_data = ImGui::GetDrawData();
    if (draw_data != NULL) {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }

    glfwSwapBuffers(g_window);

    if (g_first_frame) g_first_frame = false;

    /* Track window position / size for config persistence */
    glfwGetWindowPos(g_window, &g_config.window_x, &g_config.window_y);
    glfwGetWindowSize(g_window, &g_config.window_width, &g_config.window_height);

    return true;
}

/* ========================================================================
 * Shutdown
 * ======================================================================== */
void overlay_window_shutdown(void) {
    if (g_window != NULL) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(g_window);
        g_window = NULL;
    }
    glfwTerminate();
}

/* ========================================================================
 * Get current config
 * ======================================================================== */
void overlay_window_get_config(overlay_config_t *cfg) {
    *cfg = g_config;
}

#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
