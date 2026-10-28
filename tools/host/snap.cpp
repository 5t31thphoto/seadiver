// Look check: every biome, its light and its creatures, through the real firmware loop -> snaps/*.ppm
// (render.py makes PNGs and a contact sheet). "./snap audio" also writes listening previews.
#include "harness_core.inc"
#include <chrono>
using namespace mantis;

static void settle(int frames) { for (int i = 0; i < frames; i++) frame(); }
static void place(uint8_t biome, float tod, float alt, float yaw, float pitch, float D0 = 400.f) {
  g_force = SceneForce();
  distanceTravelled = D0;
  journeyUpdate(D0, 0.f, 0.f, 10.f);               // catch the journey up first: crossing biomes re-picks the ones ahead
  journeyForce(D0, biome);
  journeyForce(biomeEnd(D0) + 1.f, biome);
  journeyForce(biomeStart(D0) - 1.f, biome);
  setTimeOfDay(tod, millis() / 1000.f, D0);
  subX = 1000.f + D0 * 0.37f; subY = -300.f + D0 * 0.11f;
  terrainInit(subX, subY, D0);
  float fl = terrainFloor(subX, subY);
  subZ = fminf(fl + alt, terrainCeiling(subX, subY) - 1.5f);
  if (subZ > -1.5f) subZ = -1.5f;
  helmSetAttitude(yaw, pitch);
  helmSetThrottle(0.f); subSpeed = 0.f;
  fieldInit(subX, subY, yaw, D0);
  fieldDebugClear();
  lightsAuto = true;
  settle(70);
  helmSetAttitude(yaw, pitch);
}
static void shot(const char* name) {
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 10; i++) frame();
  double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count() / 10.0;
  if (getenv("PERF")) printf("  %-18s %.2f ms/frame\n", name, ms);
  char p[64]; snprintf(p, sizeof p, "snaps/%s.ppm", name); writePPM(p);
  const Scene& s = g_scene;
  printf("%-18s %-20s depth %5.1f alt %5.1f light %.2f vis-ish beams %.2f awe %.2f\n", name, biomeName(s.biome), s.depth, s.altitude, s.light, s.beams, s.awe);
}
static void crit(uint8_t k, float rel, float dist, float dz) { fieldDebugSpawn(k, helmYaw() + rel, dist, dz); }

