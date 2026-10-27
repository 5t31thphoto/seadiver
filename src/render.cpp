// Mantis Seadiver renderer.
// 160x120 indexed framebuffer. Every pixel is a palette index; the palette is rebuilt each frame from the time
// of day, the weather above, your depth and the water you're in, so light at depth (red goes first, then green,
// then everything), the sun's warmth near the surface and the dark of the deep are palette work, not pixel work.
// Back to front: the water column -> the surface from below (the window of sky, ripples, ice) -> the seabed
// (a voxel heightfield with caustics, haze and depth) -> everything that grows and swims (props.cpp) ->
// marine snow and plankton -> light shafts -> the floodlights' cone -> sonar -> instruments.

#include <stdio.h>
#include "render_int.h"
#include "scene.h"
#include "journey.h"
#include "terrain.h"
#include "field.h"
#include "helm.h"
#include "mem.h"
#ifdef HOST
#include <stdio.h>
#include <stdlib.h>
#endif

namespace mantis {
HudInfo g_hud;
namespace rr {

uint8_t fb[W * H];
uint8_t* zb = nullptr;
float g_cx, g_cy, g_cz, g_cY = 1.f, g_sY = 0.f, g_t, g_vis = 40.f, g_pitch, g_yaw;
int g_hor = H / 2, g_pan = 0;
float g_alpha = 1.f;
uint8_t g_zc = 0;
uint8_t HW[17][17];
float g_surge = 0.f, g_lampNear = 0.f;

static uint16_t pal565[256];
static uint8_t palRGB[256 * 3];
static uint8_t s_dn[256], s_up[256];      // one step darker / lighter within the same ramp
static bool s_emis[256];                  // makes its own light: the lamp's cone and shadows leave it alone
static uint8_t* s_ct = nullptr;           // 64x64 caustic net
static float s_sunX = 80.f, s_sunY = -40.f, s_sunVis = 0.f;

void line(int x0, int y0, int x1, int y1, uint8_t c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int n = 0; n < 600; n++) {
        put(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void thickLine(float x0, float y0, float x1, float y1, float w, uint8_t c) {
    if (w <= 1.2f) { line((int)x0, (int)y0, (int)x1, (int)y1, c); return; }
    float dx = x1 - x0, dy = y1 - y0;
    float L = sqrtf(dx * dx + dy * dy);
    int n = (int)(L / fmaxf(1.f, w * 0.5f)) + 1;
    if (n > 200) n = 200;
    int r = (int)(w * 0.5f);
    for (int i = 0; i <= n; i++) { float u = (float)i / n; splat((int)(x0 + dx * u), (int)(y0 + dy * u), r, c); }
}
void ellipse(float cx, float cy, float rx, float ry, uint8_t c) {
    int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
    for (int y = y0; y <= y1; y++) {
        float v = (y + 0.5f - cy) / ry;
        if (v * v >= 1.f) continue;
        float hw = rx * sqrtf(1.f - v * v);
        hspan((int)(cx - hw + 0.5f), (int)(cx + hw - 0.5f), y, c);
    }
}

// ---------------- colour ----------------
struct C3 { float r, g, b; };
static inline C3 mix(C3 a, C3 b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline C3 scale(C3 a, float s) { return {a.r * s, a.g * s, a.b * s}; }
static inline C3 add(C3 a, C3 b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
static inline C3 mulc(C3 a, C3 b) { return {a.r * b.r, a.g * b.g, a.b * b.b}; }
static inline C3 c3(const float* v) { return {v[0], v[1], v[2]}; }
static inline float lum(C3 c) { return c.r * 0.3f + c.g * 0.55f + c.b * 0.15f; }

// The sky over the sea through the day (MantisPlanet's keys): zenith, horizon, glow, sun, ambient, key light.
struct Key { float t; C3 zen, hor, glow, sun, amb, key; float night; };
#define NIGHT_KEY(T) {T, {6, 5, 20}, {24, 18, 52}, {40, 24, 70}, {215, 220, 255}, {70, 75, 120}, {60, 70, 150}, 1.f}
static const Key KEYS[] = {
    NIGHT_KEY(0.00f),
    {0.19f, {18, 14, 50}, {75, 42, 105}, {160, 75, 115}, {255, 200, 160}, {105, 92, 140}, {190, 105, 125}, 0.55f},
    {0.26f, {58, 48, 128}, {238, 118, 92}, {255, 188, 112}, {255, 240, 190}, {160, 130, 140}, {255, 150, 80}, 0.f},
    {0.36f, {70, 120, 210}, {188, 208, 235}, {255, 240, 200}, {255, 255, 230}, {220, 220, 230}, {255, 230, 180}, 0.f},
    {0.50f, {40, 100, 215}, {168, 208, 245}, {240, 248, 255}, {255, 255, 240}, {255, 255, 255}, {255, 245, 220}, 0.f},
    {0.63f, {62, 90, 190}, {218, 190, 172}, {255, 220, 150}, {255, 245, 200}, {230, 215, 205}, {255, 210, 140}, 0.f},
    {0.71f, {82, 50, 142}, {255, 118, 70}, {255, 200, 90}, {255, 240, 170}, {170, 115, 128}, {255, 128, 50}, 0.f},
    {0.79f, {35, 22, 75}, {178, 50, 100}, {255, 110, 70}, {255, 180, 120}, {105, 78, 115}, {200, 70, 80}, 0.35f},
    NIGHT_KEY(0.89f),
    NIGHT_KEY(1.00f),
};
static constexpr int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);
static Key sampleKey(float tod) {
    int k = 0;
    while (k < NKEYS - 2 && tod >= KEYS[k + 1].t) k++;
    const Key &A = KEYS[k], &B = KEYS[k + 1];
    float t = clampf((tod - A.t) / (B.t - A.t), 0.f, 1.f);
    t = t * t * (3.f - 2.f * t);
    Key o;
    o.t = tod;
    o.zen = mix(A.zen, B.zen, t); o.hor = mix(A.hor, B.hor, t); o.glow = mix(A.glow, B.glow, t);
    o.sun = mix(A.sun, B.sun, t); o.amb = mix(A.amb, B.amb, t); o.key = mix(A.key, B.key, t);
    o.night = A.night + (B.night - A.night) * t;
    return o;
}
static C3 spectrum(float t) {
    static const C3 K[7] = {{150, 90, 230}, {80, 120, 255}, {70, 200, 235}, {90, 225, 110}, {250, 235, 90}, {255, 160, 60}, {240, 70, 60}};
    float f = clampf(t, 0.f, 1.f) * 6.f;
    int i = (int)f;
    if (i >= 6) return K[6];
    return mix(K[i], K[i + 1], f - i);
}

static C3 s_fog;
static float s_lampW[4];

static void buildPalette() {
    const Scene& sc = g_scene;
    const BiomeParams& b = biomeAt(sc.distance);
    C3 P[256];
    Key K = sampleKey(sc.tod);
    // the weather greys the light that reaches the sea
    float grey = clampf(sc.cloud * 0.6f - 0.15f + sc.rain * 0.45f + sc.storm * 0.25f, 0.f, 0.9f);
    float sunUp = smoothstep(-0.05f, 0.15f, sc.sunElev) * (1.f - 0.75f * grey);
    C3 surf = scale(add(scale(K.amb, 0.5f), scale(K.key, 0.6f * sunUp)), 1.f / 255.f);
    surf = scale(surf, (0.22f + 0.78f * clampf(sc.dayLight * 1.2f, 0.f, 1.f)) * (1.f - 0.25f * grey));
    float d = fmaxf(0.f, sc.depth);
    auto T = [&](float dd) -> C3 { dd = fmaxf(0.f, dd); return {expf(-dd / b.absR), expf(-dd / b.absG), expf(-dd / b.absB)}; };
    C3 A = scale(mulc(surf, T(d)), 1.35f);                         // the light around you (red goes first)
    {   // the deep is never quite black: a faint light of its own (the reference's navy), so shapes still read
        float k = clampf(1.f - lum(A) * 3.f, 0.f, 1.f);
        C3 wm = scale(c3(b.water[1]), 1.f / 255.f);
        A = add(A, scale(add(C3{0.05f, 0.07f, 0.11f}, scale(wm, 1.2f)), k));
        if (sc.eruption > 0.01f || b.lava > 0.3f) A = add(A, scale(C3{0.32f, 0.1f, 0.03f}, sc.eruption * 0.8f + b.lava * 0.12f));   // the fire lights the water
    }
    // the floodlights: they matter when it's dark, and fall off with distance (the haze bands)
    float dark = clampf(1.f - lum(A) * 1.6f, 0.12f, 1.f);
    float lamp = sc.lights ? dark : 0.f;
    g_lampNear = lamp;
    static const float LW[4] = {1.f, 0.72f, 0.45f, 0.2f};
    for (int k = 0; k < 4; k++) s_lampW[k] = LW[k] * lamp;
    const C3 LAMP = {1.f, 0.94f, 0.82f};
    auto lit = [&](C3 base, float s, float lampW) -> C3 {            // base 0..255; s = 0 shadow .. 1 highlight
        C3 light = add(scale(A, 0.32f + 0.8f * s), scale(LAMP, lampW * 1.7f * (0.35f + 0.65f * s)));
        return mulc(base, light);
    };
    auto rampAt = [&](const float (&r)[4][3], float s) -> C3 {
        s = clampf(s, 0.f, 1.f) * 3.f;
        int i = (int)s;
        if (i >= 3) return c3(r[3]);
        return mix(c3(r[i]), c3(r[i + 1]), s - i);
    };
    // ---- the water column: what you see looking down (dark, deep) to looking up (bright) ----
    C3 wD = c3(b.water[0]), wM = c3(b.water[1]), wB = c3(b.water[2]);
    C3 tint = {sqrtf(fmaxf(surf.r, 0.01f)), sqrtf(fmaxf(surf.g, 0.01f)), sqrtf(fmaxf(surf.b, 0.01f))};   // dawn and dusk warm the shallows
    for (int i = 0; i < 16; i++) {
        float t = i / 15.f;
        C3 c = t < 0.5f ? mix(wD, wM, t * 2.f) : mix(wM, wB, (t - 0.5f) * 2.f);
        float dv = d + (0.55f - t) * 70.f;                           // looking down you see through more water
        float lv = clampf(sc.dayLight * 1.15f, 0.f, 1.1f) * expf(-fmaxf(0.f, dv) / (b.absG * 1.4f));
        c = mulc(c, mix(C3{1, 1, 1}, tint, clampf(1.f - d / 40.f, 0.f, 1.f) * 0.6f));
        P[R_WATER + i] = scale(c, 0.32f + 1.1f * lv);
    }
    s_fog = P[R_WATER + 6];
    // ---- the surface from below ----
    C3 skyW = scale(mulc(add(scale(K.hor, 0.6f), scale(K.zen, 0.4f)), T(d)), 1.f / 255.f);
    for (int i = 0; i < 8; i++) {
        float t = i / 7.f;
        P[R_SURF + i] = mix(P[R_WATER + 5], P[R_WATER + 13], 0.15f + 0.85f * t);          // the mirror: the water below, rippled
        C3 win = mix(P[R_WATER + 12], scale(skyW, 255.f * (0.5f + 0.9f * t)), 0.4f + 0.6f * t * clampf(1.f - d / 60.f, 0.f, 1.f));
        if (sc.aurora > 0.01f) {
            C3 au = t < 0.5f ? mix(C3{10, 40, 50}, C3{60, 235, 150}, t * 2.f) : mix(C3{60, 235, 150}, C3{190, 110, 255}, (t - 0.5f) * 2.f);
            win = mix(win, scale(mulc(au, T(d * 0.6f)), 1.f), sc.aurora * 0.85f);
        }
        P[R_SURF + 8 + i] = win;
    }
    if (b.iceCeiling > 0.02f) {                                       // under the ice: the ceiling is blue-white
        for (int i = 0; i < 16; i++) {
            float t = i / 15.f;
            C3 ice = mix(C3{40, 70, 110}, C3{200, 230, 250}, t);
            C3 litIce = scale(mulc(ice, add(scale(A, 0.5f + 0.6f * t), C3{0.04f, 0.06f, 0.1f})), 1.f);
            if (i == 15) litIce = mix(litIce, scale(skyW, 255.f), 0.6f);
            P[R_SURF + i] = mix(P[R_SURF + i], litIce, b.iceCeiling);
        }
    }
    // ---- sunlight in the water ----
    C3 sunW = scale(mulc(K.sun, T(d * 0.8f)), 1.05f);
    for (int i = 0; i < 16; i++) {
        float t = i / 15.f;
        P[R_SUN + i] = mix(P[R_WATER + 11], sunW, 0.25f + 0.75f * t);
        if (i == 15) P[R_SUN + i] = mix(sunW, C3{255, 255, 255}, 0.35f);
    }
    if (sc.prism > 0.05f)                                              // the light breaks into colours
        for (int i = 0; i < 16; i++) P[R_SUN + i] = mix(P[R_SUN + i], mix(spectrum(i / 15.f), P[R_WATER + 12], 0.25f), clampf(sc.prism * 1.3f, 0.f, 1.f));
    // ---- the seabed: two materials, four haze bands ----
    static const float FK[4] = {0.f, 0.32f, 0.58f, 0.8f};
    for (int band = 0; band < 4; band++)
        for (int j = 0; j < 8; j++) {
            float s = j / 7.f;
            P[R_SED + band * 8 + j] = mix(lit(rampAt(b.sed, s), s, s_lampW[band]), s_fog, FK[band]);
            P[R_ROCK + band * 8 + j] = mix(lit(rampAt(b.rock, s), s, s_lampW[band]), s_fog, FK[band]);
        }
    for (int i = 0; i < 16; i++) {
        float s = i / 15.f;
        P[R_FAR + i] = mix(lit(rampAt(b.rock, s * 0.8f), s, 0.f), s_fog, 0.7f);
    }
    // ---- what grows, stands and swims ----
    for (int i = 0; i < 16; i++) {
        bool far = i >= 8;
        float s = (i & 7) / 7.f;
        P[R_FLORA + i] = mix(lit(rampAt(b.flora, s), s, s_lampW[far ? 2 : 0]), s_fog, far ? 0.5f : 0.04f);
        P[R_STONE + i] = mix(lit(rampAt(b.stone, s), s, s_lampW[far ? 2 : 0]), s_fog, far ? 0.5f : 0.04f);
    }
    static const float CS[4] = {0.25f, 0.55f, 0.85f, 1.1f};
    for (int h = 0; h < 4; h++) {
        C3 base = c3(b.coral[h]);
        for (int s = 0; s < 4; s++) {
            C3 bs = s == 0 ? mulc(base, C3{0.55f, 0.5f, 0.75f}) : base;          // shadows lean violet
            P[R_CORAL + h * 4 + s] = lit(bs, CS[s], s_lampW[0]);
        }
    }
    C3 GL[4] = {c3(b.glow), {80, 230, 255}, {90, 255, 140}, {240, 110, 255}};
    static const float GS[4] = {0.22f, 0.45f, 0.75f, 1.f};
    for (int h = 0; h < 4; h++)
        for (int s = 0; s < 4; s++) {
            C3 g = scale(GL[h], GS[s]);
            if (s == 3) g = mix(g, C3{255, 255, 255}, 0.3f);
            P[R_GLOW + h * 4 + s] = add(mix(g, s_fog, 0.15f), scale(P[R_WATER + 4], 0.3f));
        }
    static const C3 FA[16] = {{20, 20, 26}, {60, 64, 72}, {120, 126, 136}, {190, 196, 204}, {240, 240, 236}, {90, 110, 130},
                              {110, 80, 46}, {90, 120, 60}, {190, 150, 80}, {200, 70, 50}, {240, 140, 80}, {240, 150, 200},
                              {170, 120, 240}, {140, 150, 160}, {40, 52, 70}, {140, 30, 40}};
    static const C3 F2[16] = {{250, 140, 40}, {250, 220, 60}, {50, 110, 240}, {80, 210, 230}, {150, 80, 210}, {250, 120, 170},
                              {220, 50, 50}, {90, 200, 90}, {80, 96, 110}, {170, 186, 196}, {230, 240, 245}, {70, 36, 24},
                              {150, 76, 40}, {170, 160, 200}, {40, 60, 110}, {200, 210, 220}};
    for (int i = 0; i < 16; i++) {
        P[R_FAUNA + i] = lit(FA[i], 0.75f, s_lampW[0] * 0.8f);
        P[R_FAUNA2 + i] = lit(F2[i], 0.8f, s_lampW[0] * 0.8f);
    }
    // jellies glow a little of their own in the dark
    P[R_FAUNA + FA_JELLY_P] = add(P[R_FAUNA + FA_JELLY_P], scale(C3{120, 60, 110}, b.glowAmt * 0.5f + 0.1f));
    P[R_FAUNA + FA_JELLY_V] = add(P[R_FAUNA + FA_JELLY_V], scale(C3{80, 60, 140}, b.glowAmt * 0.5f + 0.1f));
    static const C3 HOT[8] = {{70, 10, 8}, {140, 22, 8}, {205, 52, 10}, {240, 100, 20}, {255, 160, 40}, {255, 210, 90}, {255, 240, 170}, {255, 255, 230}};
    for (int i = 0; i < 8; i++) P[R_HOT + i] = mix(HOT[i], scale(HOT[i], 1.25f), sc.eruption);
    // particles: snow lit by whatever light there is; plankton and the sonar make their own
    P[R_MISC + M_SNOW] = add(mix(P[R_WATER + 9], lit({200, 210, 220}, 0.5f, s_lampW[1]), 0.6f), C3{6, 8, 10});
    P[R_MISC + M_SNOW_L] = add(lit({235, 240, 245}, 0.9f, s_lampW[0]), scale(P[R_WATER + 12], 0.4f));
    P[R_MISC + M_BUB_D] = mix(P[R_WATER + 6], P[R_WATER + 14], 0.6f);
    P[R_MISC + M_BUB_L] = add(mix(P[R_WATER + 15], C3{230, 245, 255}, 0.6f), scale(LAMP, lamp * 60.f));
    P[R_MISC + M_PLANK] = {40, 150, 200};
    P[R_MISC + M_PLANK_L] = {150, 255, 250};
    P[R_MISC + M_SONAR] = {90, 255, 170};
    P[R_MISC + M_SCAN] = {255, 200, 90};
    static const C3 HUD[16] = {{4, 8, 14}, {18, 48, 58}, {60, 150, 160}, {130, 236, 226}, {236, 246, 250}, {255, 190, 80}, {255, 80, 60}, {90, 110, 120},
                               {120, 255, 160}, {150, 100, 30}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (int i = 0; i < 16; i++) P[R_HUD + i] = HUD[i];
    // lightning far above: the whole sea flashes
    if (sc.flash > 0.01f)
        for (int i = 0; i < 240; i++) P[i] = mix(P[i], C3{200, 215, 255}, sc.flash * (i < R_SED ? 0.5f : 0.25f));
    // the dive begins out of the dark of the hatch
    if (sc.intro > 0.f) {
        float k = powf(sc.intro, 1.4f);
        for (int i = 0; i < 240; i++) P[i] = mix(P[i], C3{2, 8, 18}, k);
    }
    for (int i = 0; i < 256; i++) {
        int r = clampi((int)P[i].r, 0, 255), g = clampi((int)P[i].g, 0, 255), bl = clampi((int)P[i].b, 0, 255);
        palRGB[i * 3] = r; palRGB[i * 3 + 1] = g; palRGB[i * 3 + 2] = bl;
        uint16_t v = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (bl >> 3));
        pal565[i] = (uint16_t)((v >> 8) | (v << 8));
    }
}

static void buildStepTables() {
    for (int i = 0; i < 256; i++) { s_dn[i] = s_up[i] = (uint8_t)i; s_emis[i] = false; }
    auto ramp = [](int start, int len) {
        for (int i = 0; i < len; i++) {
            s_dn[start + i] = (uint8_t)(start + (i > 0 ? i - 1 : 0));
            s_up[start + i] = (uint8_t)(start + (i < len - 1 ? i + 1 : len - 1));
        }
    };
    ramp(R_WATER, 16); ramp(R_SURF, 8); ramp(R_SURF + 8, 8); ramp(R_SUN, 16); ramp(R_FAR, 16);
    for (int k = 0; k < 4; k++) { ramp(R_SED + k * 8, 8); ramp(R_ROCK + k * 8, 8); }
    ramp(R_FLORA, 8); ramp(R_FLORA + 8, 8); ramp(R_STONE, 8); ramp(R_STONE + 8, 8);
    for (int h = 0; h < 4; h++) { ramp(R_CORAL + h * 4, 4); ramp(R_GLOW + h * 4, 4); }
    ramp(R_HOT, 8);
    for (int i = R_SUN; i < R_SUN + 16; i++) s_emis[i] = true;
    for (int i = R_GLOW; i < R_GLOW + 16; i++) s_emis[i] = true;
    for (int i = R_HOT; i < 256; i++) s_emis[i] = true;
    s_emis[R_MISC + M_SNOW] = s_emis[R_MISC + M_SNOW_L] = s_emis[R_MISC + M_BUB_D] = s_emis[R_MISC + M_BUB_L] = false;
}

// A tiling net of light: cell edges of a 64x64 Voronoi, brightest where the cells meet.
static void buildCaustics() {
    if (!s_ct) s_ct = (uint8_t*)memFast(64 * 64);
    if (!s_ct) return;
    float px[18], py[18];
    for (int i = 0; i < 18; i++) { uint32_t h = hash32(0xCA057u + i * 7919u); px[i] = (h & 1023) / 1024.f * 64.f; py[i] = ((h >> 10) & 1023) / 1024.f * 64.f; }
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            float d1 = 1e9f, d2 = 1e9f;
            for (int i = 0; i < 18; i++)
                for (int oy = -64; oy <= 64; oy += 64)
                    for (int ox = -64; ox <= 64; ox += 64) {
                        float dx = x - px[i] - ox, dy = y - py[i] - oy;
                        float d = sqrtf(dx * dx + dy * dy);
                        if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
                    }
            float e = clampf(1.f - (d2 - d1) / 3.2f, 0.f, 1.f);
            s_ct[y * 64 + x] = (uint8_t)(e * e * 255.f);
        }
}
static inline float caustic(float wx, float wy, float t) {
    int a = s_ct[(((int)(wy * 4.f + t * 1.6f)) & 63) * 64 + (((int)(wx * 4.f + t * 2.3f)) & 63)];
    int c = s_ct[(((int)(wy * 3.3f - t * 1.9f + 21.f)) & 63) * 64 + (((int)(wx * 3.3f - t * 1.1f + 37.f)) & 63)];
    float v = (a + c) * (1.f / 510.f);
    return v * v * v;
}

// ---------------- the water column ----------------
static void drawWater() {
    const Scene& sc = g_scene;
    float sunAz = sc.sunAz;
    float glowK = s_sunVis * clampf(1.f - sc.depth / 70.f, 0.f, 1.f);
    float colGlow[W];
    for (int x = 0; x < W; x++) {
        float rel = wrapPi(sunAz - (g_yaw + atanf((x + 0.5f - W * 0.5f) / F)));
        colGlow[x] = glowK / (1.f + rel * rel * 2.5f);
    }
    for (int y = 0; y < H; y++) {
        float e = atanf((g_hor - y - 0.5f) / F);                       // the view's elevation on this row
        float base = clampf(0.5f + e * 0.95f, 0.f, 1.f) * 13.5f;
        float up = clampf(e * 1.4f + 0.2f, 0.f, 1.f);
        uint8_t* row = fb + y * W;
        uint8_t* zr = zb + y * W;
        for (int x = 0; x < W; x++) {
            float f = base + colGlow[x] * up * 3.5f;
            row[x] = (uint8_t)(R_WATER + clampi(band(f, x, y), 0, 15));
            zr[x] = 255;
        }
    }
}

// ---------------- the surface (or the ice) from below ----------------
static void drawSurface() {
    const Scene& sc = g_scene;
    const BiomeParams& b = biomeAt(sc.distance);
    bool ice = b.iceCeiling > 0.5f;
    float ceil = ice ? fminf(-3.f, sc.ceiling < -0.5f ? sc.ceiling : -6.f) : 0.f;
    float hc = ceil - g_cz;
    if (hc < 0.3f) hc = 0.3f;
    float reach = g_vis * 1.3f;
    float waves = 0.6f + 0.8f * sc.waves;
    float t = g_t;
    int ymax = g_hor - 1;
    if (ymax > H) ymax = H;
    for (int y = 0; y < ymax; y++) {
        float tanE = (g_hor - y - 0.5f) / F;
        float f = hc / tanE;
        if (f > reach) continue;
        for (int x = 0; x < W; x++) {
            float dxs = (x + 0.5f - W * 0.5f) / F;
            float l = dxs * f;
            float wx = g_cx + f * g_cY - l * g_sY, wy = g_cy + f * g_sY + l * g_cY;
            float hd = sqrtf(f * f + l * l);
            float dist = sqrtf(hd * hd + hc * hc);
            float fog = 1.f - expf(-dist / (g_vis * 1.1f));
            if (dith(x, y) < (fog - 0.55f) * 2.2f) continue;            // lost in the blue
            float e = atanf(hc / hd);
            uint8_t c;
            if (ice) {
                float n = vnoise2(wx * 0.18f, wy * 0.18f, 0x1CEu);
                float cr = fabsf(vnoise2(wx * 0.06f, wy * 0.06f, 0xC7Au) - 0.5f);
                float sh = 2.f + n * 4.5f + (e > 0.9f ? 1.5f : 0.f);
                if (cr < 0.018f) sh = 7.6f;                                  // a crack: light pours through
                else if (cr < 0.04f) sh += 1.5f;
                c = (uint8_t)(R_SURF + (sh > 4.f ? 8 : 0) + clampi(band(sh > 4.f ? sh - 2.f : sh, x, y), 0, 7));
            } else {
                float wv = fsin(wx * 0.9f + t * 1.3f + fsin(wy * 0.7f + t * 0.9f) * 1.4f) + 0.7f * fsin(wy * 1.1f - t * 1.1f + fsin(wx * 0.5f - t * 0.4f) * 1.2f);
                wv *= waves;
                float win = smoothstep(0.55f, 0.78f, e);                     // Snell's window: the sky above you
                if (win > dith(x, y)) c = (uint8_t)(R_SURF + 8 + clampi(band(3.2f + wv * 1.5f + win * 2.4f, x, y), 0, 7));
                else c = (uint8_t)(R_SURF + clampi(band(3.f + wv * 1.8f, x, y), 0, 7));
                if (sc.rain > 0.2f && (hash32((uint32_t)(int)(wx * 6.f) * 0x9E37u ^ (uint32_t)(int)(wy * 6.f) * 0x85EBu ^ (uint32_t)(int)(t * 7.f)) & 255u) < (uint32_t)(sc.rain * 10.f)) c = R_SURF + 15;
            }
            fb[y * W + x] = c;
        }
    }
    // the sun itself, bent into the window
    if (s_sunVis > 0.05f && sc.depth < 80.f && !ice) {
        float k = s_sunVis * clampf(1.f - sc.depth / 80.f, 0.f, 1.f);
        int r = (int)(5.f + 6.f * k);
        for (int dy = -r * 2; dy <= r * 2; dy++)
            for (int dx = -r * 2; dx <= r * 2; dx++) {
                int x = (int)s_sunX + dx, y = (int)s_sunY + dy;
                if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H || y >= g_hor) continue;
                float rr = sqrtf((float)(dx * dx + dy * dy)) / r;
                float v = k * (rr < 0.5f ? 15.f : 14.f * expf(-(rr - 0.5f) * 1.6f));
                if (v < 6.f + dith(x, y) * 2.f) continue;
                fb[y * W + x] = (uint8_t)(R_SUN + clampi((int)v, 0, 15));
            }
    }
}

// ---------------- the seabed: a voxel heightfield ----------------
static void drawSeabed() {
    const int16_t* Hh = terrainH();
    const uint8_t* Mm = terrainM();
    if (!Hh || !Mm) return;
    const Scene& sc = g_scene;
    const BiomeParams& b = biomeAt(sc.distance);
    const int ox = terrainOx(), oy = terrainOy();
    const float fmax = fminf(124.f, g_vis * 1.6f);
    const float clear = clampf(1.f - (sc.cloud - 0.5f) * 1.6f, 0.2f, 1.f) * (1.f - 0.6f * sc.rain);
    const float cs = b.caust * clampf(sc.dayLight * 1.3f, 0.f, 1.f) * clear * 3.2f;
    const float t = g_t;
    for (int x = 0; x < W; x++) {
        float dxs = (x + 0.5f - W * 0.5f) / F;
        float rdx = g_cY - dxs * g_sY, rdy = g_sY + dxs * g_cY;
        float lenH = sqrtf(1.f + dxs * dxs);
        int ybot = H;
        float pY = 1e9f, pSh = 0.f, pFog = 0.f, pWx = 0.f, pWy = 0.f, pFw = 0.f;
        uint8_t pMat = 0;
        bool first = true;
        float f = 0.3f;
        while (f < fmax && ybot > 0) {
            float wx = g_cx + rdx * f, wy = g_cy + rdy * f;
            float gx = wx * (1.f / TG_CELL), gy = wy * (1.f / TG_CELL);
            int ix = (int)floorf(gx), iy = (int)floorf(gy);
            if (ix < ox || iy < oy || ix + 1 >= ox + TG_N || iy + 1 >= oy + TG_N) break;
            float fx = gx - ix, fy = gy - iy;
            int i0 = ix & TG_MASK, i1 = (ix + 1) & TG_MASK, j0 = (iy & TG_MASK) * TG_N, j1 = ((iy + 1) & TG_MASK) * TG_N;
            float ha = Hh[j0 + i0], hb = Hh[j0 + i1], hc = Hh[j1 + i0], hd = Hh[j1 + i1];
            float h = ((ha + (hb - ha) * fx) * (1.f - fy) + (hc + (hd - hc) * fx) * fy) * 0.1f;
            float sy = g_hor - F * (h - g_cz) / f;
            uint8_t m0 = Mm[j0 + i0], m1 = Mm[j0 + i1], m2 = Mm[j1 + i0], m3 = Mm[j1 + i1];
            float sh = ((m0 & 7) + ((m1 & 7) - (m0 & 7)) * fx) * (1.f - fy) + ((m2 & 7) + ((m3 & 7) - (m2 & 7)) * fx) * fy;
            uint8_t mat = fx < 0.5f ? (fy < 0.5f ? m0 : m2) : (fy < 0.5f ? m1 : m3);
            uint8_t ft = tmFeat(mat);
            if (ft == TF_NONE) { uint8_t q[4] = {m0, m1, m2, m3}; for (int k = 0; k < 4; k++) if (tmFeat(q[k]) != TF_NONE) { ft = tmFeat(q[k]); mat = (uint8_t)((mat & 31) | (ft << 5)); break; } }
            float fw0 = ((tmFeat(m0) == ft) * (1.f - fx) + (tmFeat(m1) == ft) * fx) * (1.f - fy) + ((tmFeat(m2) == ft) * (1.f - fx) + (tmFeat(m3) == ft) * fx) * fy;
            if (ft == TF_BRINE || ft == TF_LAVA) fw0 = fw0 > 0.45f ? 1.f : 0.f;
            float vz = h - g_cz;
            float d3 = sqrtf(f * f * lenH * lenH + vz * vz);
            float fog = 1.f - expf(-d3 / g_vis);
            if (first) { pY = fmaxf(sy, (float)H); pSh = sh; pFog = fog; pWx = wx; pWy = wy; pMat = mat; pFw = fw0; first = false; }
            int iyTop = (int)ceilf(sy);
            if (iyTop < ybot) {
                int r0 = iyTop < 0 ? 0 : iyTop;
                float span = pY - sy;
                uint8_t zc = zcode(d3);
                float cdepth = expf(h / 22.f);                                  // caustics fade with the floor's depth
                for (int y = ybot - 1; y >= r0; y--) {
                    float u = span > 0.01f ? clampf((pY - (y + 0.5f)) / span, 0.f, 1.f) : 1.f;   // 0 = previous sample (nearer) .. 1 = this one
                    float px = pWx + (wx - pWx) * u, py = pWy + (wy - pWy) * u;
                    float fg = pFog + (fog - pFog) * u;
                    uint8_t mt = u < 0.5f ? pMat : mat;
                    int idx = y * W + x;
                    zb[idx] = zc;
                    uint8_t feat = tmFeat(mt);
                    if (fg > 0.84f && dith(x, y) < (fg - 0.84f) * 6.25f && feat != TF_LAVA) continue;   // the haze takes it
                    uint32_t hh = hash32((uint32_t)(int)floorf(px * 2.f) * 0x9E3779B1u ^ (uint32_t)(int)floorf(py * 2.f) * 0x85EBCA77u);
                    float g = ((hh & 255) * (1.f / 255.f) - 0.5f) * clampf(1.f - fg * 1.8f, 0.f, 1.f);
                    float s = pSh + (sh - pSh) * u + g * 0.9f;
                    float fw = pFw + (fw0 - pFw) * u;                                 // how much of this spot has the feature
                    if (feat != TF_NONE && fw < ((hh >> 24) & 255) * (1.f / 255.f)) feat = TF_NONE;
                    if (cs > 0.03f && fg < 0.7f) s += caustic(px, py, t) * cs * cdepth * (1.f - fg);
                    int bnd = clampi((int)(fg * 3.7f + dith(x, y) * 0.9f - 0.2f), 0, 3);
                    bool rock = (tmRock(mt) * 0.3f + 0.08f) > ((hh >> 8) & 255) * (1.f / 255.f);
                    uint8_t c = (uint8_t)((rock ? R_ROCK : R_SED) + bnd * 8 + clampi(band(s, x, y), 0, 7));
                    switch (feat) {
                        case TF_GRASS: if (((hh >> 16) & 3) != 0) c = floraI(band(s * 0.8f + ((hh >> 20) & 1), x, y), fg > 0.45f); break;
                        case TF_LAVA: {
                            float fl = fsin(px * 2.1f + t * 2.7f) + fsin(py * 1.7f - t * 1.9f);
                            c = hotI((int)(3.5f + fl * 1.3f + g * 2.f + g_scene.eruption * 2.f));
                            break;
                        }
                        case TF_BRINE: {
                            float v = fsin(px * 1.6f + t * 0.9f + fsin(py * 0.8f) * 1.5f) + fsin(py * 1.3f - t * 0.7f);
                            c = v > 1.6f ? glowI(0, 3) : glowI(0, clampi(band(0.5f + v * 0.75f - fg * 1.2f, x, y), 0, 2));
                            break;
                        }
                        case TF_GLOW: if (((hh >> 16) & 15) == 0) c = glowI(1 + ((hh >> 22) & 1), 2 + ((hh >> 24) & 1)); break;
                        case TF_SNOW: c = (uint8_t)(R_SED + bnd * 8 + clampi(band(s + 2.f, x, y), 0, 7)); break;
                        case TF_RUBBLE: if (((hh >> 16) & 7) == 0 && fg < 0.5f) c = coralI((hh >> 20) & 3, 1 + ((hh >> 23) & 1)); break;
                        case TF_TUBES: if (((hh >> 16) & 7) == 0 && fg < 0.5f) c = (uint8_t)(R_FAUNA + (((hh >> 20) & 1) ? FA_DRED : FA_WHITE)); break;
                        default: break;
                    }
                    fb[idx] = c;
                }
                ybot = r0;
            }
            pY = sy; pSh = sh; pFog = fog; pWx = wx; pWy = wy; pMat = mat; pFw = fw0;
            f += 0.06f + f * 0.035f;
        }
    }
}

// ---------------- marine snow, plankton, bubbles ----------------
static void drawParticles() {
    const Scene& sc = g_scene;
    float dens = clampf(sc.snow * 0.55f + sc.murk * 0.3f + 0.08f, 0.f, 0.9f);
    float bloom = sc.bloom;
    const float C = 2.6f;
    int cx0 = (int)floorf(g_cx / C), cy0 = (int)floorf(g_cy / C), cz0 = (int)floorf(g_cz / C);
    float fall = g_t * 0.12f;
    for (int k = -3; k <= 3; k++)
        for (int j = -5; j <= 5; j++)
            for (int i = -5; i <= 5; i++) {
                uint32_t h = hash32((uint32_t)(cx0 + i) * 0x9E3779B1u ^ (uint32_t)(cy0 + j) * 0x85EBCA77u ^ (uint32_t)(cz0 + k) * 0xC2B2AE3Du);
                if ((h & 1023u) > (uint32_t)(dens * 1023.f)) continue;
                float wx = (cx0 + i + ((h >> 10) & 255) / 255.f) * C;
                float wy = (cy0 + j + ((h >> 18) & 255) / 255.f) * C;
                float wz = (cz0 + k) * C + fmodf(((h >> 26) & 63) / 63.f * C - fall * (0.6f + ((h >> 4) & 3) * 0.2f), C);
                wx += sinf(g_t * 0.4f + (h & 63)) * 0.15f * (1.f + g_surge);
                float sx, sy, f;
                if (!proj(wx, wy, wz, sx, sy, f)) continue;
                if (f > 14.f) continue;
                int x = (int)sx, y = (int)sy;
                if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) continue;
                g_zc = zcode(f);
                if (zb[y * W + x] < g_zc) continue;
                float near = 1.f - f / 14.f;
                if (bloom > 0.1f) {                                           // plankton light up where the hull pushes through
                    float stir = clampf(sc.speed / 4.f, 0.f, 1.f) * clampf(1.f - f / 6.f, 0.f, 1.f) + fabsf(helmTurnRate()) * 0.3f;
                    if (stir * bloom > 0.15f + ((h >> 3) & 7) * 0.08f) { fb[y * W + x] = R_MISC + (stir > 0.6f ? M_PLANK_L : M_PLANK); if (f < 3.f) put(x + 1, y, R_MISC + M_PLANK); continue; }
                }
                if (near < 0.3f && ((h >> 6) & 1)) continue;
                uint8_t c = R_MISC + (near > 0.6f ? M_SNOW_L : M_SNOW);
                fb[y * W + x] = c;
                if (f < 2.5f) { put(x + 1, y, c); put(x, y + 1, c); }
            }
    // the screw's wake: bubbles stream past the bottom corners at speed
    float spd = sc.speed;
    if (spd > 3.f) {
        int n = (int)((spd - 3.f) * 3.f);
        for (int i = 0; i < n; i++) {
            uint32_t h = hash32(i * 0x9E37u + 0x8B1Bu);
            float ph = fmodf(g_t * (0.8f + (h & 7) * 0.15f) + (h >> 8 & 255) / 255.f, 1.f);
            float side = (h & 0x100) ? 1.f : -1.f;
            int x = (int)(W * 0.5f + side * (30.f + ph * 70.f + ((h >> 12) & 15)));
            int y = (int)(H - 4 - ph * 36.f + ((h >> 16) & 7));
            if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) { fb[y * W + x] = R_MISC + M_BUB_L; if (ph < 0.4f) { int x2 = x + (side > 0 ? 1 : -1); if ((unsigned)x2 < (unsigned)W) fb[y * W + x2] = R_MISC + M_BUB_D; } }
        }
    }
}

// ---------------- light shafts: screen-space scattering from the sun above ----------------
static constexpr int RAY_NB = 192, RAY_NR = 110;
static constexpr float RAY_STEP = 3.f;
static uint8_t* s_rayT = nullptr;
static float s_occ[256];
static void buildOcc() {
    for (int i = 0; i < 256; i++) {
        float o = 0.f;
        if (i >= R_SED && i < R_FAR) o = 0.5f;
        else if (i >= R_FAR && i < R_FLORA) o = 0.2f;
        else if (i >= R_FLORA && i < R_FLORA + 8) o = 0.45f;
        else if (i >= R_FLORA + 8 && i < R_STONE) o = 0.15f;
        else if (i >= R_STONE && i < R_STONE + 8) o = 0.6f;
        else if (i >= R_STONE + 8 && i < R_CORAL) o = 0.2f;
        else if (i >= R_CORAL && i < R_GLOW) o = 0.4f;
        else if (i >= R_FAUNA && i < R_HOT) o = 0.35f;
        else if (i >= R_SURF && i < R_SURF + 8) o = 0.03f;                     // the mirror side of the surface dims them a little
        s_occ[i] = o;
    }
}
static void drawShafts() {
    const Scene& sc = g_scene;
    float str = sc.beams * 1.3f + sc.prism * 0.6f;
    if (str < 0.04f || !s_rayT) return;
    // the sun sits above the view: shafts fan down from it, through gaps in the kelp and the ice
    float pat0 = g_t * 0.22f;
    for (int bi = 0; bi < RAY_NB; bi++) {
        float ang = bi * (kTau / RAY_NB);
        float dx = cosf(ang) * RAY_STEP, dy = sinf(ang) * RAY_STEP;
        float pat = fbmP((float)bi * (48.f / RAY_NB), pat0, 48, 2, 0x4A7u);
        pat = 0.3f + 0.7f * clampf((pat - 0.35f) * 2.6f, 0.f, 1.f);
        float x = s_sunX, y = s_sunY, T = 1.f;
        for (int k = 0; k < RAY_NR; k++) {
            x += dx; y += dy;
            int ix = (int)x, iy = (int)y;
            if ((unsigned)ix < (unsigned)W && (unsigned)iy < (unsigned)H) T *= 1.f - s_occ[fb[iy * W + ix]];
            T += (1.f - T) * 0.01f;
            s_rayT[bi * RAY_NR + k] = (uint8_t)(T * pat * 255.f);
        }
    }
    const uint32_t drift = (uint32_t)(g_t * 2.f), drift2 = (uint32_t)(g_t * 0.7f);
    for (int by2 = 0; by2 < H; by2 += 2) {
        float dy = by2 + 0.5f - s_sunY;
        for (int bx2 = 0; bx2 < W; bx2 += 2) {
            float dx = bx2 + 0.5f - s_sunX;
            float r = sqrtf(dx * dx + dy * dy);
            float kf = r / RAY_STEP - 1.f;
            if (kf < 0.f || kf >= RAY_NR - 1) continue;
            float ang = atan2f(dy, dx);
            if (ang < 0.f) ang += kTau;
            float bf = ang * (RAY_NB / kTau);
            int b0 = (int)bf, k0 = (int)kf;
            float fbb = bf - b0, fk = kf - k0;
            if (b0 >= RAY_NB) b0 -= RAY_NB;
            int b1 = b0 + 1 >= RAY_NB ? 0 : b0 + 1;
            const uint8_t* r0 = s_rayT + b0 * RAY_NR + k0;
            const uint8_t* r1 = s_rayT + b1 * RAY_NR + k0;
            float t0 = r0[0] + (r0[1] - r0[0]) * fk, t1 = r1[0] + (r1[1] - r1[0]) * fk;
            float Tt = (t0 + (t1 - t0) * fbb) * (1.f / 255.f);
            float B = Tt * str * (1.f - r / (RAY_NR * RAY_STEP)) * (0.6f + 0.4f * clampf(by2 / 60.f, 0.f, 1.f));
            if (B < 0.04f) continue;
            for (int y = by2; y < by2 + 2 && y < H; y++)
                for (int x = bx2; x < bx2 + 2; x++) {
                    uint8_t& p = fb[y * W + x];
                    if (B > 0.28f && (hash32((uint32_t)(x + g_pan + drift) * 0x9E3779B1u ^ (uint32_t)(y - g_hor - drift2) * 0x85EBCA77u) & 1023u) < (uint32_t)(B * 2.f)) {
                        p = (uint8_t)(R_SUN + 12); continue;                    // motes drifting in the light
                    }
                    if (sc.prism > 0.2f && B > 0.3f && p < R_SED) {          // the sunken city: the light breaks into colour
                        if (dith(x, y) < sc.prism * 0.7f) { p = (uint8_t)(R_SUN + 2 + ((b0 * 13 / RAY_NB + (int)(r / 30.f)) % 13)); continue; }
                    }
                    int b = (int)(B * 5.5f + dith(x, y) - 0.45f);
                    if (b <= 0) continue;
                    if (p < R_SED) { for (int k = 0; k < b && k < 3; k++) p = s_up[p]; }      // the water lights up
                    else if (!s_emis[p]) p = s_up[p];                                         // things catch the light
                }
        }
    }
}

// ---------------- the floodlights' cone ----------------
static void drawLampCone() {
    float k = g_lampNear;
    if (k < 0.15f) return;
    for (int y = 0; y < H; y++) {
        float v = (y - H * 0.56f) / (H * 0.62f);
        for (int x = 0; x < W; x++) {
            float u = (x - W * 0.5f) / (W * 0.62f);
            float r = sqrtf(u * u + v * v);
            uint8_t& p = fb[y * W + x];
            if (s_emis[p]) continue;
            if (r > 0.62f) {
                int st = (int)((r - 0.62f) * 7.f * k + dith(x, y));
                for (int i = 0; i < st && i < 3; i++) p = s_dn[p];
            } else if (p < R_SED && r < 0.5f && dith(x, y) < (0.5f - r) * 1.6f * k) p = s_up[p];   // the beam in the murk
        }
    }
}

// ---------------- sonar ----------------
static void drawSonar() {
    const Scene& sc = g_scene;
    if (sc.sonarAge > 2.4f) return;
    float rad = sc.sonarAge * 115.f;
    float a = 1.f - sc.sonarAge / 2.4f;
    for (int i = 0; i < 260; i++) {
        float th = i * (kTau / 260.f);
        int x = (int)(W * 0.5f + cosf(th) * rad), y = (int)(H * 0.55f + sinf(th) * rad * 0.62f);
        if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H && dith(x, y) < a) fb[y * W + x] = R_MISC + M_SONAR;
    }
    // contacts: everything alive the ping found, bracketed
    if (sc.sonarAge > 3.8f) return;
    const Critter* cr = fieldCritters();
    for (int i = 0; i < CRIT_N; i++) {
        if (!cr[i].on) continue;
        float sx, sy, f;
        if (!proj(cr[i].x, cr[i].y, cr[i].z, sx, sy, f) || f > sc.sonarRange) continue;
        float hit = f / 115.f * 2.f;
        if (sc.sonarAge < hit * 0.5f) continue;
        if (((int)(sc.sonarAge * 8.f)) & 1) continue;
        int x = (int)sx, y = (int)sy, r = 4;
        uint8_t c = R_MISC + M_SONAR;
        for (int k = 0; k < 3; k++) {
            int px[4] = {x - r, x + r, x - r, x + r}, py[4] = {y - r, y - r, y + r, y + r};
            for (int q = 0; q < 4; q++) {
                int sxp = q & 1 ? -1 : 1, syp = q & 2 ? -1 : 1;
                int xx = px[q] + (k == 1 ? sxp : 0), yy = py[q] + (k == 2 ? syp : 0);
                if ((unsigned)xx < (unsigned)W && (unsigned)yy < (unsigned)H) fb[yy * W + xx] = c;
            }
        }
    }
}

