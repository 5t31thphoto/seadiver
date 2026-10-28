// Stress: the camera right up against every kind of thing (and every creature), timing each frame.
// The heaviest frames are what you feel as a freeze on the device.
#include "harness_core.inc"
#include <chrono>
using namespace mantis;
namespace mantis { void fieldDebugObj(uint8_t, uint8_t, float, float); }
static const char* ON[] = {"none","kelp","grass","coral","anemone","sponge","rock","pillar","basalt","vent","ruin","monolith","wreck","ice","crystal","deadtree","bones","clam","detail"};
static const char* CN[] = {"manta","turtle","shark","whale","jelly","octopus","angler","serpent","narwhal","crab","isopod","tripod","nautilus","coelacanth","dolphin","eel"};
static double timeFrames(int n) { auto t0 = std::chrono::high_resolution_clock::now(); for (int i = 0; i < n; i++) renderFrame(subX, subY, subZ, helmYaw(), helmPitch(), millis() / 1000.f); return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count() / n; }
int main() {
  setup(); for (int i = 0; i < 220; i++) frame();
  helmSetThrottle(0.f); subSpeed = 0.f;
  fieldDebugClear();
  float yaw = helmYaw();
  double base = 0;
  { fieldDebugObj(OBJ_GRASS, 0, yaw, 40.f); base = timeFrames(20); printf("baseline (nothing near)          %6.2f ms\n", base); }
  double worst = 0; const char* worstName = "";
  for (int t = 1; t < OBJ_TYPES; t++)
    for (int v = 0; v < 6; v++)
      for (float d : {0.6f, 1.5f, 3.f}) {
        for (float pz : {-0.5f, 0.5f, 2.f}) {                       // level with its foot, just above, over the top
          fieldDebugObj((uint8_t)t, (uint8_t)(t == OBJ_CORAL ? v : (v % 6)), yaw, d);
          float fl = terrainFloor(subX, subY);
          subZ = fl + 1.2f + pz;
          for (float pitch : {-0.9f, 0.f}) {
            helmSetAttitude(yaw, pitch);
            double ms = timeFrames(3);
            if (ms > worst) { worst = ms; worstName = ON[t]; }
            if (ms > base * 3.f) printf("  SLOW %-9s var %d  at %.1f m, %+.1f m, pitch %+.1f: %6.2f ms (%.0fx)\n", ON[t], v, d, pz, pitch, ms, ms / base);
          }
        }
        if (t != OBJ_CORAL && t != OBJ_RUIN && t != OBJ_WRECK && t != OBJ_DETAIL && t != OBJ_SPONGE && v > 0) break;
      }
  fieldDebugClear();
  for (int k = 0; k < CR_KINDS; k++)
    for (float d : {0.8f, 2.f, 5.f}) {
      fieldDebugObj(OBJ_GRASS, 0, yaw, 40.f);
      fieldDebugClear(); fieldDebugSpawn((uint8_t)k, yaw, d, 0.f);
      helmSetAttitude(yaw, 0.f);
      double ms = timeFrames(3);
      if (ms > worst) { worst = ms; worstName = CN[k]; }
      if (ms > base * 3.f) printf("  SLOW %-9s at %.1f m: %6.2f ms (%.0fx)\n", CN[k], d, ms, ms / base);
    }
  printf("worst frame: %.2f ms (%s), %.0fx the baseline\n", worst, worstName, worst / base);
  return 0;
}