int main(int argc, char** argv) {
  bool audio = argc > 1 && !strcmp(argv[1], "audio");
  setup();
  settle(220);                                                     // past the intro
  place(B_REEF, 0.45f, 5.f, 0.3f, -0.12f);   fieldDebugSchool(SCH_REEF, helmYaw(), 9.f, false); crit(CR_TURTLE, 0.25f, 12.f, 1.f); settle(3); shot("01_coral_gardens");
  place(B_KELP, 0.36f, 3.f, 1.1f, 0.32f);    fieldDebugSchool(SCH_SILVER, helmYaw() - 0.2f, 14.f, false); settle(3); shot("02_kelp_cathedral");
  place(B_SEAGRASS, 0.55f, 3.f, 2.2f, -0.1f); crit(CR_MANTA, -0.15f, 10.f, 1.f); crit(CR_SHARK, 0.3f, 20.f, 0.f); settle(3); shot("03_seagrass");
  place(B_DROPOFF, 0.5f, 6.f, 0.7f, -0.25f); crit(CR_JELLY, 0.f, 9.f, 0.f); crit(CR_OCTOPUS, -0.3f, 7.f, 0.f); settle(3); shot("04_drop_off");
  place(B_CANYON, 0.5f, 10.f, 4.0f, -0.1f);  crit(CR_ANGLER, 0.1f, 6.f, -1.f); crit(CR_COELACANTH, -0.25f, 11.f, 0.f); settle(3); shot("05_light_canyon");
  place(B_BONEREEF, 0.5f, 5.f, 3.1f, -0.12f); crit(CR_JELLY, 0.1f, 8.f, 1.f); settle(3); shot("06_bone_reef");
  place(B_LANTERN, 0.5f, 4.f, 0.5f, -0.15f); fieldDebugSchool(SCH_GLOW, helmYaw(), 8.f, false); crit(CR_ANGLER, -0.2f, 7.f, 0.f); settle(3); shot("07_lantern_garden");
  place(B_ABYSS, 0.5f, 3.f, 5.2f, -0.2f);    crit(CR_TRIPOD, 0.f, 6.f, 0.f); crit(CR_ISOPOD, -0.3f, 5.f, 0.f); crit(CR_WHALE, 0.2f, 45.f, 8.f); settle(3); shot("08_abyssal_plain");
  place(B_VENTS, 0.5f, 6.f, 1.9f, -0.15f);   crit(CR_CRAB, 0.f, 7.f, 0.f); settle(3); shot("09_black_smokers");
  place(B_RUINS, 0.31f, 5.f, 0.f, 0.05f);    fieldDebugSchool(SCH_REEF, helmYaw() + 0.2f, 12.f, false); crit(CR_NAUTILUS, -0.15f, 6.f, 0.f); settle(40); shot("10_sunken_city");
  place(B_VOLCANO, 0.5f, 14.f, 2.6f, -0.2f); g_force.eruption = 1.f; settle(20); shot("11_fire_beneath");
  place(B_JELLY, 0.5f, 120.f, 0.9f, 0.1f);   crit(CR_JELLY, 0.f, 8.f, 0.f); crit(CR_JELLY, 0.3f, 14.f, 3.f); crit(CR_JELLY, -0.3f, 12.f, -2.f); settle(3); shot("12_jelly_cathedral");
  place(B_WRECK, 0.5f, 6.f, 3.6f, -0.1f);    crit(CR_EEL, 0.15f, 7.f, 0.f); fieldDebugSchool(SCH_SILVER, helmYaw() - 0.3f, 15.f, false); settle(3); shot("13_iron_graveyard");
  place(B_BRINE, 0.5f, 7.f, 4.4f, -0.3f);    settle(3); shot("14_brine_lakes");
  place(B_CRYSTAL, 0.5f, 6.f, 0.2f, -0.1f);  fieldDebugSchool(SCH_GLOW, helmYaw(), 10.f, false); settle(3); shot("15_amethyst_deep");
  place(B_POLAR, 0.45f, 20.f, 1.4f, 0.25f);  crit(CR_NARWHAL, 0.1f, 12.f, 2.f); settle(3); shot("16_under_the_ice");
  place(B_LAIR, 0.5f, 6.f, 2.9f, -0.05f);    crit(CR_SERPENT, 0.f, 14.f, 2.f); settle(3); shot("17_leviathan_hall");
  place(B_REEF, 0.95f, 5.f, 0.9f, -0.1f);    helmSetThrottle(0.8f); subSpeed = 7.f; g_force.bloom = 1.f; settle(20); shot("18_night_bloom");
  place(B_SEAGRASS, 0.5f, 4.f, 0.4f, 0.f);   fieldDebugSchool(SCH_SILVER, helmYaw(), 10.f, true); crit(CR_DOLPHIN, 0.3f, 14.f, 0.f); settle(40); shot("19_bait_ball");
  place(B_KELP, 0.72f, 6.f, -0.4f, 0.4f);    settle(3); shot("20_kelp_dusk");
  // the cards
  place(B_REEF, 0.45f, 5.f, 0.3f, -0.12f); crit(CR_TURTLE, 0.f, 6.f, 0.f); settle(2);
  databankAdd(16); databankAdd(20);
  cardOpen(CARD_SCAN, CR_TURTLE, true); cardT = 4.5f; shot("21_card_scan");
  cardClose(); cardOpen(CARD_BANK, 0, false); shot("22_card_databank");
  cardClose(); cardOpen(CARD_CHART, 0, false); shot("23_card_chart");
  cardClose();
  if (audio) {
    auto listen = [](const char* name, int frames) { g_wav.clear(); g_recordAudio = true; settle(frames); g_recordAudio = false; char p[64]; snprintf(p, sizeof p, "snaps/%s.wav", name); writeWav(p); };
    place(B_REEF, 0.45f, 5.f, 0.3f, 0.f); helmSetThrottle(0.5f); listen("audio_reef_cruise", 600);
    place(B_KELP, 0.4f, 6.f, 0.3f, 0.f); helmSetThrottle(0.5f); crit(CR_WHALE, 0.f, 40.f, 0.f); sfxCritter(CRIT_WHALE, 40.f); listen("audio_whale", 450);
    place(B_POLAR, 0.4f, 20.f, 0.3f, 0.f); listen("audio_under_ice", 450);
    place(B_VENTS, 0.4f, 6.f, 0.3f, 0.f); sonarPing(); listen("audio_vents_sonar", 300);
  }
  return 0;
}
