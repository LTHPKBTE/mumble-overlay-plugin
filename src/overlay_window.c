/*
 * overlay_window.c — GLFW + cimgui overlay window implementation
 *
 * Creates a floating overlay with:
 *   - Transparency control (alpha slider)
 *   - Optional mouse passthrough
 *   - Always-on-top
 *   - Live list of currently speaking users
 *   - System language detection for UI strings
 *
 * Uses cimgui (Dear ImGui C bindings).
 */

/* ---- Must be defined BEFORE including cimgui.h ---- */
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3

#include "cimgui.h"
#include "cimgui_impl.h"

/* macro alias — cimgui v1.92+ uses igGetIO_Nil */
#define igGetIO igGetIO_Nil

#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ---- Overlay internal header ---- */
#include "overlay_window.h"

/* ========================================================================
 * OpenGL 1.1 symbols — provided by opengl32.dll / libGL.so
 * These are linked directly (no loader needed for basic calls).
 * ======================================================================== */
#define GL_COLOR_BUFFER_BIT 0x00004000
#ifdef _WIN32
/* Windows: opengl32.dll uses __stdcall */
#define GL_CALL __stdcall
extern void GL_CALL glClear(unsigned int mask);
extern void GL_CALL glClearColor(float r, float g, float b, float a);
extern void GL_CALL glViewport(int x, int y, int w, int h);
#else
extern void glClear(unsigned int mask);
extern void glClearColor(float r, float g, float b, float a);
extern void glViewport(int x, int y, int w, int h);
#endif

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
 * Helper: make an ImVec2_c / ImVec4_c
 * ======================================================================== */
static inline ImVec2_c mkvec2(float x, float y) {
    ImVec2_c v;
    v.x = x; v.y = y;
    return v;
}

static inline ImVec4_c mkvec4(float x, float y, float z, float w) {
    ImVec4_c v;
    v.x = x; v.y = y; v.z = z; v.w = w;
    return v;
}

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

    /* --- cimgui --- */
    igCreateContext(NULL);
    ImGuiIO *io = igGetIO();
    io->IniFilename = NULL;

#ifdef IMGUI_HAS_DOCK
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    ImGuiStyle *style = igGetStyle();
    igStyleColorsDark(NULL);
    style->Alpha = g_config.alpha;

    /* --- Backend init --- */
    if (!ImGui_ImplGlfw_InitForOpenGL(g_window, true)) {
        igDestroyContext(NULL);
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
        igDestroyContext(NULL);
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
    ImGuiStyle *style = igGetStyle();
    style->Alpha = g_config.alpha;

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
    igNewFrame();

    /* ================================================================
     * Settings panel
     * ================================================================ */
    if (g_settings_open) {
        igSetNextWindowSize(mkvec2(300, 200), ImGuiCond_FirstUseEver);
        bool show = true;
        if (igBegin(LOC("设置", "Settings"), &show, ImGuiWindowFlags_None)) {
            igSliderFloat(LOC("透明度", "Transparency"), &g_config.alpha,
                          0.1f, 1.0f, "%.2f", ImGuiSliderFlags_None);
            igCheckbox(LOC("鼠标穿透", "Mouse passthrough"), &g_config.mouse_passthrough);
            igCheckbox(LOC("窗口置顶", "Always on top"), &g_config.always_on_top);
            apply_config_to_window();

            igSeparator();

            igTextWrapped(
                LOC("拖拽窗口标题栏来移动位置。\n"
                    "关闭此窗口不会停止插件。",
                    "Drag the title bar to move.\n"
                    "Closing this window does not stop the plugin.")
            );
        }
        igEnd();

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
    igSetNextWindowPos(mkvec2(0, 0), pos_cond, mkvec2(0, 0));
    igSetNextWindowSize(mkvec2((float)display_w, (float)display_h), ImGuiCond_Always);

    ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoTitleBar
                                | ImGuiWindowFlags_NoResize
                                | ImGuiWindowFlags_NoMove
                                | ImGuiWindowFlags_NoCollapse
                                | ImGuiWindowFlags_NoBringToFrontOnFocus
                                | ImGuiWindowFlags_NoSavedSettings;

    igBegin("SpeakingOverlayMain", NULL, main_flags);

    /* ---- Header line: title + settings button ---- */
    igTextColored(mkvec4(1.0f, 1.0f, 1.0f, 1.0f), LOC("说话列表", "Speaking Users"));

    igSameLine(0.0f, -1.0f);
    float cursor_x = igGetCursorPosX();
    ImVec2_c avail = igGetContentRegionAvail();
    igSetCursorPosX(cursor_x + avail.x - 80.0f);

    if (igSmallButton(LOC("设置", "Settings"))) {
        g_settings_open = !g_settings_open;
    }

    igSeparator();

    /* ---- Speaking users list ---- */
    uint32_t user_ids[64];
    char     names[64][128];
    int      states[64];
    int user_count = poll ? poll(userdata, user_ids, names, states, 64) : 0;

    if (user_count == 0) {
        igTextColored(mkvec4(0.45f, 0.45f, 0.45f, 1.0f),
                      LOC("  当前没人说话...", "  Nobody is speaking..."));
    } else {
        for (int i = 0; i < user_count; i++) {
            ImVec4_c col;
            const char *status_text;
            switch (states[i]) {
                case 1: /* SU_TALKING */
                    col = mkvec4(0.2f, 1.0f, 0.3f, 1.0f);
                    status_text = LOC("说话", "Talking");
                    break;
                case 2: /* SU_WHISPERING */
                    col = mkvec4(1.0f, 0.9f, 0.2f, 1.0f);
                    status_text = LOC("密语", "Whisper");
                    break;
                case 3: /* SU_SHOUTING */
                    col = mkvec4(1.0f, 0.25f, 0.25f, 1.0f);
                    status_text = LOC("喊话", "Shout");
                    break;
                default:
                    col = mkvec4(0.7f, 0.7f, 0.7f, 1.0f);
                    status_text = "";
                    break;
            }

            /* User row: colored bullet + name, status aligned right */
            igTextColored(col, "  \xe2\x97\x8f  %s", names[i]);

            igSameLine(0.0f, -1.0f);
            float px = igGetCursorPosX();
            ImVec2_c av = igGetContentRegionAvail();
            igSetCursorPosX(px + av.x - 60.0f);
            igTextColored(mkvec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", status_text);
        }
    }

    igEnd();

    /* ================================================================
     * Render
     * ================================================================ */
    igRender();

    {
        int fb_w, fb_h;
        glfwGetFramebufferSize(g_window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ImDrawData *draw_data = igGetDrawData();
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
        igDestroyContext(NULL);
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
