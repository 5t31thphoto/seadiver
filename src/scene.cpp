// Size over speed here: this code runs per object or per frame, not per pixel or per sample, and the app shares
// flash with others (see README, "Footprint").
#pragma GCC optimize("Os")
#include "scene.h"
#include "universe.h"
#include "audio.h"
#include "field.h"
#include "journey.h"
#include "terrain.h"
#include <math.h>

namespace mantis {

Scene g_scene;
SceneForce g_force;
Thunder g_thunder = {0, 0, false};

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float smoothstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }
static inline float follow(float cur, float target, float dt, float tau) { return cur + (target - cur) * clampf(dt / tau, 0.f, 1.f); }
static inline float bump(float x, float a, float b, float c, float d) { return smoothstep(a, b, x) * (1.f - smoothstep(c, d, x)); }
static uint32_t s_rng = 0xA511E9B3u;
static float frand() { s_rng = hash32(s_rng + 0x9E3779B9u); return (s_rng >> 8) * (1.f / 16777216.f); }
static float s_eruptNext = 25.f, s_eruptT = -100.f;
static uint8_t s_lastBiome = 255;

void sceneInit() {
    Scene& s = g_scene;
    s = Scene();
    s.cloud = 0.3f; s.boltAge = 99.f; s.intro = 1.f; s.sonarAge = 99.f; s.lights = true;
    s_rng = hash32(worldSeed() ^ 0x5CE7Eu);
    s_lastBiome = 255;
    s_eruptNext = 25.f; s_eruptT = -100.f;
}