// ---------------- instruments: a 3x5 pixel font ----------------
static const struct { char c; uint16_t m; } GL3[] = {
    {'A', 0x7BED}, {'B', 0x6BAE}, {'C', 0x7927}, {'D', 0x6B6E}, {'E', 0x79A7}, {'F', 0x79A4}, {'G', 0x796F}, {'H', 0x5BED}, {'I', 0x7497}, {'J', 0x126F}, {'K', 0x5BAD}, {'L', 0x4927}, {'M', 0x5FED}, {'N', 0x6B6D}, {'O', 0x7B6F}, {'P', 0x7BE4}, {'Q', 0x7B79}, {'R', 0x6BAD}, {'S', 0x79CF}, {'T', 0x7492}, {'U', 0x5B6F}, {'V', 0x5B6A}, {'W', 0x5BFD}, {'X', 0x5AAD}, {'Y', 0x5A92}, {'Z', 0x72A7}, {'0', 0x7B6F}, {'1', 0x2C97}, {'2', 0x73E7}, {'3', 0x72CF}, {'4', 0x5BC9}, {'5', 0x79CF}, {'6', 0x79EF}, {'7', 0x7292}, {'8', 0x7BEF}, {'9', 0x7BCF}, {'-', 0x01C0}, {'.', 0x0002}, {':', 0x0410}, {'/', 0x12A4}, {'\'', 0x2400}, {'<', 0x1511}, {'>', 0x4454}, {'+', 0x05D0}, {'!', 0x2482}, {'%', 0x52A5}, {'*', 0x2EA8}};
