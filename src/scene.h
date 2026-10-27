#pragma once
#include <stdint.h>

// One shared picture of "what the sea is doing right now", written by the main loop and read by the
// renderer and the audio task. Floats only (32-bit stores are atomic on the ESP32).

namespace mantis {

struct Scene {
    float t, dt, tod, yaw, pitch, distance, speed, throttle;
    float x, y, z;                      // where the sub is (z up, the surface at 0)
    float depth, altitude, ceiling;     // metres below the surface, above the seabed; the ice above (0 = open sea)
    float cruiseFor, stillFor;          // seconds of steady cruising / of drifting still
    float sunElev, sunAz;

    // the weather, far above
    float cloud, rain, storm, waves, flash, boltAge;
    // light
    float dayLight;                     // how bright the surface is (0 night .. 1 noon)
    float light;                        // how much of it reaches you (0 .. 1)

    // around you
    uint8_t biome;
    float zone;                         // 0 sunlit .. 2 midnight (blended)
    float shrimp, whaleSong, iceSong, ventRumble, eerie, hush, glow, snow, murk, surge, kelpNear, ventNear, lavaNear;

    // moments
    float beams, aurora, bloom, eruption, prism, whale, baitball, jellies, intro, awe;
    float sonarAge, sonarRange;         // seconds since the last ping; the sweep's reach
    bool lights;
    uint32_t daySeed;
};

extern Scene g_scene;

// Overrides for previews / tests (negative = automatic).
struct SceneForce { float cloud = -1, rain = -1, storm = -1, aurora = -1, bloom = -1, eruption = -1, prism = -1; };
extern SceneForce g_force;

void sceneInit();
void sceneUpdate(float t, float dt, float x, float y, float z, float yaw, float pitch, float distance, float speed, float throttle);

struct Thunder { float at, power; bool pending; };
extern Thunder g_thunder;

}
