#include "field.h"
#include "terrain.h"
#include "universe.h"
#include "journey.h"
#include "scene.h"
#include "audio.h"
#include <math.h>
#include <string.h>

namespace mantis {

static FieldObj s_obj[FIELD_N];
static Critter s_cr[CRIT_N];
static School s_sc[SCHOOL_N];
static float s_next[CR_KINDS];
static float s_schoolNext = 2.f;
static uint32_t s_rng = 0x9E3779B9u;
static float s_px = 0, s_py = 0, s_pz = 0, s_D = 0;
static float s_nearest[OBJ_TYPES];

static float frand() { s_rng = hash32(s_rng + 0x6D2B79F5u); return (s_rng >> 8) * (1.f / 16777216.f); }
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static uint8_t featAt(float x, float y) {
    const uint8_t* m = terrainM();
    if (!m) return 0;
    int ix = (int)floorf(x / TG_CELL + 0.5f), iy = (int)floorf(y / TG_CELL + 0.5f);
    return tmFeat(m[(iy & TG_MASK) * TG_N + (ix & TG_MASK)]);
}

// ---------------- dreaming the seabed's furniture ----------------
static void spawnObj(FieldObj& o, float ang, float range) {
    o.x = s_px + range * cosf(ang);
    o.y = s_py + range * sinf(ang);
    o.z = terrainFloor(o.x, o.y);
    o.seed = hash32(s_rng ^ (uint32_t)(range * 977.f) ^ (uint32_t)(ang * 7919.f));
    o.type = OBJ_NONE; o.var = 0; o.solid = false; o.radius = 0.f; o.top = 0.f; o.a = o.b = 0.f;
    uint8_t f = featAt(o.x, o.y);
    if (f == TF_BRINE || f == TF_LAVA) return;                      // nothing grows in brine or lava
    const BiomeParams& b = biomeAt(s_D + range);
    struct W { uint8_t t, v; float w; };
    W w[] = {
        {OBJ_KELP, 0, b.kelp * 0.55f}, {OBJ_GRASS, 0, b.grass * 0.7f}, {OBJ_CORAL, 0, b.coral_ * 0.5f}, {OBJ_CORAL, 8, b.glowcoral * 0.45f},
        {OBJ_ANEMONE, 0, b.anemone * 0.15f}, {OBJ_SPONGE, 0, b.sponge * 0.16f}, {OBJ_ROCK, 0, b.rocks * 0.17f}, {OBJ_PILLAR, 0, b.pillars * 0.07f},
        {OBJ_BASALT, 0, b.basalt * 0.12f}, {OBJ_VENT, 0, b.vents * 0.06f}, {OBJ_RUIN, 0, b.ruins * 0.11f}, {OBJ_MONOLITH, 0, b.monolith * 0.03f},
        {OBJ_WRECK, 0, b.wreck * 0.08f}, {OBJ_ICE, 0, b.ice * 0.12f}, {OBJ_CRYSTAL, 0, b.crystals * 0.13f}, {OBJ_DEADTREE, 0, b.deadtrees * 0.09f},
        {OBJ_BONES, 0, b.bones * 0.035f}, {OBJ_CLAM, 0, b.clams * 0.05f}, {OBJ_DETAIL, 0, b.details * 0.22f}};
    const int NW = sizeof(w) / sizeof(w[0]);
    float sum = 0.f;
    for (int i = 0; i < NW; i++) sum += w[i].w;
    float p = frand();
    if (sum < 1e-4f || p > fminf(0.88f, sum)) return;
    float r = frand() * sum;
    int k = 0;
    for (; k < NW - 1; k++) { r -= w[k].w; if (r <= 0.f) break; }
    o.type = w[k].t; o.var = w[k].v;
    float q = frand();
    switch (o.type) {
        case OBJ_KELP:    o.a = fmaxf(3.f, fminf(-o.z - 1.f, 9.f + q * 20.f)); break;       // up toward the light
        case OBJ_GRASS:   o.a = 0.35f + q * 0.5f; break;
        case OBJ_CORAL:   o.var |= (uint8_t)(frand() * 6.f);
                          o.a = 0.8f + q * 1.9f; if ((o.var & 7) == 2) o.a *= 1.3f;
                          if (o.a > 1.2f && (o.var & 7) != 3) { o.solid = true; o.radius = o.a * 0.55f; o.top = o.a * 0.8f; }
                          break;
        case OBJ_ANEMONE: o.a = 0.4f + q * 0.5f; break;
        case OBJ_SPONGE:  o.a = 0.5f + q * 1.4f; o.var = b.zone > 1.5f ? 1 : 0; break;         // glass sponges in the deep
        case OBJ_ROCK:    o.a = 0.5f + q * 2.2f; o.solid = true; o.radius = o.a * 0.9f; o.top = o.a * 1.3f; break;
        case OBJ_PILLAR:  o.a = 1.2f + q * 2.2f; o.b = 8.f + frand() * 22.f; o.solid = true; o.radius = o.a; o.top = o.b; break;
        case OBJ_BASALT:  o.a = 1.f + q * 1.5f; o.b = 2.f + frand() * 6.f; o.solid = true; o.radius = o.a; o.top = o.b; break;
        case OBJ_VENT:    o.a = 3.f + q * 8.f; o.var = b.lava > 0.5f ? 1 : 0; o.solid = true; o.radius = 0.9f; o.top = o.a; break;
        case OBJ_RUIN:    o.var = (uint8_t)(frand() * 6.f); o.a = 3.f + q * 5.f; o.b = 2.f + frand() * 3.f;
                          if (o.var == 5) { o.a = 9.f + q * 5.f; o.b = 12.f + frand() * 6.f; }
                          o.solid = o.var != 2; o.radius = o.var == 5 ? o.b * 0.5f : (o.var == 3 ? o.b * 0.5f : 0.7f); o.top = o.a; break;
        case OBJ_MONOLITH: o.a = 4.f + q * 4.f; o.b = 1.2f + frand(); o.solid = true; o.radius = o.b * 0.6f; o.top = o.a; break;
        case OBJ_WRECK:   o.var = (uint8_t)(frand() * 4.f); o.a = 3.f + q * 8.f; o.b = 4.f + frand() * 8.f; o.solid = true; o.radius = o.b * 0.4f; o.top = o.a; break;
        case OBJ_ICE:     o.a = 1.5f + q * 6.f; o.b = 0.6f + frand() * 1.4f; o.solid = true; o.radius = o.b; o.top = o.a; break;
        case OBJ_CRYSTAL: o.a = 1.2f + q * 5.f; o.solid = o.a > 2.5f; o.radius = o.a * 0.35f; o.top = o.a; break;
        case OBJ_DEADTREE: o.a = 3.f + q * 5.f; o.solid = true; o.radius = 0.4f; o.top = o.a; break;
        case OBJ_BONES:   o.a = 6.f + q * 6.f; break;
        case OBJ_CLAM:    o.a = 0.6f + q * 0.7f; o.solid = true; o.radius = o.a * 0.8f; o.top = o.a * 0.6f; break;
        case OBJ_DETAIL:  o.var = (uint8_t)(frand() * 4.f); o.a = 0.2f + q * 0.25f; break;
        default: break;
    }
}

static bool overlaps(const FieldObj& o) {
    if (!o.solid) return false;
    for (int i = 0; i < FIELD_N; i++) {
        const FieldObj& q = s_obj[i];
        if (&q == &o || !q.solid || q.type == OBJ_NONE) continue;
        float dx = q.x - o.x, dy = q.y - o.y, m = (q.radius + o.radius) * 0.8f + 0.3f;
        if (dx * dx + dy * dy < m * m) return true;
    }
    return false;
}
static void spawnClear(FieldObj& o, float ang, float range) {
    spawnObj(o, ang, range);
    if (overlaps(o)) { o.type = OBJ_NONE; o.solid = false; }
}

// ---------------- creatures ----------------
struct Habit { float speed, minAlt, maxAlt, life; bool floor, curious; float interval; };
static const Habit HAB[CR_KINDS] = {
    {1.6f, 2.f, 10.f, 50.f, false, true, 30.f},     // manta
    {1.0f, 1.f, 8.f, 60.f, false, true, 25.f},      // turtle
    {2.2f, 2.f, 12.f, 40.f, false, false, 35.f},    // shark
    {2.0f, 6.f, 40.f, 50.f, false, false, 110.f},   // whale
    {0.25f, 3.f, 30.f, 70.f, false, false, 14.f},   // jellies
    {0.4f, 0.f, 0.4f, 60.f, true, true, 30.f},      // octopus
    {0.6f, 1.f, 8.f, 60.f, false, false, 30.f},     // anglerfish
    {2.0f, 2.f, 14.f, 45.f, false, false, 70.f},    // serpent
    {1.8f, 3.f, 20.f, 45.f, false, true, 30.f},     // narwhal
    {0.3f, 0.f, 0.1f, 60.f, true, false, 14.f},     // crabs
    {0.2f, 0.f, 0.1f, 60.f, true, false, 22.f},     // isopod
    {0.f, 0.f, 0.f, 60.f, true, false, 20.f},       // tripod fish
    {0.4f, 1.f, 6.f, 60.f, false, true, 26.f},      // nautilus
    {0.8f, 1.f, 6.f, 60.f, false, false, 40.f},     // coelacanth
    {4.0f, 3.f, 14.f, 30.f, false, true, 40.f},     // dolphins
    {1.0f, 0.3f, 3.f, 50.f, false, false, 28.f},    // eel
};
static float weightOf(const BiomeParams& b, uint8_t k) {
    switch (k) {
        case CR_MANTA: return b.manta; case CR_TURTLE: return b.turtle; case CR_SHARK: return b.shark; case CR_WHALE: return b.whale;
        case CR_JELLY: return b.jelly; case CR_OCTOPUS: return b.octopus; case CR_ANGLER: return b.angler; case CR_SERPENT: return b.serpent;
        case CR_NARWHAL: return b.narwhal; case CR_CRAB: return b.crab; case CR_ISOPOD: return b.isopod; case CR_TRIPOD: return b.tripod;
        case CR_NAUTILUS: return b.nautilus; case CR_COELACANTH: return b.coelacanth; case CR_DOLPHIN: return b.dolphin; case CR_EEL: return b.eel;
    }
    return 0.f;
}
static float habitZ(const Habit& h, float x, float y) {
    float fl = terrainFloor(x, y), ce = fminf(terrainCeiling(x, y), 0.f) - 1.5f;
    float z;
    if (h.floor) z = fl + h.minAlt + frand() * (h.maxAlt - h.minAlt) + 0.05f;
    else {
        z = s_pz + (frand() - 0.5f) * 10.f;                          // near your depth, where you'll meet it
        z = clampf(z, fl + h.minAlt, fl + h.maxAlt + fmaxf(0.f, s_pz - fl) * 0.6f);
    }
    return fminf(z, ce);
}
static void newTarget(Critter& c, bool near) {
    const Habit& h = HAB[c.kind];
    float a = frand() * kTau, r = near ? 8.f + frand() * 18.f : 18.f + frand() * 20.f;
    c.tx = (near ? s_px : c.x) + r * cosf(a);
    c.ty = (near ? s_py : c.y) + r * sinf(a);
    c.tz = habitZ(h, c.tx, c.ty);
    c.timer = 6.f + frand() * 10.f;
}
static void spawnCritter(Critter& c, uint8_t kind, float yaw) {
    const Habit& h = HAB[kind];
    memset(&c, 0, sizeof(c));
    c.on = true; c.kind = kind; c.seed = hash32(s_rng ^ kind * 0x9E37u); frand();
    float ang = yaw + (frand() - 0.5f) * 2.4f;                          // somewhere ahead-ish, out in the blue
    float r = kind == CR_WHALE ? 48.f + frand() * 10.f : (h.floor ? 14.f + frand() * 16.f : 24.f + frand() * 14.f);
    c.x = s_px + r * cosf(ang); c.y = s_py + r * sinf(ang);
    c.z = habitZ(h, c.x, c.y);
    c.size = 1.f + (frand() - 0.5f) * 0.4f;
    c.count = kind == CR_JELLY ? 4 + (uint8_t)(frand() * 6.f) : kind == CR_CRAB ? 3 + (uint8_t)(frand() * 4.f) : kind == CR_DOLPHIN ? 2 + (uint8_t)(frand() * 3.f) : 1;
    if (g_scene.jellies > 0.3f && kind == CR_JELLY) c.count = 9;
    c.phase = frand() * kTau;
    c.mood = MOOD_WANDER;
    if (kind == CR_WHALE || kind == CR_SERPENT) {                         // the big ones pass by, across your bow
        c.mood = MOOD_PASS;
        float side = frand() < 0.5f ? 1.f : -1.f;
        c.tx = c.x + cosf(yaw + side * 1.57f) * 120.f; c.ty = c.y + sinf(yaw + side * 1.57f) * 120.f; c.tz = c.z;
        if (kind == CR_WHALE) sfxCritter(CRIT_WHALE, r);
    } else newTarget(c, true);
    c.t0 = g_scene.t;
}

static void tickCritters(float yaw, float t, float dt, float speed) {
    const BiomeParams& b = biomeAt(s_D);
    for (int k = 0; k < CR_KINDS; k++) {
        float w = weightOf(b, (uint8_t)k);
        if (w <= 0.01f) continue;
        s_next[k] -= dt * w;
        if (s_next[k] > 0.f) continue;
        s_next[k] = HAB[k].interval * (0.6f + frand() * 0.8f);
        bool dup = false;
        int free = -1;
        for (int i = 0; i < CRIT_N; i++) { if (s_cr[i].on && s_cr[i].kind == k) dup = true; if (!s_cr[i].on && free < 0) free = i; }
        if (dup || free < 0) continue;
        if (k == CR_TRIPOD || k == CR_CRAB || k == CR_ISOPOD || k == CR_OCTOPUS) {   // floor dwellers: only if the floor is near
            if (s_pz - terrainFloor(s_px, s_py) > 30.f) continue;
        }
        spawnCritter(s_cr[free], (uint8_t)k, yaw);
    }
    for (int i = 0; i < CRIT_N; i++) {
        Critter& c = s_cr[i];
        if (!c.on) continue;
        const Habit& h = HAB[c.kind];
        float dx = c.x - s_px, dy = c.y - s_py, dz = c.z - s_pz;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        float age = t - c.t0;
        if (d > 80.f || (age > 4.f && d > 62.f && c.mood != MOOD_PASS)) { c.on = false; continue; }
        if (c.mood == MOOD_PASS && age > 70.f) { c.on = false; continue; }
        // moods: bolt from a fast close pass, come and look if you're still and they're the curious kind
        if (c.mood != MOOD_PASS) {
            if (d < 4.f + speed * 0.9f && speed > 2.5f && c.mood != MOOD_FLEE && c.kind != CR_TRIPOD) {
                c.mood = MOOD_FLEE; c.timer = 3.f;
                float l = fmaxf(0.1f, sqrtf(dx * dx + dy * dy));
                c.tx = c.x + dx / l * 25.f; c.ty = c.y + dy / l * 25.f; c.tz = c.z + (h.floor ? 0.f : 2.f);
            } else if (h.curious && g_scene.stillFor > 3.f && c.mood == MOOD_WANDER && d < 30.f) {
                c.mood = MOOD_CURIOUS; c.timer = 10.f + frand() * 8.f;
            }
            if (c.mood == MOOD_CURIOUS) {
                float l = fmaxf(0.1f, d);
                c.tx = s_px + dx / l * 5.f; c.ty = s_py + dy / l * 5.f; c.tz = h.floor ? c.z : s_pz + dz / l * 2.f;
                if (g_scene.stillFor < 0.5f) { c.mood = MOOD_WANDER; newTarget(c, true); }
            }
            c.timer -= dt;
            if (c.timer <= 0.f) {
                if (age > h.life) { c.mood = MOOD_PASS; float a = atan2f(dy, dx); c.tx = c.x + cosf(a) * 90.f; c.ty = c.y + sinf(a) * 90.f; c.tz = c.z; }
                else { c.mood = MOOD_WANDER; newTarget(c, true); }
            }
        }
        // steer toward the target, with a body's inertia
        float ex = c.tx - c.x, ey = c.ty - c.y, ez = c.tz - c.z;
        float el = sqrtf(ex * ex + ey * ey + ez * ez);
        float sp = h.speed * (c.mood == MOOD_FLEE ? 2.6f : 1.f);
        if (el < 1.f && c.mood == MOOD_WANDER) { newTarget(c, true); }
        float wx = el > 0.01f ? ex / el * sp : 0.f, wy = el > 0.01f ? ey / el * sp : 0.f, wz = el > 0.01f ? ez / el * sp * 0.5f : 0.f;
        if (c.kind == CR_JELLY) { wz += 0.12f * sinf(t * 0.9f + c.phase); }
        float k = 1.f - expf(-dt / (c.kind == CR_DOLPHIN ? 0.6f : 1.5f));
        c.vx += (wx - c.vx) * k; c.vy += (wy - c.vy) * k; c.vz += (wz - c.vz) * k;
        c.x += c.vx * dt; c.y += c.vy * dt; c.z += c.vz * dt;
        float fl = terrainFloor(c.x, c.y);
        if (h.floor) c.z = fl + (c.kind == CR_OCTOPUS ? 0.15f : 0.05f);
        else c.z = clampf(c.z, fl + fmaxf(0.6f, h.minAlt * 0.5f), fminf(terrainCeiling(c.x, c.y), 0.f) - 1.2f);
        c.phase += dt * (0.5f + sqrtf(c.vx * c.vx + c.vy * c.vy) * 1.5f);
    }
}

// ---------------- schools ----------------
static void spawnSchool(School& s, uint8_t kind, float yaw, bool bait) {
    memset(&s, 0, sizeof(s));
    s.on = true; s.kind = kind; s.bait = bait; s.seed = hash32(s_rng ^ 0x5C00u); frand();
    s.n = bait ? FISH_N : (uint8_t)(10 + frand() * (FISH_N - 10));
    float ang = yaw + (frand() - 0.5f) * 1.8f, r = 22.f + frand() * 10.f;
    s.x = s_px + r * cosf(ang); s.y = s_py + r * sinf(ang);
    float fl = terrainFloor(s.x, s.y);
    s.z = clampf(kind == SCH_REEF ? fl + 1.5f + frand() * 4.f : s_pz + (frand() - 0.5f) * 6.f, fl + 1.f, fminf(terrainCeiling(s.x, s.y), 0.f) - 1.5f);
    s.spread = bait ? 2.8f : 2.5f + frand() * 2.5f;
    s.tx = s.x; s.ty = s.y; s.tz = s.z;
    s.t0 = g_scene.t;
    for (int i = 0; i < s.n; i++) {
        s.fx[i] = s.x + (frand() - 0.5f) * s.spread * 2.f; s.fy[i] = s.y + (frand() - 0.5f) * s.spread * 2.f; s.fz[i] = s.z + (frand() - 0.5f) * s.spread;
        s.fvx[i] = s.fvy[i] = 0.f;
    }
}
static void tickSchools(float yaw, float t, float dt, float speed) {
    const BiomeParams& b = biomeAt(s_D);
    s_schoolNext -= dt;
    if (s_schoolNext <= 0.f) {
        s_schoolNext = 6.f + frand() * 8.f;
        float w[SCH_KINDS] = {b.fishReef, b.fishSilver, b.fishGlow, b.fishDark};
        float sum = w[0] + w[1] + w[2] + w[3];
        int free = -1;
        for (int i = 0; i < SCHOOL_N; i++) if (!s_sc[i].on) { free = i; break; }
        if (free >= 0 && sum > 0.05f) {
            float r = frand() * sum;
            uint8_t k = 0;
            for (; k < SCH_KINDS - 1; k++) { r -= w[k]; if (r <= 0.f) break; }
            bool bait = k == SCH_SILVER && g_scene.zone < 0.6f && g_scene.dayLight > 0.4f && (g_scene.daySeed & 3u) != 0u && frand() < 0.4f;
            spawnSchool(s_sc[free], k, yaw, bait);
        }
    }
    for (int si = 0; si < SCHOOL_N; si++) {
        School& s = s_sc[si];
        if (!s.on) continue;
        float dx = s.x - s_px, dy = s.y - s_py, dz = s.z - s_pz;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > 75.f || t - s.t0 > 120.f) { s.on = false; continue; }
        // the school wanders around you; a bait ball holds station and wheels
        float ex = s.tx - s.x, ey = s.ty - s.y;
        if (ex * ex + ey * ey < 4.f || frand() < dt * 0.05f) {
            float a = frand() * kTau, r = 10.f + frand() * 18.f;
            s.tx = s_px + r * cosf(a); s.ty = s_py + r * sinf(a);
            float fl = terrainFloor(s.tx, s.ty);
            s.tz = clampf(s.kind == SCH_REEF ? fl + 2.f + frand() * 3.f : s.z + (frand() - 0.5f) * 4.f, fl + 1.f, fminf(terrainCeiling(s.tx, s.ty), 0.f) - 1.5f);
        }
        float sp = s.bait ? 0.3f : (s.kind == SCH_SILVER ? 1.6f : 0.9f);
        float el = sqrtf(ex * ex + ey * ey) + 1e-3f;
        float k = 1.f - expf(-dt / 2.f);
        s.vx += (ex / el * sp - s.vx) * k; s.vy += (ey / el * sp - s.vy) * k; s.vz += ((s.tz - s.z) * 0.2f - s.vz) * k;
        s.x += s.vx * dt; s.y += s.vy * dt; s.z += s.vz * dt;
        for (int i = 0; i < s.n; i++) {
            uint32_t h = hash32(s.seed + i * 0x9E37u);
            float ph = (h & 1023) / 1023.f * kTau, ra = 0.4f + ((h >> 10) & 255) / 255.f;
            float ox, oy, oz;
            if (s.bait) {                                              // a wheeling sphere
                float w = t * (0.9f + 0.3f * ((h >> 20) & 3)) + ph;
                float tilt = ((h >> 18) & 255) / 255.f * 2.f - 1.f;
                ox = cosf(w) * s.spread * ra; oy = sinf(w) * s.spread * ra; oz = tilt * s.spread * 0.8f + sinf(w * 0.7f) * 0.5f;
            } else {                                                   // a loose drifting shoal
                ox = cosf(ph + t * 0.25f) * s.spread * ra; oy = sinf(ph * 1.3f + t * 0.21f) * s.spread * ra; oz = sinf(ph * 2.1f + t * 0.33f) * s.spread * 0.4f;
            }
            float gx = s.x + ox, gy = s.y + oy, gz = s.z + oz;
            // shy of the sub
            float fx = s.fx[i] - s_px, fy = s.fy[i] - s_py, fz = s.fz[i] - s_pz;
            float fd = sqrtf(fx * fx + fy * fy + fz * fz);
            float scare = clampf((3.5f + speed * 0.4f - fd) / 2.5f, 0.f, 1.f);
            if (scare > 0.f && fd > 0.01f) { gx += fx / fd * 5.f * scare; gy += fy / fd * 5.f * scare; gz += fz / fd * 2.f * scare; }
            float kk = 1.f - expf(-dt / (s.bait ? 0.25f : 0.6f));
            float vx = (gx - s.fx[i]) * 1.2f, vy = (gy - s.fy[i]) * 1.2f;
            s.fvx[i] += (vx - s.fvx[i]) * kk; s.fvy[i] += (vy - s.fvy[i]) * kk;
            s.fx[i] += s.fvx[i] * dt; s.fy[i] += s.fvy[i] * dt;
            s.fz[i] += (gz - s.fz[i]) * kk;
        }
    }
}