static uint16_t glyph3(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    for (auto& g : GL3) if (g.c == c) return g.m;
    return 0;
}
static int textW(const char* s) { int n = (int)strlen(s); return n ? n * 4 - 1 : 0; }
static void text(int x, int y, const char* s, uint8_t c, bool shadow = true) {
    for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
        int cx = x + (pass == 0 ? 1 : 0), cy = y + (pass == 0 ? 1 : 0);
        uint8_t col = pass == 0 ? (uint8_t)(R_HUD + HU_BLACK) : c;
        for (const char* p = s; *p; p++, cx += 4) {
            uint16_t m = glyph3(*p);
            for (int r = 0; r < 5; r++)
                for (int k = 0; k < 3; k++)
                    if (m & (1u << (14 - (r * 3 + k)))) { int xx = cx + k, yy = cy + r; if ((unsigned)xx < (unsigned)W && (unsigned)yy < (unsigned)H) fb[yy * W + xx] = col; }
        }
    }
}
static void textFade(int x, int y, const char* s, uint8_t c, float a) {
    if (a <= 0.02f) return;
    if (a >= 0.98f) { text(x, y, s, c); return; }
    for (int pass = 0; pass < 2; pass++) {
        int cx = x + (pass == 0 ? 1 : 0), cy = y + (pass == 0 ? 1 : 0);
        uint8_t col = pass == 0 ? (uint8_t)(R_HUD + HU_BLACK) : c;
        for (const char* p = s; *p; p++, cx += 4) {
            uint16_t m = glyph3(*p);
            for (int r = 0; r < 5; r++)
                for (int k = 0; k < 3; k++)
                    if (m & (1u << (14 - (r * 3 + k)))) { int xx = cx + k, yy = cy + r; if ((unsigned)xx < (unsigned)W && (unsigned)yy < (unsigned)H && dith(xx, yy + g_hor) < a) fb[yy * W + xx] = col; }
        }
    }
}
static void hudPut(int x, int y, uint8_t c) { if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) fb[y * W + x] = c; }

