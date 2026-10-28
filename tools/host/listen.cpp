// A listening tour through the real firmware's sound -> snaps/tour.wav (tools/host/listen.sh makes an MP3).
#include "harness_core.inc"
using namespace mantis;
static void place(uint8_t biome, float tod, float alt, float yaw, float D0) {
  distanceTravelled = D0;
  journeyUpdate(D0, 0.f, 0.f, 10.f);               // catch the journey up first: crossing biomes re-picks the ones ahead
  journeyForce(D0, biome); journeyForce(biomeEnd(D0) + 1.f, biome); journeyForce(biomeStart(D0) - 1.f, biome);
  setTimeOfDay(tod, millis() / 1000.f, D0);
  subX = 1000.f + D0 * 0.37f; subY = -300.f + D0 * 0.11f;
  terrainInit(subX, subY, D0);
  subZ = fminf(terrainFloor(subX, subY) + alt, terrainCeiling(subX, subY) - 1.5f);
  if (subZ > -1.5f) subZ = -1.5f;
  helmSetAttitude(yaw, 0.f); fieldInit(subX, subY, yaw, D0);
}
static void run(float sec, float throttle) { helmSetThrottle(throttle); int n = (int)(sec * 30.f); for (int i = 0; i < n; i++) frame(0.03f * sinf(i * 0.02f), 0.f); }
static std::vector<size_t> g_cuts;
static void cut() { g_cuts.push_back(g_wav.size()); }
int main() {
  g_recordAudio = true;
  setup();
  run(9.f, 0.f);                                                      // the hatch seals, the tanks flood, the dive begins
  cut(); place(B_REEF, 0.45f, 5.f, 0.3f, 400.f); run(5.f, 0.6f);      // a reef cruise: shrimp, bubbles, the screw
  for (int k = 0; k < 8; k++) { sfxUi(UI_SCAN_TICK); run(0.15f, 0.6f); }
  sfxUi(UI_SCAN_NEW); run(8.f, 0.6f);                                 // ...a new entry in the databank
  cut(); place(B_KELP, 0.4f, 8.f, 1.f, 1400.f); sfxCritter(CRIT_WHALE, 30.f); run(13.f, 0.4f);   // a whale in the kelp
  cut(); place(B_POLAR, 0.4f, 20.f, 1.f, 2400.f); run(2.f, 0.3f); sfxCritter(CRIT_NARWHAL, 15.f); run(10.f, 0.3f);   // under the ice
  cut(); place(B_VENTS, 0.5f, 6.f, 1.f, 3400.f); run(2.f, 0.3f); sonarPing(); run(5.f, 0.3f); sonarPing(); run(4.f, 0.3f);   // sonar over the vents
  cut(); place(B_LAIR, 0.5f, 8.f, 1.f, 4400.f); run(2.f, 0.2f); sfxCritter(CRIT_SERPENT, 20.f); run(9.f, 0.2f);   // the leviathan
  cut();
  // a short crossfade at each change of place, so the tour doesn't click
  const size_t X = 12000;
  for (size_t c : g_cuts) {
    if (c < X || c + X > g_wav.size()) continue;
    for (size_t i = 0; i < X; i++) { float g = (float)i / X; g_wav[c - X + i] = (int16_t)(g_wav[c - X + i] * (1.f - g * 0.85f)); g_wav[c + i] = (int16_t)(g_wav[c + i] * (0.15f + 0.85f * g)); }
  }
  writeWav("snaps/tour.wav");
  printf("%.1f s\n", g_wav.size() / (float)AUDIO_SR);
  return 0;
}
