// Soak: a bot pilot flies the real firmware loop for a while on several seeds. Fails on NaN, on ending up
// inside the seabed or above the surface, on getting stuck, or if the depth director never takes you deeper.
#include "harness_core.inc"
#include <chrono>
using namespace mantis;
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
int main(int argc, char** argv) {
  int minutes = argc > 1 ? atoi(argv[1]) : 6;
  setup();
  for (int i = 0; i < 30; i++) frame();                // hold still while the helm takes your neutral pose
  int fails = 0; bool reachedDeep = false;
  double renderMs = 0; long frames = 0;
  for (int seed = 1; seed <= 3; seed++) {
    universeInit(seed * 7919u); journeyInit(worldSeed(), 0.f);
    distanceTravelled = 0.f; subX = subY = 0.f;
    terrainInit(0, 0, 0); subZ = fminf(-4.f, terrainFloor(0, 0) * 0.5f);
    fieldInit(0, 0, 0, 0); helmInit(0.f); helmSetThrottle(0.75f);
    bool seen[B_ALL_N] = {false}; int maxZone = 0; float maxDepth = 0, stuck = 0; float lastD = 0;
    int N = minutes * 60 * 30;
    for (int i = 0; i < N; i++) {
      // the pilot: hug the seabed for a while (to sink a zone), wander the heading, climb when it's close
      float alt = g_scene.altitude;
      float wantAlt = (i / 9000) % 2 == 0 ? 4.f : 25.f;
      float wantPitch = clampf(-(alt - wantAlt) * 0.05f, -0.6f, 0.6f);     // nose toward the altitude you want
      float stickY = clampf((helmPitch() - wantPitch) * 1.2f, -0.4f, 0.4f); // + = dive
      float stickX = 0.18f * sinf(i * 0.0021f + seed);
      if (stuck > 1.f) { stickX = 0.45f; stickY = -0.3f; }
      auto t0 = std::chrono::high_resolution_clock::now();
      frame(stickX, stickY);
      renderMs += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count(); frames++;
      if (!(subX == subX) || !(subZ == subZ) || !(helmYaw() == helmYaw())) { printf("FAIL seed %d: NaN at frame %d\n", seed, i); fails++; break; }
      float fl = terrainFloor(subX, subY);
      if (subZ < fl + 0.3f) { printf("FAIL seed %d: inside the seabed (z %.2f floor %.2f) at %d\n", seed, subZ, fl, i); fails++; break; }
      if (subZ > 0.01f) { printf("FAIL seed %d: above the surface at %d\n", seed, i); fails++; break; }
      seen[g_scene.biome] = true;
      maxZone = maxZone > (int)(g_scene.zone + 0.5f) ? maxZone : (int)(g_scene.zone + 0.5f);
      maxDepth = fmaxf(maxDepth, g_scene.depth);
      if (i % 30 == 0) { stuck = distanceTravelled - lastD < 0.5f ? stuck + 1.f : 0.f; lastD = distanceTravelled; }
      if (stuck > 20.f) { printf("FAIL seed %d: stuck for 20 s at %d (alt %.1f)\n", seed, i, alt); fails++; break; }
    }
    int nb = 0; for (int b = 0; b < B_ALL_N; b++) nb += seen[b];
    printf("seed %d: %.2f km, %d biomes, deepest zone %d, deepest %.0f m, databank %d\n", seed, distanceTravelled / 1000.f, nb, maxZone, maxDepth, databankCount());
    if (maxZone >= 2) reachedDeep = true;
  }
  if (!reachedDeep) { printf("FAIL: the depth director never took the pilot to the midnight zone\n"); fails++; }
  printf("host frame %.2f ms avg (render + sim + audio)\n", renderMs / frames);
  printf(fails ? "SOAK FAILED\n" : "SOAK PASS\n");
  return fails ? 1 : 0;
}