void sceneUpdate(float t, float dt, float x, float y, float z, float yaw, float pitch, float distance, float speed, float throttle) {
    Scene& s = g_scene;
    s.t = t; s.dt = dt; s.yaw = yaw; s.pitch = pitch; s.distance = distance; s.speed = speed; s.throttle = throttle;
    s.x = x; s.y = y; s.z = z;
    s.depth = -z;
    float floorZ = terrainFloor(x, y);
    s.altitude = z - floorZ;
    s.ceiling = terrainCeiling(x, y);
    float clock = dayClock(t, distance);
    s.tod = clock - floorf(clock);
    s.sunElev = sunPathElevation(s.tod);
    s.sunAz = AZ_EAST + (s.tod - 0.245f) * kTau;
    s.daySeed = hash32((uint32_t)floorf(clock) ^ worldSeed());
    const BiomeParams& here = biomeAt(distance);
    s.biome = journeyBiome();
    if (s.biome != s_lastBiome) {
        if (s_lastBiome != 255 && t > 5.f) sfxMoment(MOMENT_BIOME);
        s_lastBiome = s.biome;
    }

    // ---------- you ----------
    if (speed > 0.6f) { s.cruiseFor = fminf(s.cruiseFor + dt, 90.f); s.stillFor = 0.f; }
    else { s.stillFor += dt; s.cruiseFor = fmaxf(0.f, s.cruiseFor - dt * (s.stillFor > 4.f ? 2.5f : 0.25f)); }

    // ---------- the weather far above: a slow field over distance and time ----------
    float u = distance * 0.0012f + t * 0.0011f;
    float v = clampf((fbmP(0.f, u, 1, 3, 0x7EA7u) - 0.28f) / 0.46f, 0.f, 1.f);
    float settle = smoothstep(150.f, 300.f, t);
    float cloudT = clampf(0.12f + v * 1.05f, 0.f, 1.f);
    if (t < 300.f) cloudT = fminf(cloudT, 0.45f + 0.55f * settle);
    float rainT = smoothstep(0.62f, 0.78f, v) * settle;
    float stormT = smoothstep(0.84f, 0.97f, v) * settle;
    if (g_force.cloud >= 0) cloudT = g_force.cloud;
    if (g_force.rain >= 0) rainT = g_force.rain;
    if (g_force.storm >= 0) stormT = g_force.storm;
    s.cloud = follow(s.cloud, cloudT, dt, 12.f);
    s.rain = follow(s.rain, rainT, dt, 8.f);
    s.storm = follow(s.storm, stormT, dt, 10.f);
    if (g_force.cloud >= 0) s.cloud = g_force.cloud;
    if (g_force.rain >= 0) s.rain = g_force.rain;
    if (g_force.storm >= 0) s.storm = g_force.storm;
    s.waves = 0.25f + 0.35f * fbmP(0.f, u * 3.f + 5.f, 1, 2, 0x3C1Du) + s.rain * 0.25f + s.storm * 0.6f;

    // lightning: seen from below as the whole surface flashing, heard as a muffled roll
    s.boltAge += dt;
    float nearSurf = clampf(1.f - s.depth / 70.f, 0.f, 1.f);
    if (s.storm > 0.25f && frand() < s.storm * 0.18f * dt) {
        s.boltAge = 0.f;
        float delay = 0.4f + frand() * 3.5f, power = (0.45f + 0.55f * s.storm) * nearSurf;
        if (power > 0.05f) { g_thunder = {t + delay, power, true}; sfxThunder(delay, power); }
    }
    {
        float e = s.boltAge;
        float f = e < 0.05f ? 1.f : e < 0.1f ? 0.25f : e < 0.16f ? 0.85f : expf(-(e - 0.16f) * 10.f);
        s.flash = e < 2.f ? f * nearSurf * nearSurf : 0.f;
    }

    // ---------- light ----------
    float clearSky = clampf(1.f - (s.cloud - 0.45f) * 2.f, 0.f, 1.f) * (1.f - 0.7f * s.rain);
    s.dayLight = smoothstep(-0.12f, 0.45f, s.sunElev) * (0.55f + 0.45f * clearSky);
    float reach = here.absG * 1.4f;
    s.light = s.dayLight * expf(-fmaxf(0.f, s.depth) / fmaxf(5.f, reach));

    // ---------- around you ----------
    s.zone = here.zone;
    s.shrimp = here.shrimp; s.whaleSong = here.whaleSong; s.iceSong = here.iceSong; s.ventRumble = here.ventRumble;
    s.eerie = here.eerie; s.hush = here.hush; s.glow = here.glowAmt; s.snow = here.snow; s.murk = here.murk; s.surge = here.surge;
    float kd = fieldNearest(OBJ_KELP), vd = fieldNearest(OBJ_VENT);
    s.kelpNear = follow(s.kelpNear, kd < 40.f ? 1.f / (1.f + kd / 6.f) : 0.f, dt, 1.f);
    s.ventNear = follow(s.ventNear, vd < 60.f ? 1.f / (1.f + vd / 10.f) : 0.f, dt, 1.f);

    // ---------- moments ----------
    uint32_t ds = s.daySeed;
    bool night = s.tod > 0.84f || s.tod < 0.16f;
    float nightK = night ? clampf(fminf(fabsf(s.tod - 0.84f), fabsf(s.tod - 0.16f)) * 20.f, 0.f, 1.f) : 0.f;
    // light shafts: the sun up, the sky clear, near enough to the surface to see them
    float lowSun = clampf(1.f - s.sunElev / 0.9f, 0.3f, 1.f);
    s.beams = smoothstep(0.02f, 0.2f, s.sunElev) * clearSky * clampf(1.f - s.depth / 90.f, 0.f, 1.f) * (0.4f + 0.6f * here.shafts) * lowSun * (1.f - s.intro);
    // aurora: polar nights, through the leads in the ice and the surface's window
    float auT = ((ds >> 3) % 10u < (uint32_t)(2 + 6 * here.auroraBoost) ? 1.f : 0.f) * nightK * clampf(1.f - (s.cloud - 0.4f) * 3.f, 0.f, 1.f) * clampf(1.f - s.depth / 60.f, 0.f, 1.f);
    if (here.auroraBoost < 0.3f) auT *= 0.4f;
    if (g_force.aurora >= 0) auT = g_force.aurora;
    float prevA = s.aurora;
    s.aurora = follow(s.aurora, auT, dt, 6.f);
    if (prevA < 0.3f && s.aurora >= 0.3f) sfxMoment(MOMENT_AURORA);
    // a bioluminescent bloom: some nights the plankton light up wherever anything moves
    float blT = fmaxf(night && ((ds >> 6) % 3u) != 0u ? nightK : 0.f, clampf(here.glowAmt * 0.6f + (s.depth > 160.f ? 0.4f : 0.f), 0.f, 1.f));
    if (g_force.bloom >= 0) blT = g_force.bloom;
    float prevB = s.bloom;
    s.bloom = follow(s.bloom, blT, dt, 4.f);
    if (prevB < 0.5f && s.bloom >= 0.5f && night) sfxMoment(MOMENT_BLOOM);
    // the volcano breathes now and then
    if (here.volcano > 0.5f) {
        s_eruptNext -= dt;
        if (s_eruptNext <= 0.f) { s_eruptT = t; s_eruptNext = 40.f + frand() * 50.f; sfxMoment(MOMENT_ERUPT); }
    }
    float erT = (t - s_eruptT) < 14.f ? bump(t - s_eruptT, 0.f, 1.5f, 9.f, 14.f) : 0.f;
    if (g_force.eruption >= 0) erT = g_force.eruption;
    s.eruption = follow(s.eruption, erT, dt, 0.6f);
    // prism light over the ruins: the low sun splits into colours
    float prT = here.prism * smoothstep(0.03f, 0.12f, s.sunElev) * (1.f - smoothstep(0.55f, 0.8f, s.sunElev)) * clearSky * clampf(1.f - s.depth / 70.f, 0.f, 1.f);
    if (g_force.prism >= 0) prT = g_force.prism;
    float prevP = s.prism;
    s.prism = follow(s.prism, prT, dt, 4.f);
    if (prevP < 0.35f && s.prism >= 0.35f) sfxMoment(MOMENT_PRISM);
    s.jellies = clampf(here.jelly - 1.f, 0.f, 1.f);
    s.whale = follow(s.whale, fieldWhaleNear(), dt, 2.f);
    s.baitball = follow(s.baitball, fieldBaitBall(), dt, 2.f);
    s.sonarAge += dt;

    s.intro = clampf(1.f - t / 6.f, 0.f, 1.f);
    float cathedral = s.beams * clampf(s.kelpNear * 2.f, 0.f, 1.f);
    s.awe = fmaxf(fmaxf(cathedral * 0.8f, s.aurora * 0.9f), fmaxf(fmaxf(s.prism, s.whale), fmaxf(s.eruption * 0.8f, fmaxf(s.jellies * 0.6f, s.baitball * 0.7f))));
    if (night && s.bloom > 0.5f && speed > 1.f) s.awe = fmaxf(s.awe, 0.5f);
}

}