static void drawHud() {
    const Scene& sc = g_scene;
    const HudInfo& hd = g_hud;
    char b[32];
    // depth, and how far the seabed is below you
    snprintf(b, sizeof b, "%dM", (int)(sc.depth + 0.5f));
    text(3, 3, b, R_HUD + (sc.depth > 200.f ? HU_AMBER : HU_BTEAL));
    if (sc.altitude < 60.f) { snprintf(b, sizeof b, "FLOOR %dM", (int)(sc.altitude + 0.5f)); text(3, 10, b, R_HUD + (sc.altitude < 3.f ? HU_AMBER : HU_TEAL)); }
    // the compass tape
    {
        int cx = W / 2;
        for (int k = -20; k <= 20; k++) {
            float a = g_yaw + k * 0.06f;
            float deg = a * 57.2958f;
            int di = (int)floorf(deg / 15.f + 0.5f);
            float off = (deg - di * 15.f) / 57.2958f;
            if (fabsf(off) > 0.03f) continue;
            int x = cx + k * 2;
            int dd = ((di * 15) % 360 + 360) % 360;
            const char* lab = dd == 0 ? "E" : dd == 90 ? "S" : dd == 180 ? "W" : dd == 270 ? "N" : nullptr;
            if (lab) text(x - 1, 2, lab, R_HUD + (dd == 270 ? HU_AMBER : HU_BTEAL));
            else { hudPut(x, 5, R_HUD + HU_TEAL); hudPut(x, 6, R_HUD + HU_TEAL); }
        }
        hudPut(cx, 8, R_HUD + HU_WHITE); hudPut(cx - 1, 9, R_HUD + HU_WHITE); hudPut(cx + 1, 9, R_HUD + HU_WHITE);
    }
    // the reticle leans with the turn
    {
        int rx = W / 2 + (int)(helmTurnRate() * 6.f), ry = (int)(H * 0.5f) - (int)(helmPitchRate() * 6.f);
        uint8_t rc = R_HUD + HU_TEAL;
        for (int k = 3; k <= 5; k++) { hudPut(rx - k, ry, rc); hudPut(rx + k, ry, rc); }
        hudPut(rx, ry - 3, rc); hudPut(rx, ry + 3, rc);
        // the pitch ladder: a short bar where level is
        int ly = g_hor;
        if (ly > 14 && ly < H - 14) { for (int k = 10; k < 16; k++) { hudPut(W / 2 - k, ly, R_HUD + HU_DTEAL); hudPut(W / 2 + k, ly, R_HUD + HU_DTEAL); } }
    }
    // the throttle slider, right edge (SpaceMantis's): a detent at cruise
    {
        int x0 = W - 6, y0 = SLIDER_Y0 / 2, y1 = SLIDER_Y1 / 2;
        int ty = y1 - (int)(hd.throttle * (y1 - y0));
        for (int y = y0; y <= y1; y++) { hudPut(x0, y, R_HUD + HU_DTEAL); hudPut(x0 + 2, y, R_HUD + HU_DTEAL); }
        uint8_t fc = hd.throttle > 0.75f ? (uint8_t)(R_HUD + HU_AMBER) : (uint8_t)(R_HUD + HU_TEAL);
        for (int y = ty; y <= y1; y++) hudPut(x0 + 1, y, fc);
        int cy = (y0 + y1) / 2;
        hudPut(x0 - 1, cy, R_HUD + HU_GREY); hudPut(x0 + 3, cy, R_HUD + HU_GREY);
        for (int x = x0 - 2; x <= x0 + 4; x++) { hudPut(x, ty, R_HUD + HU_WHITE); hudPut(x, ty + 1, R_HUD + HU_GREY); }
        snprintf(b, sizeof b, "%d", (int)(sc.speed * 1.944f + 0.5f));       // knots
        text(W - 4 - textW(b), y1 + 4, b, R_HUD + HU_TEAL);
    }
    if (sc.lights) { text(3, H - 8, "*", R_HUD + HU_AMBER); }
    if (hd.scanT > 0.f) {                                                     // the scanner's brackets
        int x = hd.scanX / 2, y = hd.scanY / 2;
        float k = clampf(hd.scanT, 0.f, 1.f);
        int r = 14 - (int)(k * 7.f);
        uint8_t c = R_MISC + M_SCAN;
        if (hd.scanT < 1.f || ((int)(g_t * 6.f) & 1)) {
            for (int i = 0; i < 4; i++) {
                int sxp = (i & 1) ? 1 : -1, syp = (i & 2) ? 1 : -1;
                for (int j = 0; j < 4; j++) { hudPut(x + sxp * r - sxp * j, y + syp * r, c); hudPut(x + sxp * r, y + syp * r - syp * j, c); }
            }
        }
        if (hd.scanT < 1.f) { int w = (int)(k * 24.f); for (int i = 0; i < 24; i++) hudPut(x - 12 + i, y + r + 4, i < w ? c : (uint8_t)(R_HUD + HU_DTEAL)); }
    }
    if (hd.banner && hd.bannerA > 0.02f) {
        int w = textW(hd.banner);
        textFade((W - w) / 2, 20, hd.banner, R_HUD + HU_WHITE, hd.bannerA);
    }
    if (hd.hintA > 0.02f) {
        const char* h1 = "TILT TO STEER  SLIDE TO LOOK";
        const char* h2 = "TAP A CREATURE TO SCAN";
        textFade((W - textW(h1)) / 2, H - 15, h1, R_HUD + HU_TEAL, hd.hintA);
        textFade((W - textW(h2)) / 2, H - 8, h2, R_HUD + HU_TEAL, hd.hintA);
    }
    if (hd.muteIcon && hd.muteA > 0.02f) textFade(W - 4 - textW(hd.muted ? "SOUND OFF" : "SOUND ON"), 3, hd.muted ? "SOUND OFF" : "SOUND ON", R_HUD + HU_AMBER, hd.muteA);
    if (hd.showFps) { snprintf(b, sizeof b, "%d", hd.fps); text(W - 4 - textW(b), 12, b, R_HUD + HU_GREEN); }
}

