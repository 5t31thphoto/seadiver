#include "terrain.h"
#include "universe.h"
#include "journey.h"
#include "mem.h"
#include <math.h>
#include <string.h>

namespace mantis {

static int16_t* s_h = nullptr;      // TG_N * TG_N, decimetres
static uint8_t* s_m = nullptr;
static int s_ox = 0, s_oy = 0;      // valid window origin (cells)
static float s_px = 0.f, s_py = 0.f, s_D = 0.f;
static uint32_t s_gen = 1;

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float smoothstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }

// ---------------- the shape of the seabed ----------------
// Volcanic cones and smoker mounds sit on a 90 m lattice of feature points.
static float conesAt(float x, float y, float* crater) {
    const float C = 90.f;
    float gx = x / C, gy = y / C;
    int ix = (int)floorf(gx), iy = (int)floorf(gy);
    float best = 1e9f, bestR = 1.f;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            uint32_t h = hash32((uint32_t)(ix + i) * 0x9E3779B1u ^ (uint32_t)(iy + j) * 0x85EBCA77u ^ 0xC0E5u ^ worldSeed());
            if ((h & 3u) == 0u) continue;                                         // not every cell has one
            float fx = (ix + i + 0.2f + 0.6f * ((h >> 8) & 255) / 255.f) * C, fy = (iy + j + 0.2f + 0.6f * ((h >> 16) & 255) / 255.f) * C;
            float R = 26.f + 22.f * ((h >> 24) & 255) / 255.f;
            float d = sqrtf((x - fx) * (x - fx) + (y - fy) * (y - fy)) / R;
            if (d < best) { best = d; bestR = R; }
        }
    (void)bestR;
    if (best >= 1.f) { *crater = 0.f; return 0.f; }
    float cone = powf(1.f - best, 1.6f);
    *crater = best < 0.22f ? (1.f - best / 0.22f) : 0.f;
    return cone;
}