// ---------------- the field ----------------
void fieldInit(float x, float y, float yaw, float D) {
    s_rng = hash32(worldSeed() ^ 0xF1E1Du);
    s_px = x; s_py = y; s_pz = -6.f; s_D = D;
    for (int i = 0; i < FIELD_N; i++) { s_obj[i].type = OBJ_NONE; s_obj[i].solid = false; }
    for (int i = 0; i < FIELD_N; i++) {
        float ang = frand() * kTau, rng = FIELD_R * sqrtf(frand());
        if (rng < 2.f) rng = 2.f + frand() * 4.f;
        spawnClear(s_obj[i], ang, rng);
        FieldObj& o = s_obj[i];                                       // nothing solid on top of you at the start
        if (o.solid && sqrtf((o.x - x) * (o.x - x) + (o.y - y) * (o.y - y)) < o.radius + 4.f) { o.type = OBJ_NONE; o.solid = false; }
    }
    for (int i = 0; i < CRIT_N; i++) s_cr[i].on = false;
    for (int i = 0; i < SCHOOL_N; i++) s_sc[i].on = false;
    for (int k = 0; k < CR_KINDS; k++) s_next[k] = 3.f + frand() * HAB[k].interval * 0.5f;
    s_schoolNext = 1.5f;
    (void)yaw;
}