// ---------------- entry ----------------
void renderInitImpl() {
    if (!zb) zb = (uint8_t*)memFast(W * H);
    buildStepTables();
    buildCaustics();
    buildOcc();
    if (!s_rayT) s_rayT = (uint8_t*)memSlow(RAY_NB * RAY_NR);
    for (int r = 0; r <= 16; r++)
        for (int dy = 0; dy <= r; dy++) {
            float v = (r + 0.3f) * (r + 0.3f) - dy * dy;
            HW[r][dy] = v > 0.f ? (uint8_t)sqrtf(v) : 0;
        }
    memset(fb, 0, sizeof(fb));
    propsInit();
}

void renderFrameImpl(float x, float y, float z, float yaw, float pitch, float t) {
    const Scene& sc = g_scene;
    const BiomeParams& b = biomeAt(sc.distance);
    g_yaw = yaw; g_t = t;
    g_pitch = clampf(pitch, -0.72f, 0.72f);                          // the view tilts less than the hull (pixel art stays upright)
    // a slow swell: the hull rides it near the surface
    float swell = clampf(1.f - sc.depth / 20.f, 0.f, 1.f) * (0.15f + 0.25f * sc.waves);
    g_cx = x; g_cy = y; g_cz = z + sinf(t * 0.9f) * swell * 0.4f;
    g_cY = cosf(yaw); g_sY = sinf(yaw);
    g_hor = (int)lroundf(H * 0.5f + F * tanf(g_pitch) + sinf(t * 0.7f) * swell * 2.f);
    g_pan = (int)lroundf(yaw * F);
    g_surge = b.surge;
    g_vis = b.vis * (1.f - 0.35f * b.murk) * (1.f - 0.3f * sc.storm * clampf(1.f - sc.depth / 30.f, 0.f, 1.f));
    if (g_vis < 12.f) g_vis = 12.f;
    // where the sun is, bent down into the window (Snell): always high, never below ~48 degrees
    s_sunVis = smoothstep(0.0f, 0.12f, sc.sunElev) * clampf(1.f - (sc.cloud - 0.55f) * 2.f, 0.f, 1.f) * (1.f - 0.8f * sc.rain);
    {
        float ce = cosf(fmaxf(sc.sunElev, 0.02f));
        float eW = 1.5707963f - asinf(clampf(ce / 1.33f, -1.f, 1.f));
        float rel = wrapPi(sc.sunAz - yaw);
        float relC = clampf(rel, -1.2f, 1.2f);
        s_sunX = W * 0.5f + tanf(relC) * F;
        s_sunY = g_hor - F * tanf(eW) / fmaxf(0.3f, cosf(relC));
        if (fabsf(rel) > 1.2f) s_sunY -= (fabsf(rel) - 1.2f) * 80.f;     // behind you: the shafts lean away from you
        if (s_sunY > H * 0.1f) s_sunY = H * 0.1f;
    }
    buildPalette();
    drawWater();
    drawSurface();
    drawSeabed();
    drawProps();
    g_alpha = 1.f;
    drawParticles();
    drawShafts();
    drawLampCone();
    drawSonar();
    drawHud();
}

}  // namespace rr

using namespace rr;
void renderInit() { renderInitImpl(); }
void renderFrame(float x, float y, float z, float yaw, float pitch, float t) { renderFrameImpl(x, y, z, yaw, pitch, t); }
bool renderProject(float wx, float wy, float wz, int& px, int& py, float& dist) {
    float sx, sy, f;
    if (!proj(wx, wy, wz, sx, sy, f)) return false;
    px = (int)(sx * 2.f); py = (int)(sy * 2.f);
    float dx = wx - g_cx, dy = wy - g_cy, dz = wz - g_cz;
    dist = sqrtf(dx * dx + dy * dy + dz * dz);
    return px >= -40 && px < 360 && py >= -40 && py < 280;
}
float renderFov() { return FOV; }
const uint8_t* frameBuffer() { return fb; }
const uint16_t* framePalette565() { return pal565; }
const uint8_t* framePaletteRGB() { return palRGB; }
int frameBob() { return 0; }

}
