#pragma once
// Renderer internals shared by render.cpp (water, seabed, light, HUD) and props.cpp (what grows and swims).
#include "render.h"
#include "universe.h"
#include <math.h>
#include <string.h>

namespace mantis {
namespace rr {

constexpr int W = FB_W, H = FB_H;
constexpr float FOV = 1.10f;                       // radians across the screen
constexpr float F = (W * 0.5f) / 0.61310f;          // focal length in pixels (tan(FOV/2) = 0.6131)

// ---------------- palette layout ----------------
enum : uint8_t {
    R_WATER = 0,      // 16: the water column, looking down (dark) .. up toward the light
    R_SURF = 16,      // 16: the surface from below: 0..7 reflection, 8..15 the window of sky
    R_SUN = 32,       // 16: sunlight through the water: shafts, glints, caustic tops
    R_SED = 48,       // 32: seabed sediment: 4 haze bands x 8 shades
    R_ROCK = 80,      // 32: seabed rock: 4 haze bands x 8 shades
    R_FAR = 112,      // 16: far silhouettes
    R_FLORA = 128,    // 16: kelp, grass: 0..7 near, 8..15 hazed
    R_STONE = 144,    // 16: rocks, pillars, ruins, wrecks: 0..7 near, 8..15 hazed
    R_CORAL = 160,    // 16: 4 hues x 4 shades
    R_GLOW = 176,     // 16: bioluminescence, 4 hues x 4 strengths (emissive)
    R_FAUNA = 192,    // 16: creatures
    R_FAUNA2 = 208,   // 16: reef fish, silver, rust
    R_HOT = 224,      // 8: lava and fire (emissive)
    R_MISC = 232,     // 8: snow, bubbles, plankton, sonar, scan
    R_HUD = 240       // 16: instruments (fixed)
};
enum : uint8_t { M_SNOW, M_SNOW_L, M_BUB_D, M_BUB_L, M_PLANK, M_PLANK_L, M_SONAR, M_SCAN };
enum : uint8_t { HU_BLACK, HU_DTEAL, HU_TEAL, HU_BTEAL, HU_WHITE, HU_AMBER, HU_RED, HU_GREY, HU_GREEN, HU_DAMBER };
enum : uint8_t {   // R_FAUNA
    FA_BLACK, FA_DGREY, FA_GREY, FA_LGREY, FA_WHITE, FA_SHARK, FA_TURT_B, FA_TURT_G, FA_SHELL, FA_OCTO_R, FA_OCTO_O,
    FA_JELLY_P, FA_JELLY_V, FA_NARWHAL, FA_WHALE, FA_DRED };
enum : uint8_t {   // R_FAUNA2
    F2_ORANGE, F2_YELLOW, F2_BLUE, F2_CYAN, F2_PURPLE, F2_PINK, F2_RED, F2_GREEN, F2_SILV_D, F2_SILV, F2_SILV_L,
    F2_RUST_D, F2_RUST, F2_LILAC, F2_COEL, F2_COEL_S };

// ---------------- frame state ----------------
extern uint8_t fb[W * H];
extern uint8_t* zb;                       // depth code per pixel (255 = open water); heap, to keep static RAM small
extern float g_cx, g_cy, g_cz, g_cY, g_sY, g_t, g_vis, g_pitch, g_yaw;
extern int g_hor, g_pan;
extern float g_alpha;                     // < 1: dissolve through ordered dither (haze, fade-in)
extern uint8_t g_zc;                      // the depth code of what's being drawn
extern uint8_t HW[17][17];
extern float g_surge, g_lampNear;

static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
inline float dith(int x, int y) { return (BAYER[(((y - g_hor) & 3) << 2) | ((x + g_pan) & 3)] + 0.5f) * (1.f / 16.f); }
inline int band(float f, int x, int y) {
    int i = (int)floorf(f);
    float fr = f - i;
    if (fr > 0.86f) return i + 1;
    if (fr > 0.6f && (((x + g_pan) ^ (y - g_hor)) & 1)) return i + 1;
    return i;
}
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline float smoothstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }
// a fast sine (parabolic, ~0.1% error): the surface and shimmer use thousands a frame
inline float fsin(float x) {
    x *= 0.15915494f; x -= floorf(x + 0.5f);              // -0.5 .. 0.5 turns
    float y = 8.f * x - 16.f * x * fabsf(x);
    return 0.225f * (y * fabsf(y) - y) + y;
}
inline uint8_t zcode(float d) { float c = sqrtf(fmaxf(d, 0.f)) * 22.f; return (uint8_t)(c > 254.f ? 254.f : c); }
inline float fogOf(float d) { return 1.f - expf(-d / g_vis); }

inline void put(int x, int y, uint8_t c) {
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) {
        int i = y * W + x;
        if (zb[i] < g_zc) return;
        if (g_alpha < 1.f && dith(x, y) >= g_alpha) return;
        fb[i] = c;
    }
}
inline void hspan(int x0, int x1, int y, uint8_t c) {
    if ((unsigned)y >= (unsigned)H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > W - 1) x1 = W - 1;
    for (int x = x0; x <= x1; x++) {
        int i = y * W + x;
        if (zb[i] < g_zc) continue;
        if (g_alpha < 1.f && dith(x, y) >= g_alpha) continue;
        fb[i] = c;
    }
}
inline void splat(int cx, int cy, int r, uint8_t c) {
    if (r > 16) r = 16;
    if (r <= 0) { put(cx, cy, c); return; }
    for (int dy = -r; dy <= r; dy++) { int hw = HW[r][dy < 0 ? -dy : dy]; hspan(cx - hw, cx + hw, cy + dy, c); }
}
void line(int x0, int y0, int x1, int y1, uint8_t c);
void thickLine(float x0, float y0, float x1, float y1, float w, uint8_t c);
void ellipse(float cx, float cy, float rx, float ry, uint8_t c);

// Project a world point. f = forward distance (metres); false if behind you.
inline bool proj(float wx, float wy, float wz, float& sx, float& sy, float& f) {
    float dx = wx - g_cx, dy = wy - g_cy;
    f = dx * g_cY + dy * g_sY;
    if (f < 0.25f) return false;
    float l = -dx * g_sY + dy * g_cY;
    float inv = F / f;
    sx = W * 0.5f + l * inv;
    sy = (float)g_hor - (wz - g_cz) * inv;
    return true;
}

// colour helpers: near / hazed halves of the 16-entry ramps
inline uint8_t floraI(int s, bool far) { return (uint8_t)(R_FLORA + (far ? 8 : 0) + clampi(s, 0, 7)); }
inline uint8_t stoneI(int s, bool far) { return (uint8_t)(R_STONE + (far ? 8 : 0) + clampi(s, 0, 7)); }
inline uint8_t coralI(int hue, int s) { return (uint8_t)(R_CORAL + (hue & 3) * 4 + clampi(s, 0, 3)); }
inline uint8_t glowI(int hue, int s) { return (uint8_t)(R_GLOW + (hue & 3) * 4 + clampi(s, 0, 3)); }
inline uint8_t hotI(int s) { return (uint8_t)(R_HOT + clampi(s, 0, 7)); }

// props.cpp
void drawProps();
void propsInit();

}
}