void fieldTick(float x, float y, float z, float yaw, float D, float t, float dt, float speed) {
    s_px = x; s_py = y; s_pz = z; s_D = D;
    for (int k = 0; k < OBJ_TYPES; k++) s_nearest[k] = 999.f;
    // forget what's out of range; re-dream it at the far edge ahead
    int budget = 24;
    for (int i = 0; i < FIELD_N; i++) {
        FieldObj& o = s_obj[i];
        float dx = o.x - x, dy = o.y - y;
        float d2 = dx * dx + dy * dy;
        if (d2 > FIELD_R * FIELD_R) {
            if (budget-- <= 0) { o.type = OBJ_NONE; o.solid = false; continue; }
            float ang = yaw + (frand() - 0.5f) * 3.2f;
            spawnClear(o, ang, FIELD_R * (0.86f + 0.13f * frand()));
            continue;
        }
        if (o.type != OBJ_NONE) { float d = sqrtf(d2); if (d < s_nearest[o.type]) s_nearest[o.type] = d; }
    }
    tickCritters(yaw, t, dt, speed);
    tickSchools(yaw, t, dt, speed);
}

float fieldMove(float& x, float& y, float& z, float dx, float dy, float dz, float* bump) {
    *bump = 0.f;
    float hl = sqrtf(dx * dx + dy * dy);
    float nx = x + dx, ny = y + dy, nz = z + dz;
    float clear = HULL_R * 0.9f;
    // the seabed: a gentle slope lifts you over it; a wall stops you
    float fl = terrainFloor(nx, ny) + clear;
    if (nz < fl) {
        float rise = fl - nz;
        if (rise <= hl * 1.4f + 0.04f) nz = fl;
        else {
            float stopped = fminf(1.f, (rise - hl * 1.4f) * 4.f);
            nx = x; ny = y;
            nz = fmaxf(nz, terrainFloor(x, y) + clear);
            *bump = fmaxf(*bump, stopped * fminf(1.f, (hl + fabsf(dz)) * 8.f));
        }
    }
    // the surface, or the ice shelf
    float ce = fminf(terrainCeiling(nx, ny), 0.f) - (terrainCeiling(nx, ny) < 0.f ? HULL_R : 0.4f);
    if (nz > ce) {
        if (terrainCeiling(nx, ny) < 0.f && dz > 0.02f) *bump = fmaxf(*bump, fminf(1.f, dz * 6.f));
        nz = ce;
    }
    // solid things: glance off the side, stop dead head-on
    float hx = hl > 1e-5f ? dx / hl : 0.f, hy = hl > 1e-5f ? dy / hl : 0.f;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < FIELD_N; i++) {
            const FieldObj& o = s_obj[i];
            if (!o.solid || o.type == OBJ_NONE) continue;
            if (nz > o.z + o.top + HULL_R * 0.6f) continue;               // over the top of it
            float minD = o.radius + HULL_R;
            float ox = o.x - nx, oy = o.y - ny;
            float d2 = ox * ox + oy * oy;
            if (d2 >= minD * minD) continue;
            float d = sqrtf(d2) + 1e-4f;
            float nnx = ox / d, nny = oy / d;
            float facing = hx * nnx + hy * nny;
            if (facing > 0.82f && hl > 1e-4f) { nx = x; ny = y; *bump = fmaxf(*bump, fminf(1.f, hl * 10.f)); }
            else { float push = minD - d; nx -= nnx * push; ny -= nny * push; *bump = fmaxf(*bump, hl > 0.05f ? 0.15f : 0.f); }
        }
    }
    float moved = sqrtf((nx - x) * (nx - x) + (ny - y) * (ny - y) + (nz - z) * (nz - z));
    float want = sqrtf(dx * dx + dy * dy + dz * dz);
    x = nx; y = ny; z = nz;
    return want > 1e-6f ? fminf(1.f, moved / want) : 1.f;
}