float terrainShape(float x, float y, float D, uint8_t* mat) {
    const BiomeParams& b = biomeAt(D);
    float z = -b.floorBase;
    // broad rolling mounds
    z += (fbm2(x * 0.011f, y * 0.011f, 3, 0x5EA1u) - 0.5f) * 2.f * b.mounds;
    // sand waves laid down by the current (long, gently meandering crests)
    if (b.dunes > 0.01f) {
        float w = fbm2(x * 0.018f, y * 0.018f, 2, 0xD0E5u);
        z += sinf((x * 0.83f + y * 0.56f) * 0.33f + w * 7.f) * b.dunes;
    }
    // ridges: sharp crests, as broken rock
    float rg = 0.f;
    if (b.ridges > 0.01f) {
        float r = fbm2(x * 0.0075f, y * 0.0075f, 3, 0x41D6u);
        r = 1.f - fabsf(2.f * r - 1.f);
        rg = r * r;
        z += (rg - 0.35f) * 1.6f * b.ridges;
    }
    // cliffs: the seabed steps down a wall into the blue
    if (b.cliffs > 0.01f) {
        float c = fbm2(x * 0.0045f, y * 0.0045f, 2, 0xC11Fu);
        float st = smoothstep(0.42f, 0.5f, c) + smoothstep(0.58f, 0.64f, c);
        z += (st - 1.f) * b.cliffs;
    }
    // a winding canyon cut down into the plateau
    float cut = 0.f;
    if (b.canyon > 0.01f) {
        float n = fbm2(x * 0.0042f, y * 0.0042f, 3, 0xCA4Eu);
        float w = fabsf(n - 0.5f);
        cut = 1.f - smoothstep(0.018f, 0.075f, w);
        float walls = 1.f - smoothstep(0.07f, 0.16f, w);                        // the rim rises a little toward the edge
        z += b.canyon * (0.45f + 0.1f * walls - cut);
    }
    // terraces: the drowned city's steps
    if (b.terrace > 0.2f) {
        float t = b.terrace;
        float q = floorf(z / t) * t;
        z = q + (z - q) * 0.18f;
    }
    // volcanic cones with craters
    float crater = 0.f, cone = 0.f;
    if (b.cones > 0.5f) {
        cone = conesAt(x, y, &crater);
        z += cone * b.cones - crater * b.cones * 0.35f;
    }
    // fine roughness: rock is rough, ooze is smooth
    z += (fbm2(x * 0.09f, y * 0.09f, 2, 0x70F7u) - 0.5f) * (0.5f + b.rockMix * 1.8f);
    // brine pools: hollows filled to a flat, heavy surface
    uint8_t feat = TF_NONE;
    if (b.pools > 0.3f) {
        float p = fbm2(x * 0.0095f, y * 0.0095f, 2, 0xB21Eu);
        if (p > 0.58f) {                                            // a hollow, and a lake lying in it with a flat surface
            float lvl = -b.floorBase - b.mounds * 0.5f - 1.f;
            float zc = z - (p - 0.58f) * 90.f * b.pools;
            if (zc < lvl) { z = lvl; feat = TF_BRINE; } else z = zc;
        }
    }
    if (mat) {
        float rough = (fbm2(x * 0.032f, y * 0.032f, 2, 0x2C0Cu) - 0.5f) * 2.4f;
        float rk = clampf(b.rockMix * 1.7f - 0.45f + rough + rg * 0.6f * (b.ridges > 4.f), 0.f, 0.999f);
        uint8_t r = (uint8_t)(rk * 4.f);
        if (feat == TF_NONE) {
            float f = fbm2(x * 0.05f, y * 0.05f, 2, 0xFEA7u);
            if (b.lava > 0.4f && (crater > 0.15f || fabsf(fbm2(x * 0.012f, y * 0.012f, 2, 0x1A7Au) - 0.5f) < 0.022f + 0.05f * cone)) feat = TF_LAVA;
            else if (b.grass > 0.5f && f > 0.46f) feat = TF_GRASS;
            else if (b.kelp > 0.5f && f > 0.6f) feat = TF_GRASS;
            else if (b.vents > 0.5f && f > 0.66f) feat = TF_TUBES;
            else if (b.iceCeiling > 0.5f && f > 0.5f && r < 2) feat = TF_SNOW;
            else if (b.glowAmt > 0.5f && f > 0.68f) feat = TF_GLOW;
            else if ((b.coral_ > 0.5f || b.ruins > 0.5f) && f > 0.64f) feat = TF_RUBBLE;
        }
        *mat = (uint8_t)((r & 3) << 3 | feat << 5);
    }
    return z;
}

// light on a cell from its slope: water lights from above; a little from the south-east
static uint8_t shadeFrom(float gx, float gy) {
    float nx = -gx, ny = -gy, nz = 1.f;
    float il = 1.f / sqrtf(nx * nx + ny * ny + 1.f);
    float d = (nx * 0.15f + ny * 0.12f + nz * 0.98f) * il;     // light comes down through the water, nearly straight
    return (uint8_t)clampf(1.4f + d * 4.3f, 0.f, 7.f);         // diffuse light: walls stay readable, flats leave room for caustics
}

static void dreamCell(int ix, int iy) {
    float x = ix * TG_CELL, y = iy * TG_CELL;
    float r = sqrtf((x - s_px) * (x - s_px) + (y - s_py) * (y - s_py));
    float D = s_D + r;
    uint8_t mat = 0;
    float z = terrainShape(x, y, D, &mat);
    float zx = terrainShape(x + 1.2f, y, D, nullptr), zy = terrainShape(x, y + 1.2f, D, nullptr);
    float sh = shadeFrom((zx - z) / 1.2f, (zy - z) / 1.2f);
    if (tmFeat(mat) == TF_BRINE) sh = 4;
    int idx = (iy & TG_MASK) * TG_N + (ix & TG_MASK);
    float dz = z * 10.f;
    s_h[idx] = (int16_t)(dz < -32000.f ? -32000 : (dz > 0.f ? 0 : (int)lroundf(dz)));
    s_m[idx] = (uint8_t)(mat | (uint8_t)sh);
}

