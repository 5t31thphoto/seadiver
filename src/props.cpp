// What grows, stands and swims: kelp, grass, corals, sponges, rocks, pillars, vents, ruins, wrecks, ice,
// crystals, bones; and every creature and school. All procedural, so each scales smoothly with distance and
// dissolves into the haze through ordered dither (MantisPlanet's re-dreamt things fade in the same way).
#include <math.h>
#include "render_int.h"
#include "field.h"
#include "scene.h"
#include "journey.h"
#include "mem.h"

namespace mantis {
namespace rr {

static float s_t = 0.f;
static bool s_far = false;           // this thing is hazed: draw from the hazed halves of the ramps
static bool s_glowy = false;         // the deep: corals and jellies make their own light

static inline uint32_t H32(uint32_t s, uint32_t k) { return hash32(s + k * 0x9E3779B9u); }
static inline float Hf(uint32_t s, uint32_t k) { return (H32(s, k) >> 8) * (1.f / 16777216.f); }

static void fillTri(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t c) {
    if (y1 < y0) { float t; t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 < y0) { float t; t = x0; x0 = x2; x2 = t; t = y0; y0 = y2; y2 = t; }
    if (y2 < y1) { float t; t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    int ya = (int)ceilf(y0), yb = (int)ceilf(y2);
    if (ya < 0) ya = 0;
    if (yb > H) yb = H;
    for (int y = ya; y < yb; y++) {
        float yc = y + 0.5f;
        float xa = y2 != y0 ? x0 + (x2 - x0) * (yc - y0) / (y2 - y0) : x0;
        float xb = yc < y1 ? (y1 != y0 ? x0 + (x1 - x0) * (yc - y0) / (y1 - y0) : x1) : (y2 != y1 ? x1 + (x2 - x1) * (yc - y1) / (y2 - y1) : x2);
        if (xa > xb) { float t = xa; xa = xb; xb = t; }
        hspan((int)ceilf(xa - 0.5f), (int)floorf(xb - 0.5f), y, c);
    }
}

// ================================ the seabed's furniture ================================
static void drawKelp(float sx, float by, float s, float hgt, uint32_t seed) {
    float sw = g_surge * (0.6f + 0.4f * Hf(seed, 1));
    float ph = Hf(seed, 2) * kTau;
    int n = clampi((int)(hgt * s / 3.f), 4, 48);
    float wp = fmaxf(1.f, 0.06f * s);
    float px = sx, py = by;
    for (int k = 1; k <= n; k++) {
        float u = (float)k / n, z = u * hgt;
        float sway = (sinf(s_t * 0.7f + u * 2.4f + ph) * (0.25f + u * 1.3f) + u * 0.6f) * sw;
        float x = sx + sway * s, y = by - z * s;
        thickLine(px, py, x, y, wp, floraI(2 + (u > 0.7f), s_far));
        if (s <= 4.5f && (k & 1)) put((int)(x + ((k & 2) ? 1 : -1)), (int)y, floraI(4, s_far));   // far off: a ragged stalk
        // blades: alternate sides every ~0.7 m, each with a gas bladder at its root
        uint32_t hb = H32(seed, 10 + k);
        if (s > 4.5f && (hb & 1) == (uint32_t)(k & 1)) {
            float side = (k & 1) ? 1.f : -1.f;
            float bl = (0.7f + 0.6f * Hf(hb, 1)) * s;
            float flut = sinf(s_t * 1.3f + u * 3.f + ph) * 0.25f;
            float ang = -0.55f + flut + (side > 0 ? 0.f : 3.14159f) * 1.f;   // angled up and out
            float ca = side > 0 ? cosf(-0.55f + flut) : -cosf(-0.55f - flut), sa = sinf(-0.55f + flut * side);
            (void)ang;
            float tx = x + ca * bl, ty = y + sa * bl;
            float w = fmaxf(0.6f, 0.13f * s);
            float mx = x + ca * bl * 0.45f, my = y + sa * bl * 0.45f;
            float nx = -sa * w, ny = ca * w;
            uint8_t lit = floraI(4 + (int)(u * 2.f) + (side > 0), s_far), dk = floraI(2 + (int)(u * 2.f), s_far);
            fillTri(x, y, mx + nx, my + ny, tx, ty, lit);
            fillTri(x, y, mx - nx, my - ny, tx, ty, dk);
            if (s > 7.f) line((int)x, (int)y, (int)tx, (int)ty, floraI(6, s_far));
            if (s > 5.f) splat((int)(x + ca * 0.1f * s), (int)(y + sa * 0.1f * s), (int)fmaxf(1.f, 0.06f * s), floraI(6, s_far));   // the gas bladder
        }
        px = x; py = y;
    }
    if (hgt > 6.f && s > 1.5f) {                                       // where it reaches the light, a floating canopy
        for (int i = 0; i < 6; i++) {
            float a = Hf(seed, 40 + i) * 2.f - 1.f;
            thickLine(px, py, px + a * s * 1.6f, py + fabsf(a) * s * 0.25f, fmaxf(1.f, 0.2f * s), floraI(4 + (i & 1) * 2, s_far));
        }
    }
}

static void drawGrass(float sx, float by, float s, float hgt, uint32_t seed) {
    int n = s < 5.f ? 2 : 4 + (int)(Hf(seed, 0) * 4.f);
    float sw = g_surge * 0.6f + 0.15f;
    for (int i = 0; i < n; i++) {
        float ox = (Hf(seed, i * 3 + 1) - 0.5f) * 0.5f * s;
        float h = hgt * (0.55f + 0.45f * Hf(seed, i * 3 + 2)) * s;
        float lean = (sinf(s_t * 1.1f + Hf(seed, i) * 6.f) * sw + (Hf(seed, i * 3 + 3) - 0.5f) * 0.4f) * h;
        float mx = sx + ox + lean * 0.35f, my = by - h * 0.55f;
        float tx = sx + ox + lean, ty = by - h;
        line((int)(sx + ox), (int)by, (int)mx, (int)my, floraI(2, s_far));
        line((int)mx, (int)my, (int)tx, (int)ty, floraI(4 + (i & 1), s_far));
        if (s > 14.f) line((int)(sx + ox + 1), (int)by, (int)(mx + 1), (int)my, floraI(1, s_far));
    }
}

static void drawCoral(float sx, float by, float s, float a, uint8_t var, uint32_t seed) {
    int hue = (int)(H32(seed, 7) & 3);
    bool glow = (var & 8) != 0;
    auto C = [&](int sh) -> uint8_t { return glow ? glowI(hue, sh) : coralI(hue, sh - (s_far ? 1 : 0)); };
    float R = a * s;
    switch (var & 7) {
        case 0: {   // branching: a few generations of forking arms
            struct Br { float x, y, ang, len, w; int gen; };
            Br st[24]; int n = 0;
            int trunks = 2 + (int)(Hf(seed, 1) * 3.f);
            for (int i = 0; i < trunks && n < 24; i++) st[n++] = {sx + (Hf(seed, 2 + i) - 0.5f) * R * 0.3f, by, -1.5708f + (Hf(seed, 9 + i) - 0.5f) * 1.2f, R * 0.45f, fmaxf(1.f, R * 0.09f), 0};
            int guard = 0;
            while (n > 0 && guard++ < 60) {
                Br b = st[--n];
                float ex = b.x + cosf(b.ang) * b.len, ey = b.y + sinf(b.ang) * b.len;
                thickLine(b.x, b.y, ex, ey, b.w, C(1 + (b.gen > 0)));
                if (b.gen >= 2 || b.len < 1.5f) { splat((int)ex, (int)ey, (int)(b.w * 0.6f), C(3)); continue; }
                for (int k = 0; k < 2 && n < 23; k++) {
                    float da = (k ? 0.5f : -0.5f) + (Hf(seed, 30 + guard * 2 + k) - 0.5f) * 0.4f;
                    st[n++] = {ex, ey, b.ang + da, b.len * 0.68f, fmaxf(1.f, b.w * 0.72f), b.gen + 1};
                }
            }
            break;
        }
        case 1: {   // a brain dome, grooved
            float rx = R * 0.62f, ry = R * 0.48f;
            for (int y = (int)(by - ry); y <= (int)by; y++) {
                float v = (by - y) / ry;
                if (v > 1.f) continue;
                float hw = rx * sqrtf(1.f - v * v);
                for (int x = (int)(sx - hw); x <= (int)(sx + hw); x++) {
                    float u = (x - sx) / rx;
                    float gro = fsin((u * 7.f + fsin(v * 9.f + Hf(seed, 3) * 6.f) * 0.7f) * 2.2f);
                    int sh = v > 0.6f ? 3 : v > 0.25f ? 2 : 1;
                    if (gro > 0.7f && R > 6.f) sh--;
                    put(x, y, C(sh));
                }
            }
            break;
        }
        case 2: {   // a table: a stalk and a broad plate
            float top = by - R * 0.55f;
            thickLine(sx, by, sx, top, fmaxf(1.f, R * 0.12f), C(1));
            ellipse(sx, top, R * 0.85f, fmaxf(1.f, R * 0.14f), C(1));
            ellipse(sx, top - R * 0.04f, R * 0.8f, fmaxf(1.f, R * 0.08f), C(2));
            hspan((int)(sx - R * 0.75f), (int)(sx + R * 0.75f), (int)(top - R * 0.1f), C(3));
            break;
        }
        case 3: {   // a sea fan: a lattice spread on the current
            int rays = clampi((int)(R / 2.f), 5, 14);
            float sway = sinf(s_t * 0.6f + Hf(seed, 4) * 6.f) * g_surge * 0.15f;
            for (int i = 0; i < rays; i++) {
                float ang = -1.5708f + ((float)i / (rays - 1) - 0.5f) * 2.f + sway;
                float L = R * (0.75f + 0.25f * Hf(seed, 50 + i));
                line((int)sx, (int)by, (int)(sx + cosf(ang) * L), (int)(by + sinf(ang) * L * 1.1f), C(1 + (i & 1)));
            }
            for (int k = 1; k <= 3 && R > 8.f; k++) {
                float rr = R * k / 4.f;
                for (int i = 0; i < 20; i++) { float ang = -1.5708f + (i / 19.f - 0.5f) * 2.f + sway; put((int)(sx + cosf(ang) * rr), (int)(by + sinf(ang) * rr * 1.1f), C(2)); }
            }
            break;
        }
        case 4: {   // tube sponges
            int n = 3 + (int)(Hf(seed, 5) * 3.f);
            for (int i = 0; i < n; i++) {
                float ox = (Hf(seed, 60 + i) - 0.5f) * R * 0.7f, h = R * (0.5f + 0.5f * Hf(seed, 70 + i)), w = fmaxf(1.f, R * 0.11f);
                for (int y = (int)(by - h); y <= (int)by; y++) hspan((int)(sx + ox - w), (int)(sx + ox + w), y, C(y < by - h * 0.7f ? 2 : 1));
                hspan((int)(sx + ox - w), (int)(sx + ox + w), (int)(by - h), C(3));
                if (w > 2.f) hspan((int)(sx + ox - w + 1), (int)(sx + ox + w - 1), (int)(by - h) + 1, C(0));
            }
            break;
        }
        default: {  // pillar coral: upright fingers
            int n = 4 + (int)(Hf(seed, 6) * 4.f);
            for (int i = 0; i < n; i++) {
                float ox = (Hf(seed, 80 + i) - 0.5f) * R * 0.8f, h = R * (0.4f + 0.6f * Hf(seed, 90 + i)), w = fmaxf(1.f, R * 0.07f);
                thickLine(sx + ox, by, sx + ox, by - h, w * 2.f, C(1 + (i & 1)));
                splat((int)(sx + ox), (int)(by - h), (int)w, C(3));
            }
            break;
        }
    }
}

static void drawAnemone(float sx, float by, float s, float a, uint32_t seed) {
    int hue = (int)(H32(seed, 7) & 3);
    float R = a * s;
    ellipse(sx, by - R * 0.2f, R * 0.35f, R * 0.25f, coralI(hue, 1));
    int n = clampi((int)(R * 1.2f), 6, 26);
    for (int i = 0; i < n; i++) {
        float ang = -3.14159f + (i + 0.5f) / n * 3.14159f;
        float L = R * (0.55f + 0.35f * Hf(seed, i));
        float sw = sinf(s_t * 1.4f + i * 0.5f) * 0.25f * (g_surge + 0.3f);
        float ex = sx + cosf(ang + sw) * L, ey = by - R * 0.35f + sinf(ang + sw) * L * 0.9f;
        line((int)sx, (int)(by - R * 0.35f), (int)ex, (int)ey, s_glowy ? glowI(hue, 1) : coralI(hue, 2));
        put((int)ex, (int)ey, s_glowy ? glowI(hue, 3) : coralI(hue, 3));
    }
}

static void drawSponge(float sx, float by, float s, float a, uint8_t var, uint32_t seed) {
    float R = a * s;
    if (var == 1) {                                                   // a glass sponge: a pale lattice vase
        float h = R * 1.3f;
        for (int k = 0; k <= 8; k++) {
            float u = k / 8.f;
            float w = R * (0.18f + 0.22f * u);
            float y = by - h * u;
            put((int)(sx - w), (int)y, R_FAUNA + FA_LGREY); put((int)(sx + w), (int)y, R_FAUNA + FA_LGREY);
            if (R > 8.f) for (int x = (int)(sx - w); x <= (int)(sx + w); x += 2) put(x, (int)y, R_FAUNA + FA_GREY);
        }
        line((int)(sx - R * 0.18f), (int)by, (int)(sx - R * 0.4f), (int)(by - h), R_FAUNA + FA_LGREY);
        line((int)(sx + R * 0.18f), (int)by, (int)(sx + R * 0.4f), (int)(by - h), R_FAUNA + FA_LGREY);
        hspan((int)(sx - R * 0.4f), (int)(sx + R * 0.4f), (int)(by - h), R_FAUNA + FA_WHITE);
        return;
    }
    int hue = 1 + (int)(H32(seed, 3) & 1);                            // a barrel: orange or violet
    float h = R * 0.9f;
    for (int y = (int)(by - h); y <= (int)by; y++) {
        float u = (by - y) / h;
        float w = R * (0.32f + 0.18f * u);
        hspan((int)(sx - w), (int)(sx + w), y, coralI(hue, (y & 3) == 0 ? 0 : 1));
        put((int)(sx + w * 0.4f), y, coralI(hue, 2));
    }
    ellipse(sx, by - h, R * 0.5f, fmaxf(1.f, R * 0.12f), coralI(hue, 2));
    ellipse(sx, by - h, R * 0.38f, fmaxf(1.f, R * 0.08f), coralI(hue, 0));
}

// faceted rock: per-pixel nearest of a few hashed points, each facet tilted toward or away from the light above
static void rockBody(float cx, float cy, float Rx, float Ry, uint32_t seed, bool stone) {
    float fpx[7], fpy[7], fsh[7];
    for (int i = 0; i < 7; i++) {
        fpx[i] = Hf(seed, i * 3) * 2.f - 1.f; fpy[i] = Hf(seed, i * 3 + 1) * 2.f - 1.f;
        fsh[i] = 2.f + Hf(seed, i * 3 + 2) * 2.5f - fpy[i] * 1.8f;
    }
    int y0 = (int)(cy - Ry), y1 = (int)(cy + Ry);
    for (int y = y0; y <= y1; y++) {
        float v = (y + 0.5f - cy) / Ry;
        if (v * v >= 1.f) continue;
        float lump = 1.f + 0.08f * fsin(v * 7.f + Hf(seed, 40) * 6.f);
        float hw = Rx * sqrtf(1.f - v * v) * lump;
        for (int x = (int)(cx - hw); x <= (int)(cx + hw); x++) {
            float u = (x + 0.5f - cx) / Rx;
            int best = 0; float bd = 1e9f;
            for (int i = 0; i < 7; i++) { float dx = u - fpx[i], dy = v - fpy[i]; float d = dx * dx + dy * dy; if (d < bd) { bd = d; best = i; } }
            float sh = fsh[best];
            if (v < -0.6f) sh += 1.5f;                                  // light from above
            if (u * u + v * v > 0.8f) sh -= 1.f;
            int b = band(sh, x, y);
            put(x, y, stone ? stoneI(b, s_far) : (uint8_t)(R_ROCK + (s_far ? 16 : 0) + clampi(b + 1, 0, 7)));
        }
    }
}

static void drawRock(float sx, float by, float s, float a, uint32_t seed) {
    float R = a * s;
    rockBody(sx, by - R * 0.55f, R * (0.9f + 0.3f * Hf(seed, 99)), R * 0.7f, seed, true);
    if (R > 6.f && biomeAt(g_scene.distance).coral_ > 0.3f)        // reef rock: coral and sponge crusting it
        for (int i = 0; i < 6; i++) splat((int)(sx + (Hf(seed, 120 + i) - 0.5f) * R * 1.2f), (int)(by - R * (0.5f + 0.6f * Hf(seed, 130 + i))), (int)(R * 0.08f), coralI((int)(H32(seed, 140 + i) & 3), 2));
}

static void drawPillar(float sx, float by, float s, float a, float b, uint32_t seed) {
    float h = b * s, R = a * s;
    float top = by - h;
    int ya = (int)fmaxf(top, -1.f), yb = (int)by;
    for (int y = ya; y <= yb; y++) {
        float u = (by - y) / h;
        float w = R * (1.f - 0.4f * u + 0.12f * fsin(u * 11.f + Hf(seed, 1) * 6.f) + 0.08f * fsin(u * 29.f));
        float bands = fsin(u * 26.f + Hf(seed, 2) * 6.f);
        for (int x = (int)(sx - w); x <= (int)(sx + w); x++) {
            float q = (x - sx) / w;
            float sh = 3.5f - fabsf(q) * 2.5f + q * 0.8f + bands * 0.6f + (u > 0.96f ? 2.f : 0.f);
            put(x, y, stoneI(band(sh, x, y), s_far));
        }
    }
    if (biomeAt(g_scene.distance).glowAmt > 0.3f)                  // glowing growths in the deep
        for (int i = 0; i < 5; i++) put((int)(sx + (Hf(seed, 50 + i) - 0.5f) * R), (int)(by - h * Hf(seed, 60 + i)), glowI(1, 2 + (i & 1)));
}

static void drawBasalt(float sx, float by, float s, float a, float b, uint32_t seed) {
    int n = 3 + (int)(Hf(seed, 0) * 4.f);
    float w = a * s / n * 1.4f;
    for (int i = 0; i < n; i++) {
        float ox = (i - (n - 1) * 0.5f) * w * 1.02f;
        float h = b * s * (0.45f + 0.55f * Hf(seed, 10 + i));
        float x0 = sx + ox - w * 0.5f, x1 = sx + ox + w * 0.5f;
        for (int y = (int)(by - h); y <= (int)by; y++) {
            hspan((int)x0, (int)(sx + ox), y, (uint8_t)(R_ROCK + (s_far ? 16 : 0) + 1));
            hspan((int)(sx + ox) + 1, (int)x1, y, (uint8_t)(R_ROCK + (s_far ? 16 : 0) + 3));
        }
        ellipse(sx + ox, by - h, w * 0.5f, fmaxf(1.f, w * 0.2f), (uint8_t)(R_ROCK + (s_far ? 16 : 0) + 5));
    }
}

static void drawVent(float sx, float by, float s, float a, uint8_t var, uint32_t seed) {
    float h = a * s, R = 0.55f * s;
    for (int y = (int)(by - h); y <= (int)by; y++) {
        float u = (by - y) / h;
        float w = R * (1.f - 0.45f * u) * (1.f + 0.25f * fsin(u * 17.f + Hf(seed, 1) * 6.f));
        for (int x = (int)(sx - w); x <= (int)(sx + w); x++) {
            float q = (x - sx) / fmaxf(w, 0.5f);
            int sh = (int)(1.5f + q * 0.8f + fsin(u * 40.f) * 0.6f);
            put(x, y, var ? (uint8_t)(R_ROCK + clampi(sh, 0, 3)) : stoneI(sh - 1, s_far));
        }
    }
    float topY = by - h;
    // heat: the throat glows, and seams of fire run down the chimney
    { float ga = g_alpha; g_alpha = fmaxf(ga, 0.7f);
      splat((int)sx, (int)topY, (int)fmaxf(1.f, R * 0.5f), var ? hotI(6) : hotI(3));
      put((int)sx, (int)topY, hotI(7));
      for (int k = 0; k < 4; k++) { float u = Hf(seed, 70 + k); put((int)(sx + (Hf(seed, 80 + k) - 0.5f) * R), (int)(by - h * u), hotI(2 + (int)(2.f * (1.f - u)))); }
      g_alpha = ga; }
    // the plume: a black smoker pours black water; a lava vent breathes a glowing green ghost
    float er = g_scene.eruption;
    int puffs = var ? 12 : 9;
    for (int k = 0; k < puffs; k++) {
        float ph = fmodf(s_t * (var ? 0.35f : 0.5f) + k / (float)puffs + Hf(seed, 20 + k) * 0.1f, 1.f);
        float rise = ph * (var ? 14.f + er * 10.f : 9.f) * s;
        float r = (0.35f + ph * (var ? 2.4f : 1.5f)) * s;
        float drift = (sinf(s_t * 0.5f + k) * 0.5f + ph * 1.2f) * s;
        float alpha = (1.f - ph) * (var ? 0.9f : 1.f);
        float ga = g_alpha;
        g_alpha = ga * alpha;
        uint8_t c = var ? glowI(2, (int)((1.f - ph) * 3.f + er)) : (uint8_t)(R_FAUNA + (ph < 0.25f ? FA_DGREY : FA_GREY));
        splat((int)(sx + drift), (int)(topY - rise), (int)fminf(16.f, r), c);
        g_alpha = ga;
    }
    // tube worms around the foot
    if (s > 6.f) for (int i = 0; i < 6; i++) {
        float ox = (Hf(seed, 40 + i) - 0.5f) * 2.4f * s, hh = (0.25f + 0.3f * Hf(seed, 50 + i)) * s;
        line((int)(sx + ox), (int)by, (int)(sx + ox), (int)(by - hh), R_FAUNA + FA_WHITE);
        put((int)(sx + ox), (int)(by - hh), R_FAUNA + FA_DRED); put((int)(sx + ox) + 1, (int)(by - hh), R_FAUNA + FA_DRED);
    }
}

static void column(float x, float by, float s, float h, float w, uint32_t seed, bool broken) {
    float top = by - h;
    for (int xx = (int)(x - w); xx <= (int)(x + w); xx++)
        for (int y = (int)top + (broken ? (int)(Hf(seed, xx & 31) * w * 0.9f) : 0); y <= (int)by; y++) {
            float q = (xx - x) / w;
            float sh = 3.5f - fabsf(q) * 2.f + q * 0.7f;
            if (w > 3.f && ((xx - (int)(x - w)) % 3) == 0) sh -= 1.f;    // fluting
            put(xx, y, stoneI(band(sh, xx, y), s_far));
        }
    if (!broken) {
        hspan((int)(x - w * 1.5f), (int)(x + w * 1.5f), (int)top, stoneI(6, s_far));
        for (int k = 1; k < fmaxf(2.f, s * 0.25f); k++) hspan((int)(x - w * 1.4f), (int)(x + w * 1.4f), (int)top + k, stoneI(4, s_far));
    }
    hspan((int)(x - w * 1.4f), (int)(x + w * 1.4f), (int)by, stoneI(2, s_far));
}

static void drawRuin(float sx, float by, float s, float a, float b, uint8_t var, uint32_t seed) {
    float h = a * s, w = b * s;
    switch (var) {
        case 0: column(sx, by, s, h, 0.45f * s, seed, false); break;
        case 1: column(sx, by, s, h * 0.45f, 0.45f * s, seed, true);
                ellipse(sx + 1.4f * s, by - 0.35f * s, 0.5f * s, 0.35f * s, stoneI(3, s_far)); break;
        case 2: column(sx - w * 0.5f, by, s, h, 0.35f * s, seed, false); column(sx + w * 0.5f, by, s, h, 0.35f * s, seed + 1, false);
                for (int k = 0; k < (int)fmaxf(2.f, 0.6f * s); k++) hspan((int)(sx - w * 0.7f), (int)(sx + w * 0.7f), (int)(by - h - k), stoneI(k == 0 ? 6 : 3, s_far));
                break;
        case 3: {   // a wall with windows
            for (int y = (int)(by - h); y <= (int)by; y++)
                for (int x = (int)(sx - w * 0.5f); x <= (int)(sx + w * 0.5f); x++) {
                    float u = (x - sx) / w + 0.5f, v = (by - y) / h;
                    bool win = fmodf(u * 4.f, 1.f) > 0.35f && fmodf(u * 4.f, 1.f) < 0.65f && v > 0.35f && v < 0.75f;
                    bool brick = ((y - (int)by) % (int)fmaxf(2.f, s * 0.5f)) == 0;
                    int topEdge = (int)(Hf(seed, (int)(u * 10.f)) * h * 0.15f);
                    if (by - y > h - topEdge) continue;
                    put(x, y, win ? (uint8_t)(R_WATER + 1) : stoneI(brick ? 2 : 3 + (u > 0.5f), s_far));
                }
            break;
        }
        case 4: {   // a statue on a plinth
            float ph = h * 0.25f;
            for (int y = (int)(by - ph); y <= (int)by; y++) hspan((int)(sx - 0.8f * s), (int)(sx + 0.8f * s), y, stoneI(2 + (y == (int)(by - ph)) * 3, s_far));
            float bodyB = by - ph, bodyT = by - h * 0.82f;
            for (int y = (int)bodyT; y <= (int)bodyB; y++) { float u = (bodyB - y) / (bodyB - bodyT); float ww = s * (0.45f - 0.12f * u + 0.12f * (u < 0.3f)); hspan((int)(sx - ww), (int)(sx + ww), y, stoneI(3 + (int)(u * 2.f), s_far)); }
            splat((int)sx, (int)(by - h * 0.9f), (int)(0.25f * s), stoneI(5, s_far));
            thickLine(sx + 0.4f * s, bodyT + 0.3f * s, sx + 0.9f * s, by - h * 1.05f, fmaxf(1.f, 0.12f * s), stoneI(4, s_far));   // a raised arm, a trident
            line((int)(sx + 0.9f * s), (int)(by - h * 1.05f), (int)(sx + 0.9f * s), (int)(by - h * 1.15f), stoneI(6, s_far));
            break;
        }
        default: {  // a temple front: steps, a colonnade, a pediment
            int cols = 5;
            float colH = h * 0.7f, stepH = h * 0.12f;
            for (int k = 0; k < 3; k++) { float yy = by - k * stepH / 3.f; float ww = w * (0.6f - k * 0.03f); for (int y = (int)(yy - stepH / 3.f); y <= (int)yy; y++) hspan((int)(sx - ww), (int)(sx + ww), y, stoneI(3 + k, s_far)); }
            for (int y = (int)(by - stepH - colH); y <= (int)(by - stepH); y++) hspan((int)(sx - w * 0.5f), (int)(sx + w * 0.5f), y, (uint8_t)(R_WATER + 1));
            for (int i = 0; i < cols; i++) column(sx - w * 0.45f + i * (w * 0.9f / (cols - 1)), by - stepH, s, colH, 0.35f * s, seed + i, i == 3 && Hf(seed, 5) < 0.5f);
            float pb = by - stepH - colH;
            fillTri(sx - w * 0.55f, pb, sx + w * 0.55f, pb, sx, pb - h * 0.2f, stoneI(3, s_far));
            hspan((int)(sx - w * 0.55f), (int)(sx + w * 0.55f), (int)pb, stoneI(6, s_far));
            break;
        }
    }
    if (biomeAt(g_scene.distance).coral_ > 0.2f && s > 4.f)          // coral has taken the stone
        for (int i = 0; i < 8; i++) splat((int)(sx + (Hf(seed, 200 + i) - 0.5f) * fmaxf(w, s)), (int)(by - h * Hf(seed, 210 + i)), (int)(s * 0.12f), coralI((int)(H32(seed, 220 + i) & 3), 2));
}

static void drawMonolith(float sx, float by, float s, float a, float b, uint32_t seed) {
    float h = a * s, w = b * s * 0.5f;
    for (int y = (int)(by - h); y <= (int)by; y++) {
        float u = (by - y) / h;
        float ww = w * (1.f - 0.15f * u);
        hspan((int)(sx - ww), (int)sx, y, stoneI(2, s_far));
        hspan((int)sx + 1, (int)(sx + ww), y, stoneI(3, s_far));
    }
    float pulse = 0.5f + 0.5f * sinf(s_t * 1.3f + Hf(seed, 1) * 6.f);
    int rows = clampi((int)(h / fmaxf(3.f, s * 0.6f)), 2, 8);
    for (int r = 0; r < rows; r++) {
        uint32_t g = H32(seed, r);
        float yy = by - h * (0.2f + 0.65f * r / rows);
        for (int k = 0; k < 3; k++) if ((g >> k) & 1) { int x = (int)(sx - w * 0.5f + k * w * 0.5f); put(x, (int)yy, glowI(0, 2 + (pulse > 0.5f))); if (s > 6.f) put(x, (int)yy + 1, glowI(0, 1)); }
    }
}

static void drawWreck(float sx, float by, float s, float a, float b, uint8_t var, uint32_t seed) {
    float h = a * s, w = b * s;
    switch (var) {
        case 0: {   // a girder tower
            float x0 = sx - w * 0.25f, x1 = sx + w * 0.25f;
            int seg = clampi((int)(a / 1.5f), 2, 8);
            float lean = (Hf(seed, 1) - 0.5f) * w * 0.4f;
            for (int k = 0; k <= seg; k++) {
                float u = (float)k / seg, y = by - h * u, l = lean * u;
                line((int)(x0 + l), (int)y, (int)(x1 + l), (int)y, stoneI(3, s_far));
                if (k < seg) {
                    float u2 = (float)(k + 1) / seg, y2 = by - h * u2, l2 = lean * u2;
                    line((int)(x0 + l), (int)y, (int)(x1 + l2), (int)y2, stoneI(2, s_far));
                    thickLine(x0 + l, y, x0 + l2, y2, fmaxf(1.f, 0.15f * s), stoneI(4, s_far));
                    thickLine(x1 + l, y, x1 + l2, y2, fmaxf(1.f, 0.15f * s), stoneI(3, s_far));
                }
            }
            break;
        }
        case 1: {   // a broken hull, portholes in a row
            float hh = h * 0.6f;
            for (int y = (int)(by - hh); y <= (int)by; y++) {
                float v = (by - y) / hh;
                float ww = w * 0.5f * sqrtf(fmaxf(0.f, 1.f - (1.f - v) * (1.f - v) * 0.6f));
                int cut = (int)(Hf(seed, (int)(v * 8.f)) * w * 0.15f);
                for (int x = (int)(sx - ww); x <= (int)(sx + ww) - cut; x++) {
                    float q = (x - sx) / ww;
                    int sh = (int)(2.5f + v * 2.f - fabsf(q));
                    if (((x + y) & 7) == 0) sh--;                                // rust bloom
                    put(x, y, stoneI(sh, s_far));
                }
            }
            for (int i = 0; i < 5; i++) {
                float px = sx - w * 0.35f + i * w * 0.15f, py = by - hh * 0.6f;
                splat((int)px, (int)py, (int)fmaxf(1.f, 0.18f * s), R_WATER + 1);
                if (g_lampNear > 0.3f && s > 5.f) put((int)px, (int)py, R_SUN + 8);
            }
            break;
        }
        case 2: {   // a tank on its side
            float r = h * 0.25f;
            ellipse(sx, by - r, w * 0.5f, r, stoneI(2, s_far));
            ellipse(sx, by - r * 1.25f, w * 0.48f, r * 0.55f, stoneI(4, s_far));
            ellipse(sx - w * 0.5f, by - r, r * 0.4f, r, stoneI(1, s_far));
            break;
        }
        default: {  // a crane: mast, jib, a hanging chain
            thickLine(sx, by, sx, by - h, fmaxf(1.f, 0.3f * s), stoneI(3, s_far));
            float jx = sx + w * 0.6f, jy = by - h * 0.85f;
            thickLine(sx, by - h, jx, jy, fmaxf(1.f, 0.2f * s), stoneI(4, s_far));
            float sway = sinf(s_t * 0.5f) * s * 0.2f;
            for (int k = 0; k < 10; k++) put((int)(jx + sway * k / 10.f), (int)(jy + k * h * 0.05f), (k & 1) ? stoneI(2, s_far) : stoneI(5, s_far));
            break;
        }
    }
}

static void drawIce(float sx, float by, float s, float a, float b, uint32_t seed) {
    int n = 2 + (int)(Hf(seed, 0) * 3.f);
    for (int i = 0; i < n; i++) {
        float ox = (Hf(seed, 10 + i) - 0.5f) * b * s * 1.6f;
        float h = a * s * (0.5f + 0.5f * Hf(seed, 20 + i)), w = b * s * (0.4f + 0.3f * Hf(seed, 30 + i));
        fillTri(sx + ox - w, by, sx + ox, by, sx + ox + (Hf(seed, 40 + i) - 0.5f) * w * 0.6f, by - h, stoneI(5, s_far));
        fillTri(sx + ox, by, sx + ox + w, by, sx + ox + (Hf(seed, 40 + i) - 0.5f) * w * 0.6f, by - h, stoneI(3, s_far));
        line((int)(sx + ox), (int)by, (int)(sx + ox + (Hf(seed, 40 + i) - 0.5f) * w * 0.6f), (int)(by - h), R_FAUNA + FA_WHITE);
    }
}

static void drawCrystal(float sx, float by, float s, float a, uint32_t seed) {
    int n = 3 + (int)(Hf(seed, 0) * 5.f);
    float pulse = 0.5f + 0.5f * sinf(s_t * 0.9f + Hf(seed, 1) * 6.f);
    for (int i = 0; i < n; i++) {
        float ang = -1.5708f + (Hf(seed, 10 + i) - 0.5f) * 1.5f;
        float L = a * s * (0.4f + 0.6f * Hf(seed, 20 + i)), w = fmaxf(1.f, L * 0.16f);
        float bx = sx + (Hf(seed, 30 + i) - 0.5f) * a * s * 0.4f;
        float tx = bx + cosf(ang) * L, ty = by + sinf(ang) * L;
        float nx = -sinf(ang) * w, ny = cosf(ang) * w;
        float kx = bx + cosf(ang) * L * 0.8f, ky = by + sinf(ang) * L * 0.8f;
        int hue = (int)(H32(seed, 40 + i) & 3);
        fillTri(bx - nx, by - ny, bx, by, kx - nx, ky - ny, coralI(hue, 1));
        fillTri(bx - nx, by - ny, kx, ky, kx - nx, ky - ny, coralI(hue, 1));
        fillTri(bx, by, bx + nx, by + ny, kx, ky, coralI(hue, 2));
        fillTri(bx + nx, by + ny, kx + nx, ky + ny, kx, ky, coralI(hue, 2));
        fillTri(kx - nx, ky - ny, kx + nx, ky + ny, tx, ty, coralI(hue, 3));
        line((int)bx, (int)by, (int)kx, (int)ky, glowI(0, 2 + (pulse > 0.6f)));     // the light inside it
        put((int)tx, (int)ty, glowI(0, 3));
    }
}

static void drawDeadTree(float sx, float by, float s, float a, uint32_t seed) {
    struct Br { float x, y, ang, len, w; int gen; };
    Br st[20]; int n = 0;
    st[n++] = {sx, by, -1.5708f + (Hf(seed, 1) - 0.5f) * 0.3f, a * s * 0.5f, fmaxf(1.f, 0.35f * s), 0};
    int guard = 0;
    while (n > 0 && guard++ < 40) {
        Br b = st[--n];
        float ex = b.x + cosf(b.ang) * b.len, ey = b.y + sinf(b.ang) * b.len;
        thickLine(b.x, b.y, ex, ey, b.w, (uint8_t)(R_FAUNA + (b.gen ? FA_DGREY : FA_BLACK)));
        if (b.gen >= 3) continue;
        for (int k = 0; k < 2 && n < 19; k++) st[n++] = {ex, ey, b.ang + (k ? 0.6f : -0.55f) + (Hf(seed, guard * 2 + k) - 0.5f) * 0.5f, b.len * 0.62f, fmaxf(1.f, b.w * 0.6f), b.gen + 1};
    }
}

static void drawBones(float sx, float by, float s, float a, uint32_t seed) {
    float L = a * s;
    line((int)(sx - L * 0.5f), (int)by, (int)(sx + L * 0.5f), (int)by, R_FAUNA + FA_LGREY);   // the spine
    int ribs = 8;
    for (int i = 0; i < ribs; i++) {
        float x = sx - L * 0.4f + i * L * 0.8f / (ribs - 1);
        float rh = L * 0.22f * sinf(3.14159f * (i + 0.5f) / ribs);
        float px = x, py = by;
        for (int k = 1; k <= 8; k++) {
            float u = k / 8.f;
            float xx = x + sinf(u * 3.14159f) * L * 0.05f, yy = by - sinf(u * 2.6f) * rh;
            line((int)px, (int)py, (int)xx, (int)yy, R_FAUNA + (k < 4 ? FA_WHITE : FA_LGREY));
            px = xx; py = yy;
        }
    }
    splat((int)(sx + L * 0.55f), (int)(by - L * 0.05f), (int)(L * 0.07f), R_FAUNA + FA_LGREY);    // the skull
    (void)seed;
}

static void drawClam(float sx, float by, float s, float a, uint32_t seed) {
    float R = a * s;
    ellipse(sx, by - R * 0.3f, R * 0.8f, R * 0.35f, R_FAUNA + FA_LGREY);
    ellipse(sx, by - R * 0.42f, R * 0.68f, R * 0.14f, R_FAUNA2 + (H32(seed, 1) & 1 ? F2_BLUE : F2_CYAN));
    for (int x = (int)(sx - R * 0.7f); x <= (int)(sx + R * 0.7f); x++) {
        int z = ((x - (int)sx) / (int)fmaxf(1.f, R * 0.15f)) & 1;
        put(x, (int)(by - R * 0.42f - (z ? 1 : 0)), R_FAUNA + FA_WHITE);
        if ((x & 3) == 0) put(x, (int)(by - R * 0.45f), R_FAUNA2 + F2_CYAN);
    }
}

static void drawDetail(float sx, float by, float s, float a, uint8_t var, uint32_t seed) {
    float R = a * s;
    if (R < 1.5f) return;
    switch (var) {
        case 0: {   // a starfish
            uint8_t c = (uint8_t)(R_FAUNA + ((H32(seed, 1) % 3) == 0 ? FA_OCTO_R : FA_OCTO_O));
            if (H32(seed, 2) % 4 == 0) c = R_FAUNA2 + F2_PURPLE;
            for (int k = 0; k < 5; k++) { float ang = k * 1.2566f + Hf(seed, 3); thickLine(sx, by - R * 0.15f, sx + cosf(ang) * R, by - R * 0.15f + sinf(ang) * R * 0.35f, fmaxf(1.f, R * 0.3f), c); }
            break;
        }
        case 1: {   // an urchin
            splat((int)sx, (int)(by - R * 0.4f), (int)(R * 0.4f), R_FAUNA + FA_BLACK);
            for (int k = 0; k < 10; k++) { float ang = k * 0.628f; line((int)sx, (int)(by - R * 0.4f), (int)(sx + cosf(ang) * R), (int)(by - R * 0.4f + sinf(ang) * R * 0.8f), R_FAUNA + FA_DGREY); }
            break;
        }
        case 2:     // a shell
            ellipse(sx, by - R * 0.3f, R * 0.5f, R * 0.3f, R_FAUNA + FA_SHELL);
            line((int)(sx - R * 0.4f), (int)(by - R * 0.3f), (int)(sx + R * 0.4f), (int)(by - R * 0.4f), R_FAUNA + FA_WHITE);
            break;
        default:    // a sea cucumber
            ellipse(sx, by - R * 0.2f, R * 0.9f, R * 0.22f, R_FAUNA2 + F2_RUST_D);
            for (int k = 0; k < 4; k++) put((int)(sx - R * 0.6f + k * R * 0.4f), (int)(by - R * 0.35f), R_FAUNA2 + F2_RUST);
            break;
    }
}

// ================================ creatures ================================
// A fish-shaped body in side view: head at `dir` (+1 right), tapered tail, lit back, pale belly, an eye.
static void fishBody(float cx, float cy, float len, float hgt, float dir, uint8_t back, uint8_t side, uint8_t belly, float wig, bool eye = true) {
    int n = (int)len + 1;
    for (int i = 0; i <= n; i++) {
        float v = (float)i / n;                                       // 0 tail .. 1 head
        float prof = sinf(3.14159f * clampf(v * 0.92f + 0.06f, 0.f, 1.f));
        prof = powf(prof, 0.8f) * (v < 0.2f ? 0.5f + v * 2.5f : 1.f);
        float hw = hgt * 0.5f * prof;
        float x = cx + dir * (v - 0.5f) * len;
        float yo = sinf(wig - v * 4.f) * hgt * 0.12f * (1.f - v);
        int y0 = (int)(cy + yo - hw), y1 = (int)(cy + yo + hw);
        for (int y = y0; y <= y1; y++) {
            float q = (y - (cy + yo - hw)) / fmaxf(1.f, 2.f * hw);
            put((int)x, y, q < 0.3f ? back : q < 0.72f ? side : belly);
        }
    }
    // the tail fin
    float tx = cx - dir * len * 0.5f;
    float tw = hgt * 0.65f, flick = sinf(wig) * hgt * 0.2f;
    fillTri(tx + dir * len * 0.06f, cy, tx - dir * len * 0.16f, cy - tw + flick, tx - dir * len * 0.16f, cy + tw + flick, back);
    if (eye && len > 6.f) put((int)(cx + dir * len * 0.36f), (int)(cy - hgt * 0.12f), R_FAUNA + FA_BLACK);
}

static void drawFishSmall(float sx, float sy, float len, float dir, uint8_t body, uint8_t hi, float wig) {
    if (len < 2.5f) { put((int)sx, (int)sy, body); put((int)(sx + dir), (int)sy, hi); return; }
    fishBody(sx, sy, len, fmaxf(1.f, len * 0.42f), dir, body, body, hi, wig);
}

static float s_camBelow = 0.f;      // > 0: you're looking up at its belly

static void drawCritter(const Critter& c, float sx, float sy, float s, float dir) {
    float wig = c.phase * 6.f;
    float sz = c.size;
    switch (c.kind) {
        case CR_MANTA: {
            float span = 4.2f * sz * s, chord = 1.8f * sz * s;
            float flap = sinf(c.phase * 1.4f) * span * 0.16f;
            uint8_t top = s_camBelow > 0.f ? (uint8_t)(R_FAUNA + FA_WHITE) : (uint8_t)(R_FAUNA + FA_BLACK);
            fillTri(sx + dir * chord * 0.5f, sy, sx - dir * chord * 0.3f, sy, sx - dir * chord * 0.1f, sy - span * 0.5f - flap, top);
            fillTri(sx + dir * chord * 0.5f, sy, sx - dir * chord * 0.3f, sy, sx - dir * chord * 0.1f, sy + span * 0.12f + flap * 0.3f, R_FAUNA + FA_DGREY);
            ellipse(sx, sy, chord * 0.45f, fmaxf(1.f, chord * 0.14f), R_FAUNA + FA_DGREY);
            line((int)(sx - dir * chord * 0.4f), (int)sy, (int)(sx - dir * chord * 1.3f), (int)(sy + sinf(c.phase) * 2.f), R_FAUNA + FA_BLACK);
            put((int)(sx + dir * chord * 0.55f), (int)(sy - 1), R_FAUNA + FA_BLACK);
            break;
        }
        case CR_TURTLE: {
            float L = 1.1f * sz * s;
            float paddle = sinf(c.phase * 1.6f);
            fillTri(sx + dir * L * 0.15f, sy, sx - dir * L * 0.05f, sy + 1, sx + dir * L * 0.05f, sy + L * (0.4f + 0.2f * paddle), R_FAUNA + FA_TURT_G);
            ellipse(sx, sy - L * 0.05f, L * 0.45f, L * 0.26f, R_FAUNA + FA_TURT_B);
            if (L > 8.f) for (int k = 0; k < 5; k++) put((int)(sx - L * 0.3f + k * L * 0.15f), (int)(sy - L * 0.15f), R_FAUNA + FA_SHELL);
            hspan((int)(sx - L * 0.35f), (int)(sx + L * 0.35f), (int)(sy - L * 0.25f), R_FAUNA + FA_SHELL);
            ellipse(sx + dir * L * 0.55f, sy - L * 0.02f, L * 0.13f, L * 0.09f, R_FAUNA + FA_TURT_G);
            fillTri(sx - dir * L * 0.3f, sy + 1, sx - dir * L * 0.45f, sy + 1, sx - dir * L * 0.5f, sy + L * 0.2f, R_FAUNA + FA_TURT_G);
            if (L > 6.f) put((int)(sx + dir * L * 0.6f), (int)(sy - L * 0.05f), R_FAUNA + FA_BLACK);
            break;
        }
        case CR_SHARK: case CR_DOLPHIN: case CR_COELACANTH: case CR_NARWHAL: {
            bool sh = c.kind == CR_SHARK, dol = c.kind == CR_DOLPHIN, coel = c.kind == CR_COELACANTH;
            int count = dol ? c.count : 1;
            for (int k = 0; k < count; k++) {
                uint32_t hk = H32(c.seed, k);
                float ox = k ? (Hf(hk, 1) - 0.5f) * 4.f * s : 0.f, oy = k ? (Hf(hk, 2) - 0.5f) * 1.5f * s + sinf(c.phase + k) * s * 0.3f : 0.f;
                float L = (sh ? 2.6f : dol ? 2.3f : coel ? 1.8f : 4.f) * sz * s, Hh = L * (sh ? 0.24f : dol ? 0.26f : coel ? 0.3f : 0.25f);
                uint8_t back = (uint8_t)(R_FAUNA + (sh ? FA_SHARK : dol ? FA_GREY : FA_NARWHAL)), belly = R_FAUNA + FA_WHITE;
                if (coel) { back = R_FAUNA2 + F2_COEL; belly = R_FAUNA2 + F2_COEL; }
                float x = sx + ox, y = sy + oy;
                fishBody(x, y, L, Hh, dir, back, back, belly, wig * (sh ? 0.6f : 0.4f) + k);
                fillTri(x - dir * L * 0.05f, y - Hh * 0.45f, x + dir * L * 0.12f, y - Hh * 0.45f, x - dir * L * 0.06f, y - Hh * (sh ? 1.15f : 0.9f), back);   // dorsal
                fillTri(x + dir * L * 0.1f, y + Hh * 0.2f, x + dir * L * 0.2f, y + Hh * 0.2f, x, y + Hh * 0.75f, back);                                     // pectoral
                if (dol) line((int)(x + dir * L * 0.5f), (int)(y + Hh * 0.1f), (int)(x + dir * L * 0.62f), (int)(y + Hh * 0.15f), back);                  // beak
                if (c.kind == CR_NARWHAL) {
                    float tx = x + dir * L * 0.5f, ty = y - Hh * 0.05f;
                    line((int)tx, (int)ty, (int)(tx + dir * L * 0.55f), (int)(ty - L * 0.04f), R_FAUNA + FA_WHITE);
                    for (int q = 0; q < 6 && L > 10.f; q++) put((int)(x + (Hf(c.seed, 50 + q) - 0.5f) * L * 0.7f), (int)(y - Hh * 0.2f + (Hf(c.seed, 60 + q) - 0.5f) * Hh * 0.4f), R_FAUNA + FA_DGREY);
                }
                if (coel && L > 8.f) for (int q = 0; q < 8; q++) put((int)(x + (Hf(c.seed, 70 + q) - 0.5f) * L * 0.6f), (int)(y + (Hf(c.seed, 80 + q) - 0.5f) * Hh * 0.6f), R_FAUNA2 + F2_COEL_S);
                if (sh && L > 12.f) for (int q = 0; q < 3; q++) put((int)(x + dir * L * (0.25f - q * 0.03f)), (int)y, R_FAUNA + FA_DGREY);
            }
            break;
        }
        case CR_WHALE: {
            float L = 13.f * sz * s, Hh = L * 0.2f;
            uint8_t body = R_FAUNA + FA_WHALE, belly = R_FAUNA + FA_GREY;
            fishBody(sx, sy, L, Hh, dir, body, body, belly, c.phase * 1.2f, false);
            float fl = sinf(c.phase * 0.8f) * Hh * 0.6f;
            fillTri(sx + dir * L * 0.15f, sy + Hh * 0.2f, sx + dir * L * 0.05f, sy + Hh * 0.25f, sx - dir * L * 0.12f, sy + Hh * 1.4f + fl, R_FAUNA + FA_LGREY);   // the long flipper
            for (int k = 0; k < 4 && L > 20.f; k++) line((int)(sx + dir * L * 0.4f), (int)(sy + Hh * (0.15f + k * 0.07f)), (int)(sx + dir * L * 0.05f), (int)(sy + Hh * (0.22f + k * 0.07f)), belly);
            if (L > 20.f) put((int)(sx + dir * L * 0.35f), (int)(sy), R_FAUNA + FA_BLACK);
            break;
        }
        case CR_JELLY: {
            int n = c.count;
            for (int k = 0; k < n; k++) {
                uint32_t hk = H32(c.seed, k);
                float wx = c.x + (Hf(hk, 1) - 0.5f) * 5.f, wy = c.y + (Hf(hk, 2) - 0.5f) * 5.f, wz = c.z + (Hf(hk, 3) - 0.5f) * 4.f + sinf(s_t * 0.7f + k) * 0.3f;
                float jx, jy, jf;
                if (!proj(wx, wy, wz, jx, jy, jf)) continue;
                float js = F / jf;
                float pulse = 0.5f + 0.5f * sinf(s_t * 2.2f + Hf(hk, 4) * 6.f);
                float R = (0.25f + 0.25f * Hf(hk, 5)) * sz * js;
                float rx = R * (0.9f + 0.15f * pulse), ry = R * (0.75f - 0.2f * pulse);
                bool violet = (hk >> 9) & 1;
                uint8_t bell = s_glowy ? glowI(3, 1 + (int)(pulse * 1.5f)) : (uint8_t)(R_FAUNA + (violet ? FA_JELLY_V : FA_JELLY_P));
                uint8_t rim = s_glowy ? glowI(3, 3) : (uint8_t)(R_FAUNA + FA_WHITE);
                for (int t = 0; t < 5; t++) {                                 // trailing tentacles
                    float tx = jx + (t - 2) * rx * 0.35f;
                    float px = tx, py = jy;
                    for (int q = 1; q <= 6; q++) {
                        float yy = jy + q * R * 0.45f, xx = tx + sinf(s_t * 1.6f + q * 0.8f + t) * R * 0.18f * q * 0.3f;
                        line((int)px, (int)py, (int)xx, (int)yy, t & 1 ? bell : (uint8_t)(R_FAUNA + (violet ? FA_JELLY_P : FA_JELLY_V)));
                        px = xx; py = yy;
                    }
                }
                float ga = g_alpha;
                for (int y = (int)(jy - ry); y <= (int)jy; y++) {
                    float v = (jy - y) / fmaxf(ry, 0.5f);
                    float hw = rx * sqrtf(fmaxf(0.f, 1.f - v * v));
                    g_alpha = ga * (v > 0.75f ? 1.f : 0.62f);                       // you can see through the bell
                    hspan((int)(jx - hw), (int)(jx + hw), y, v > 0.75f ? rim : bell);
                    g_alpha = ga;
                    put((int)(jx - hw), y, rim); put((int)(jx + hw), y, rim);
                }
                if (rx > 3.f) splat((int)jx, (int)(jy - ry * 0.4f), (int)(rx * 0.3f), s_glowy ? glowI(3, 3) : (uint8_t)(R_FAUNA + (violet ? FA_JELLY_P : FA_JELLY_V)));   // the gonads' four-leaf glow
                hspan((int)(jx - rx), (int)(jx + rx), (int)jy, rim);
            }
            break;
        }
        case CR_OCTOPUS: {
            float L = 0.9f * sz * s;
            for (int k = 0; k < 8; k++) {
                float ang = 3.14159f * (k / 7.f) - 3.14159f;
                float px = sx, py = sy - L * 0.15f;
                for (int q = 1; q <= 5; q++) {
                    float u = q / 5.f;
                    float curl = sinf(s_t * 1.2f + k + u * 3.f) * 0.6f * u;
                    float xx = sx + cosf(ang * 0.5f + 1.57f + curl + (k - 3.5f) * 0.25f) * L * u * 1.1f * (k < 4 ? -1.f : 1.f) * 0.8f, yy = sy - L * 0.1f + u * L * 0.1f + sinf(curl * 2.f) * L * 0.1f;
                    thickLine(px, py, xx, yy, fmaxf(1.f, L * 0.1f * (1.f - u * 0.7f)), R_FAUNA + (q & 1 ? FA_OCTO_R : FA_OCTO_O));
                    px = xx; py = yy;
                }
            }
            ellipse(sx - dir * L * 0.15f, sy - L * 0.45f, L * 0.32f, L * 0.36f, R_FAUNA + FA_OCTO_O);
            ellipse(sx - dir * L * 0.18f, sy - L * 0.55f, L * 0.18f, L * 0.18f, R_FAUNA + FA_OCTO_R);
            if (L > 6.f) put((int)(sx + dir * L * 0.05f), (int)(sy - L * 0.3f), R_FAUNA + FA_BLACK);
            break;
        }
        case CR_ANGLER: {
            float L = 0.7f * sz * s;
            ellipse(sx, sy, L * 0.5f, L * 0.42f, R_FAUNA + FA_BLACK);
            ellipse(sx - dir * L * 0.05f, sy - L * 0.08f, L * 0.4f, L * 0.3f, R_FAUNA + FA_DGREY);
            fillTri(sx - dir * L * 0.5f, sy, sx - dir * L * 0.85f, sy - L * 0.3f, sx - dir * L * 0.85f, sy + L * 0.3f, R_FAUNA + FA_BLACK);
            hspan((int)(sx + dir * L * 0.1f), (int)(sx + dir * L * 0.5f), (int)(sy + L * 0.12f), R_FAUNA + FA_BLACK);   // the jaw
            for (int k = 0; k < 4 && L > 6.f; k++) put((int)(sx + dir * (L * 0.15f + k * L * 0.1f)), (int)(sy + L * 0.08f), R_FAUNA + FA_WHITE);
            float lx = sx + dir * L * 0.65f, ly = sy - L * 0.55f + sinf(s_t * 1.5f) * L * 0.05f;
            line((int)(sx + dir * L * 0.1f), (int)(sy - L * 0.38f), (int)lx, (int)ly, R_FAUNA + FA_DGREY);
            float ga = g_alpha; uint8_t gz = g_zc; g_alpha = 1.f; g_zc = 0;    // the lure shines through the dark
            splat((int)lx, (int)ly, (int)fmaxf(1.f, L * 0.12f), glowI(1, 1));
            put((int)lx, (int)ly, glowI(1, 3));
            g_alpha = ga; g_zc = gz;
            break;
        }
        case CR_SERPENT: {
            // the leviathan: a long body following a wave behind its head, in the world
            float vx = c.vx, vy = c.vy, vl = sqrtf(vx * vx + vy * vy) + 1e-3f;
            vx /= vl; vy /= vl;
            const int N = 44;
            float segL = 0.42f * sz;
            for (int k = N - 1; k >= 0; k--) {
                float u = (float)k / N;
                float lat = sinf(c.phase * 1.2f - k * 0.22f) * 1.8f * (0.3f + u);
                float vert = sinf(c.phase * 0.7f - k * 0.15f) * 0.9f;
                float wx = c.x - vx * k * segL - vy * lat, wy = c.y - vy * k * segL + vx * lat, wz = c.z + vert;
                float px, py, pf;
                if (!proj(wx, wy, wz, px, py, pf)) continue;
                float r = (0.6f - 0.45f * u) * sz * F / pf;
                int ri = (int)fmaxf(1.f, r);
                splat((int)px, (int)py, ri + 1, R_FAUNA + FA_BLACK);                 // outline
                splat((int)px, (int)py, ri, R_FAUNA2 + F2_COEL);
                if (ri > 2) { hspan((int)(px - r * 0.6f), (int)(px + r * 0.6f), (int)(py + r * 0.45f), R_FAUNA + FA_NARWHAL);    // pale belly
                              hspan((int)(px - r * 0.5f), (int)(px + r * 0.5f), (int)(py - r * 0.55f), R_FAUNA + FA_SHARK); }   // lit back
                if (r > 3.f && (k % 3) == 0) fillTri(px - r * 0.3f, py - r * 0.8f, px + r * 0.3f, py - r * 0.8f, px, py - r * 1.6f, R_FAUNA + FA_DGREY);   // spines
                if (k == 0) {
                    ellipse(px + dir * r * 0.9f, py - r * 0.1f, r * 1.6f, r * 0.85f, R_FAUNA2 + F2_COEL);
                    hspan((int)(px + dir * r * 0.2f), (int)(px + dir * r * 2.3f), (int)(py + r * 0.35f), R_FAUNA + FA_BLACK);   // the jaw line
                    for (int q = 0; q < 4; q++) fillTri(px - dir * r * (0.2f + q * 0.35f), py - r * 0.6f, px - dir * r * (0.5f + q * 0.35f), py - r * 0.6f, px - dir * r * (0.6f + q * 0.4f), py - r * (1.7f + q * 0.2f), R_FAUNA + FA_SHARK);   // the frill
                    float ga = g_alpha; uint8_t gz = g_zc; g_alpha = 1.f; g_zc = 0;
                    int ex = (int)(px + dir * r * 1.4f), ey = (int)(py - r * 0.35f);
                    put(ex, ey, glowI(0, 3)); put(ex - (int)dir, ey, glowI(0, 2)); if (r > 4.f) put(ex, ey - 1, glowI(0, 2));
                    g_alpha = ga; g_zc = gz;
                }
            }
            break;
        }
        case CR_CRAB: case CR_ISOPOD: {
            int n = c.kind == CR_CRAB ? c.count : 1;
            bool white = biomeAt(g_scene.distance).vents > 0.5f;
            for (int k = 0; k < n; k++) {
                uint32_t hk = H32(c.seed, k);
                float ox = (Hf(hk, 1) - 0.5f) * 3.f * s, oy = (Hf(hk, 2) - 0.5f) * 0.4f * s;
                float L = (c.kind == CR_CRAB ? 0.3f : 0.35f) * sz * s;
                if (L < 1.5f) { put((int)(sx + ox), (int)(sy + oy), R_FAUNA + FA_WHITE); continue; }
                float x = sx + ox + sinf(c.phase * 2.f + k) * L * 0.2f, y = sy + oy;
                if (c.kind == CR_CRAB) {
                    uint8_t col = (uint8_t)(R_FAUNA + (white ? FA_WHITE : FA_OCTO_R));
                    for (int q = 0; q < 3; q++) { line((int)(x - L * 0.3f), (int)(y - L * 0.2f), (int)(x - L * 0.75f), (int)(y + L * 0.1f * q), col); line((int)(x + L * 0.3f), (int)(y - L * 0.2f), (int)(x + L * 0.75f), (int)(y + L * 0.1f * q), col); }
                    ellipse(x, y - L * 0.25f, L * 0.42f, L * 0.25f, col);
                    splat((int)(x - L * 0.6f), (int)(y - L * 0.55f), (int)(L * 0.15f), col); splat((int)(x + L * 0.6f), (int)(y - L * 0.55f), (int)(L * 0.15f), col);
                } else {
                    ellipse(x, y - L * 0.2f, L * 0.55f, L * 0.25f, R_FAUNA2 + F2_LILAC);
                    for (int q = 0; q < 5; q++) put((int)(x - L * 0.4f + q * L * 0.2f), (int)(y - L * 0.35f), R_FAUNA + FA_GREY);
                    line((int)(x + dir * L * 0.5f), (int)(y - L * 0.3f), (int)(x + dir * L * 0.8f), (int)(y - L * 0.5f), R_FAUNA2 + F2_LILAC);
                }
            }
            break;
        }
        case CR_TRIPOD: {
            float L = 0.35f * sz * s, legs = 0.6f * s;
            for (int k = -1; k <= 1; k++) line((int)(sx + k * L * 0.3f), (int)(sy - legs), (int)(sx + k * L * 0.5f), (int)sy, R_FAUNA + FA_GREY);
            fishBody(sx, sy - legs, L, L * 0.3f, dir, R_FAUNA + FA_DGREY, R_FAUNA + FA_GREY, R_FAUNA + FA_LGREY, 0.f);
            break;
        }
        case CR_NAUTILUS: {
            float R = 0.22f * sz * s;
            splat((int)sx, (int)sy, (int)fmaxf(1.f, R), R_FAUNA + FA_WHITE);
            for (int k = 0; k < 4 && R > 3.f; k++) { float a = k * 0.8f; line((int)sx, (int)sy, (int)(sx - dir * cosf(a) * R), (int)(sy - sinf(a) * R), R_FAUNA + FA_OCTO_O); }
            splat((int)(sx - dir * R * 0.2f), (int)(sy + R * 0.1f), (int)fmaxf(0.f, R * 0.35f), R_FAUNA + FA_SHELL);
            for (int k = 0; k < 5; k++) line((int)(sx + dir * R * 0.8f), (int)(sy + R * 0.2f), (int)(sx + dir * R * (1.4f + 0.1f * k)), (int)(sy + R * (0.1f + 0.15f * k) + sinf(s_t * 2.f + k)), R_FAUNA + FA_OCTO_O);
            break;
        }
        case CR_EEL: {
            float L = 1.8f * sz * s;
            int n = (int)fmaxf(4.f, L * 0.5f);
            float px = sx + dir * L * 0.5f, py = sy;
            for (int q = 1; q <= n; q++) {
                float u = (float)q / n;
                float xx = sx + dir * (0.5f - u) * L, yy = sy + sinf(c.phase * 3.f - u * 7.f) * L * 0.07f;
                thickLine(px, py, xx, yy, fmaxf(1.f, L * 0.08f * (1.f - u * 0.6f)), R_FAUNA + (q & 2 ? FA_TURT_G : FA_SHELL));
                px = xx; py = yy;
            }
            if (L > 8.f) put((int)(sx + dir * L * 0.45f), (int)(sy - 1), R_FAUNA + FA_BLACK);
            break;
        }
    }
}

static void drawSchool(const School& sc) {
    for (int i = 0; i < sc.n; i++) {
        float sx, sy, f;
        if (!proj(sc.fx[i], sc.fy[i], sc.fz[i], sx, sy, f)) continue;
        if (sx < -6 || sx > W + 6 || sy < -6 || sy > H + 6) continue;
        float d = f;
        float fog = fogOf(d);
        if (fog > 0.93f) continue;
        g_alpha = 1.f - smoothstep(0.6f, 0.93f, fog);
        g_zc = zcode(d);
        float s = F / f;
        uint32_t h = H32(sc.seed, i);
        // which way it's going across the view
        float ax, ay, af;
        float dir = 1.f;
        if (proj(sc.fx[i] + sc.fvx[i] * 0.3f, sc.fy[i] + sc.fvy[i] * 0.3f, sc.fz[i], ax, ay, af)) dir = ax >= sx ? 1.f : -1.f;
        float wig = s_t * 9.f + (h & 63);
        switch (sc.kind) {
            case SCH_REEF: {
                static const uint8_t RC[6] = {F2_ORANGE, F2_YELLOW, F2_BLUE, F2_PURPLE, F2_PINK, F2_CYAN};
                uint8_t body = R_FAUNA2 + RC[(sc.seed >> 3) % 6 == (h % 6) ? (h >> 4) % 6 : (sc.seed >> 3) % 6];
                drawFishSmall(sx, sy, 0.28f * s, dir, body, R_FAUNA + FA_WHITE, wig);
                break;
            }
            case SCH_SILVER: {
                bool glint = ((h + (uint32_t)(s_t * 4.f)) & 15) == 0;
                drawFishSmall(sx, sy, 0.24f * s, dir, R_FAUNA2 + F2_SILV_D, glint ? (uint8_t)(R_FAUNA2 + F2_SILV_L) : (uint8_t)(R_FAUNA2 + F2_SILV), wig);
                break;
            }
            case SCH_GLOW: {
                drawFishSmall(sx, sy, 0.2f * s, dir, R_FAUNA + FA_BLACK, R_FAUNA + FA_DGREY, wig);
                float ga = g_alpha; g_alpha = fmaxf(ga, 0.8f);
                put((int)sx, (int)sy, glowI(1, 3)); if (s > 15.f) put((int)(sx - dir * 2), (int)sy, glowI(1, 2));
                g_alpha = ga;
                break;
            }
            default:
                drawFishSmall(sx, sy, 0.3f * s, dir, R_FAUNA + FA_DGREY, R_FAUNA + FA_GREY, wig);
                break;
        }
    }
}

// ================================ painter's order ================================
struct Item { float d2; uint16_t idx; uint8_t kind; };
static Item* s_items = nullptr;
static constexpr int MAXI = FIELD_N + CRIT_N + SCHOOL_N;

void propsInit() { if (!s_items) s_items = (Item*)memFast(sizeof(Item) * MAXI); }

void drawProps() {
    if (!s_items) return;
    s_t = g_t;
    const BiomeParams& b = biomeAt(g_scene.distance);
    s_glowy = b.glowAmt > 0.5f || g_scene.depth > 180.f;
    const FieldObj* objs = fieldObjects();
    const Critter* cr = fieldCritters();
    const School* sc = fieldSchools();
    int n = 0;
    for (int i = 0; i < FIELD_N; i++) {
        if (objs[i].type == OBJ_NONE) continue;
        float dx = objs[i].x - g_cx, dy = objs[i].y - g_cy;
        float f = dx * g_cY + dy * g_sY;
        if (f < 0.3f) continue;
        float l = -dx * g_sY + dy * g_cY;
        if (fabsf(l) > f * 0.75f + 8.f) continue;                       // well outside the view
        s_items[n++] = {dx * dx + dy * dy, (uint16_t)i, 0};
    }
    for (int i = 0; i < CRIT_N; i++) if (cr[i].on) { float dx = cr[i].x - g_cx, dy = cr[i].y - g_cy, dz = cr[i].z - g_cz; s_items[n++] = {dx * dx + dy * dy + dz * dz, (uint16_t)i, 1}; }
    for (int i = 0; i < SCHOOL_N; i++) if (sc[i].on) { float dx = sc[i].x - g_cx, dy = sc[i].y - g_cy; s_items[n++] = {dx * dx + dy * dy, (uint16_t)i, 2}; }
    for (int i = 1; i < n; i++) {                                         // far to near
        Item it = s_items[i];
        int j = i - 1;
        while (j >= 0 && s_items[j].d2 < it.d2) { s_items[j + 1] = s_items[j]; j--; }
        s_items[j + 1] = it;
    }
    for (int k = 0; k < n; k++) {
        const Item& it = s_items[k];
        if (it.kind == 2) { drawSchool(sc[it.idx]); continue; }
        if (it.kind == 1) {
            const Critter& c = cr[it.idx];
            float sx, sy, f;
            if (!proj(c.x, c.y, c.z, sx, sy, f)) continue;
            float d = sqrtf(it.d2);
            float fog = fogOf(d);
            if (sx < -200 || sx > W + 200) continue;
            float s = F / f;
            float ax, ay, af, dir = 1.f;
            if (proj(c.x + c.vx * 0.5f, c.y + c.vy * 0.5f, c.z, ax, ay, af)) dir = ax >= sx ? 1.f : -1.f;
            s_camBelow = g_cz < c.z - 1.5f ? 1.f : 0.f;
            g_zc = zcode(d);
            if (c.kind == CR_WHALE && fog > 0.55f) {                     // a whale in the blue: a vast shadow
                g_alpha = 1.f - smoothstep(0.9f, 0.99f, fog);
                float L = 13.f * c.size * s, Hh = L * 0.2f;
                fishBody(sx, sy, L, Hh, dir, R_WATER + 1, R_WATER + 2, R_WATER + 3, c.phase * 1.2f, false);
                continue;
            }
            g_alpha = 1.f - smoothstep(0.62f, 0.93f, fog);
            if (g_alpha <= 0.03f && c.kind != CR_ANGLER) continue;
            s_far = fog > 0.4f;
            drawCritter(c, sx, sy, s, dir);
            continue;
        }
        const FieldObj& o = objs[it.idx];
        float sx, by, f;
        if (!proj(o.x, o.y, o.z, sx, by, f)) continue;
        float s = F / f;
        float vz = o.z - g_cz;
        float d = sqrtf(it.d2 + vz * vz);
        float fog = fogOf(d);
        if (o.solid && sqrtf(it.d2) < o.radius + 0.6f) continue;               // you're inside its footprint
        float edge = clampf((FIELD_R - sqrtf(it.d2)) / 6.f, 0.f, 1.f);       // re-dreamt things fade in at the edge
        g_alpha = (1.f - smoothstep(0.6f, 0.93f, fog)) * edge;
        if (g_alpha <= 0.04f) continue;
        g_zc = zcode(d);
        s_far = fog > 0.38f;
        switch (o.type) {
            case OBJ_KELP:    drawKelp(sx, by, s, o.a, o.seed); break;
            case OBJ_GRASS:   if (s > 2.f) drawGrass(sx, by, s, o.a, o.seed); break;
            case OBJ_CORAL:   if (o.a * s > 1.5f) drawCoral(sx, by, s, o.a, o.var, o.seed); break;
            case OBJ_ANEMONE: if (s > 3.f) drawAnemone(sx, by, s, o.a, o.seed); break;
            case OBJ_SPONGE:  drawSponge(sx, by, s, o.a, o.var, o.seed); break;
            case OBJ_ROCK:    drawRock(sx, by, s, o.a, o.seed); break;
            case OBJ_PILLAR:  drawPillar(sx, by, s, o.a, o.b, o.seed); break;
            case OBJ_BASALT:  drawBasalt(sx, by, s, o.a, o.b, o.seed); break;
            case OBJ_VENT:    drawVent(sx, by, s, o.a, o.var, o.seed); break;
            case OBJ_RUIN:    drawRuin(sx, by, s, o.a, o.b, o.var, o.seed); break;
            case OBJ_MONOLITH: drawMonolith(sx, by, s, o.a, o.b, o.seed); break;
            case OBJ_WRECK:   drawWreck(sx, by, s, o.a, o.b, o.var, o.seed); break;
            case OBJ_ICE:     drawIce(sx, by, s, o.a, o.b, o.seed); break;
            case OBJ_CRYSTAL: drawCrystal(sx, by, s, o.a, o.seed); break;
            case OBJ_DEADTREE: drawDeadTree(sx, by, s, o.a, o.seed); break;
            case OBJ_BONES:   drawBones(sx, by, s, o.a, o.seed); break;
            case OBJ_CLAM:    drawClam(sx, by, s, o.a, o.seed); break;
            case OBJ_DETAIL:  if (s > 6.f) drawDetail(sx, by, s, o.a, o.var, o.seed); break;
            default: break;
        }
    }
    g_alpha = 1.f;
    s_far = false;
}

}
}