const FieldObj* fieldObjects() { return s_obj; }
const Critter* fieldCritters() { return s_cr; }
const School* fieldSchools() { return s_sc; }
float fieldNearest(uint8_t t) { return t < OBJ_TYPES ? s_nearest[t] : 999.f; }
float fieldWhaleNear() {
    for (int i = 0; i < CRIT_N; i++) {
        const Critter& c = s_cr[i];
        if (!c.on || c.kind != CR_WHALE) continue;
        float dx = c.x - s_px, dy = c.y - s_py;
        return clampf(1.2f - sqrtf(dx * dx + dy * dy) / 60.f, 0.f, 1.f);
    }
    return 0.f;
}
float fieldBaitBall() {
    for (int i = 0; i < SCHOOL_N; i++) {
        const School& s = s_sc[i];
        if (!s.on || !s.bait) continue;
        float dx = s.x - s_px, dy = s.y - s_py;
        return clampf(1.3f - sqrtf(dx * dx + dy * dy) / 30.f, 0.f, 1.f);
    }
    return 0.f;
}
void fieldScare(float x, float y, float z, float radius) {
    for (int i = 0; i < CRIT_N; i++) {
        Critter& c = s_cr[i];
        if (!c.on || c.mood == MOOD_PASS || c.kind == CR_TRIPOD || c.kind == CR_WHALE) continue;
        float dx = c.x - x, dy = c.y - y, dz = c.z - z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > radius) continue;
        c.mood = MOOD_FLEE; c.timer = 2.5f;
        float l = fmaxf(0.1f, sqrtf(dx * dx + dy * dy));
        c.tx = c.x + dx / l * 20.f; c.ty = c.y + dy / l * 20.f;
    }
}

}