void terrainInit(float px, float py, float D) {
    if (!s_h) s_h = (int16_t*)memFast(TG_N * TG_N * sizeof(int16_t));
    if (!s_m) s_m = (uint8_t*)memFast(TG_N * TG_N);
    s_px = px; s_py = py; s_D = D;
    s_ox = (int)floorf(px / TG_CELL) - TG_N / 2;
    s_oy = (int)floorf(py / TG_CELL) - TG_N / 2;
    for (int j = 0; j < TG_N; j++)
        for (int i = 0; i < TG_N; i++) dreamCell(s_ox + i, s_oy + j);
    s_gen++;
}

void terrainUpdate(float px, float py, float D) {
    s_px = px; s_py = py; s_D = D;
    int wx = (int)floorf(px / TG_CELL) - TG_N / 2, wy = (int)floorf(py / TG_CELL) - TG_N / 2;
    int budget = 3;                                         // rows/columns per frame; the far edge is deep in the haze
    while (budget > 0 && (s_ox != wx || s_oy != wy)) {
        if (s_ox < wx) { s_ox++; int ix = s_ox + TG_N - 1; for (int j = 0; j < TG_N; j++) dreamCell(ix, s_oy + j); }
        else if (s_ox > wx) { s_ox--; int ix = s_ox; for (int j = 0; j < TG_N; j++) dreamCell(ix, s_oy + j); }
        if (s_oy < wy) { s_oy++; int iy = s_oy + TG_N - 1; for (int i = 0; i < TG_N; i++) dreamCell(s_ox + i, iy); }
        else if (s_oy > wy) { s_oy--; int iy = s_oy; for (int i = 0; i < TG_N; i++) dreamCell(s_ox + i, iy); }
        budget--;
        s_gen++;
    }
}

float terrainFloor(float x, float y) {
    float gx = x / TG_CELL, gy = y / TG_CELL;
    int ix = (int)floorf(gx), iy = (int)floorf(gy);
    if (!s_h || ix < s_ox || iy < s_oy || ix + 1 >= s_ox + TG_N || iy + 1 >= s_oy + TG_N) {
        float r = sqrtf((x - s_px) * (x - s_px) + (y - s_py) * (y - s_py));
        return terrainShape(x, y, s_D + r, nullptr);
    }
    float fx = gx - ix, fy = gy - iy;
    int i0 = ix & TG_MASK, i1 = (ix + 1) & TG_MASK, j0 = (iy & TG_MASK) * TG_N, j1 = ((iy + 1) & TG_MASK) * TG_N;
    float a = s_h[j0 + i0], b = s_h[j0 + i1], c = s_h[j1 + i0], d = s_h[j1 + i1];
    float ab = a + (b - a) * fx, cd = c + (d - c) * fx;
    return (ab + (cd - ab) * fy) * 0.1f;
}

float terrainCeiling(float x, float y) {
    float r = sqrtf((x - s_px) * (x - s_px) + (y - s_py) * (y - s_py));
    const BiomeParams& b = biomeAt(s_D + r);
    if (b.iceCeiling < 0.05f) return 0.f;
    float n = fbm2(x * 0.02f, y * 0.02f, 3, 0x1CE5u);
    float lead = fabsf(fbm2(x * 0.006f, y * 0.006f, 2, 0x1EADu) - 0.5f);   // open leads in the ice
    float thick = (2.5f + 9.f * n) * b.iceCeiling * smoothstep(0.02f, 0.05f, lead);
    return -thick;
}

const int16_t* terrainH() { return s_h; }
const uint8_t* terrainM() { return s_m; }
int terrainOx() { return s_ox; }
int terrainOy() { return s_oy; }
uint32_t terrainGeneration() { return s_gen; }

}