#ifdef HOST
namespace mantis {
// desktop-only: put a creature or a school in front of the camera for a look check
void fieldDebugSpawn(uint8_t kind, float yaw, float dist, float dz) {
    for (int i = 0; i < CRIT_N; i++) if (!s_cr[i].on) {
        spawnCritter(s_cr[i], kind, yaw);
        Critter& c = s_cr[i];
        c.x = s_px + cosf(yaw) * dist; c.y = s_py + sinf(yaw) * dist;
        float fl = terrainFloor(c.x, c.y);
        c.z = HAB[kind].floor ? fl + 0.05f : fmaxf(fl + 1.5f, s_pz + dz);
        c.tx = c.x + sinf(yaw) * 30.f; c.ty = c.y - cosf(yaw) * 30.f; c.tz = c.z;
        c.vx = sinf(yaw) * HAB[kind].speed; c.vy = -cosf(yaw) * HAB[kind].speed;
        c.mood = MOOD_PASS; c.t0 = g_scene.t;
        return;
    }
}
void fieldDebugSchool(uint8_t kind, float yaw, float dist, bool bait) {
    for (int i = 0; i < SCHOOL_N; i++) if (!s_sc[i].on) {
        spawnSchool(s_sc[i], kind, yaw, bait);
        School& s = s_sc[i];
        float nx = s_px + cosf(yaw) * dist, ny = s_py + sinf(yaw) * dist;
        float dx = nx - s.x, dy = ny - s.y, dz = s_pz - s.z;
        s.x += dx; s.y += dy; s.z += dz; s.tx = s.x; s.ty = s.y; s.tz = s.z;
        for (int k = 0; k < s.n; k++) { s.fx[k] += dx; s.fy[k] += dy; s.fz[k] += dz; }
        return;
    }
}
void fieldDebugClear() { for (int i = 0; i < CRIT_N; i++) s_cr[i].on = false; for (int i = 0; i < SCHOOL_N; i++) s_sc[i].on = false; for (int k = 0; k < CR_KINDS; k++) s_next[k] = 999.f; s_schoolNext = 999.f; }
}
#endif
