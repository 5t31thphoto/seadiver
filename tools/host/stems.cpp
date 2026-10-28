// Soloed stems: every sound in the engine on its own, in a setting where it really occurs, with its reverb and
// echo -> snaps/stems/NN_name.wav. tools/host/stems.sh turns them into MP3s.
#include "harness_core.inc"
#include "audio_int.h"
using namespace mantis;
namespace mantis { namespace snd { void musicDebugPassage(uint8_t); uint8_t musicDebugAllVoices(); } }
static void place(uint8_t biome, float tod, float alt, float yaw, float D0) {
  g_force = SceneForce();
  distanceTravelled = D0;
  journeyUpdate(D0, 0.f, 0.f, 10.f);               // catch the journey up first: crossing biomes re-picks the ones ahead
  journeyForce(D0, biome); journeyForce(biomeEnd(D0) + 1.f, biome); journeyForce(biomeStart(D0) - 1.f, biome);
  setTimeOfDay(tod, millis() / 1000.f, D0);
  subX = 1000.f + D0 * 0.37f; subY = -300.f + D0 * 0.11f;
  terrainInit(subX, subY, D0);
  subZ = fminf(terrainFloor(subX, subY) + alt, terrainCeiling(subX, subY) - 1.5f);
  if (subZ > -1.5f) subZ = -1.5f;
  helmSetAttitude(yaw, 0.f); helmSetThrottle(0.f); subSpeed = 0.f;
  fieldInit(subX, subY, yaw, D0); fieldDebugClear();
}
static void run(float sec, float stickY = 0.f) { int n = (int)(sec * 30.f); for (int i = 0; i < n; i++) frame(0.f, stickY); }
static int g_n = 0;
static void stem(int tag, const char* name, void (*scene)()) {
  snd::g_solo = -1;
  scene();                                          // set the stage with everything audible (its setup is not recorded)
  snd::g_solo = tag;
  run(1.5f);                                        // let anything already ringing from the setup die away
  g_wav.clear(); g_recordAudio = true;
  extern void (*g_act)(); g_act();
  g_recordAudio = false;
  double s = 0; int pk = 0; for (int16_t v : g_wav) { s += (double)v * v; if (abs(v) > pk) pk = abs(v); }
  char p[96]; snprintf(p, sizeof p, "snaps/stems/%02d_%s.wav", ++g_n, name); writeWav(p);
  printf("%-34s %5.1f s  rms %5.0f  peak %5d%s\n", p + 12, g_wav.size() / 24000.f, sqrt(s / fmax(1.0, (double)g_wav.size())), pk, pk < 200 ? "   << SILENT" : "");
}
void (*g_act)() = nullptr;
#define STEM(tag, name, SCENE, ACT) do { g_act = [] { ACT; }; stem(tag, name, [] { SCENE; }); } while (0)

int main() {
  setup();
  run(8.f);
  // ---------------- the sea ----------------
  STEM(snd::TAG_SEA, "sea_breath", place(B_KELP, 0.45f, 10.f, 0.f, 400.f), run(14.f));
  STEM(snd::TAG_WASH, "surface_wash_storm", (place(B_SEAGRASS, 0.5f, 6.f, 0.f, 1400.f), subZ = -2.f, g_force.storm = 0.7f, g_force.rain = 0.6f, run(10.f)), run(14.f));
  STEM(snd::TAG_RAIN, "rain_on_the_surface", (place(B_SEAGRASS, 0.5f, 6.f, 0.f, 2400.f), subZ = -2.f, g_force.rain = 1.f, run(10.f)), run(12.f));
  STEM(snd::TAG_THUNDER, "thunder_far_above", place(B_SEAGRASS, 0.5f, 6.f, 0.f, 3400.f), (sfxThunder(0.3f, 0.6f), run(5.f), sfxThunder(0.6f, 1.f), run(7.f)));
  STEM(snd::TAG_VENT, "vent_rumble", (place(B_VENTS, 0.5f, 3.f, 0.f, 4400.f), run(4.f)), run(12.f));
  STEM(snd::TAG_ERUPT, "volcano_eruption", place(B_VOLCANO, 0.5f, 10.f, 0.f, 5400.f), (sfxMoment(MOMENT_ERUPT), run(9.f)));
  STEM(snd::TAG_CHIME, "crystal_and_ice_chimes", (place(B_CRYSTAL, 0.5f, 6.f, 0.f, 6400.f), run(2.f)), run(20.f));
  STEM(snd::TAG_EERIE, "abyss_groans_and_pings", place(B_ABYSS, 0.5f, 6.f, 0.f, 7400.f), (sfxCritter(CRIT_EERIE, 20.f), run(6.f), sfxCritter(CRIT_EERIE, 20.f), run(6.f), sfxCritter(CRIT_EERIE, 20.f), run(6.f)));
  // ---------------- the sub ----------------
  STEM(snd::TAG_ENGINE, "screw_stop_cruise_flank", place(B_REEF, 0.45f, 8.f, 0.f, 8400.f), (helmSetThrottle(0.5f), run(7.f), helmSetThrottle(1.f), run(8.f), helmSetThrottle(0.f), run(4.f)));
  STEM(snd::TAG_BUBBLES, "bubbles_diving_and_climbing", (place(B_REEF, 0.45f, 8.f, 0.f, 9400.f), helmSetThrottle(0.5f), run(3.f)), (run(4.f, 0.3f), run(4.f, -0.3f), run(4.f, 0.3f)));
  STEM(snd::TAG_TRIM, "trim_tanks_venting_B", place(B_REEF, 0.45f, 8.f, 0.f, 10400.f), (sfxUi(UI_LEVEL), run(2.f), sfxUi(UI_LEVEL), run(2.f)));
  STEM(snd::TAG_CREAK, "hull_creaks_at_depth", place(B_ABYSS, 0.5f, 6.f, 0.f, 11400.f), (sfxUi(UI_CREAK), run(3.f), sfxUi(UI_CREAK), run(3.f), sfxUi(UI_CREAK), run(3.f)));
  STEM(snd::TAG_BUMP, "hull_bump_soft_to_hard", place(B_REEF, 0.45f, 8.f, 0.f, 12400.f), (sfxBump(0.2f), run(1.8f), sfxBump(0.6f), run(1.8f), sfxBump(1.f), run(2.5f)));
  STEM(snd::TAG_SONAR, "sonar_ping_and_echo", place(B_DROPOFF, 0.5f, 6.f, 0.f, 13400.f), (sonarPing(), run(4.f), sonarPing(), run(4.f)));
  STEM(snd::TAG_SCAN, "scanner_logged_then_new_entry", place(B_REEF, 0.45f, 8.f, 0.f, 14400.f),
       ({ for (int k = 0; k < 9; k++) { sfxUi(UI_SCAN_TICK); run(0.14f); } sfxUi(UI_SCAN_OK); run(2.f);
          for (int k = 0; k < 9; k++) { sfxUi(UI_SCAN_TICK); run(0.14f); } sfxUi(UI_SCAN_NEW); run(3.f); }));
  STEM(snd::TAG_UI, "ui_lights_card_cruise_detent", place(B_REEF, 0.45f, 8.f, 0.f, 15400.f),
       (sfxUi(UI_LIGHTS_ON), run(1.2f), sfxUi(UI_LIGHTS_OFF), run(1.2f), sfxUi(UI_CARD), run(1.2f), sfxUi(UI_CRUISE), run(1.2f), sfxUi(UI_DETENT), run(0.4f), sfxUi(UI_DETENT), run(1.5f)));
  STEM(snd::TAG_INTRO, "dive_begins_hatch_and_tanks", place(B_REEF, 0.45f, 8.f, 0.f, 16400.f), (sfxMoment(MOMENT_INTRO), run(5.f)));
  // ---------------- life ----------------
  STEM(snd::TAG_SHRIMP, "snapping_shrimp_reef_night", (place(B_REEF, 0.95f, 2.f, 0.f, 17400.f), run(2.f)), run(12.f));
  STEM(snd::TAG_WHALE, "humpback_song", place(B_KELP, 0.4f, 10.f, 0.f, 18400.f), (sfxCritter(CRIT_WHALE, 25.f), run(9.f), sfxCritter(CRIT_WHALE, 45.f), run(10.f)));
  STEM(snd::TAG_DOLPHIN, "dolphin_whistles_and_clicks", place(B_SEAGRASS, 0.5f, 6.f, 0.f, 19400.f), (sfxCritter(CRIT_DOLPHIN, 15.f), run(4.f), sfxCritter(CRIT_DOLPHIN, 30.f), run(4.f)));
  STEM(snd::TAG_NARWHAL, "narwhal_clicks_and_buzz", place(B_POLAR, 0.5f, 15.f, 0.f, 20400.f), (sfxCritter(CRIT_NARWHAL, 15.f), run(3.f), sfxCritter(CRIT_NARWHAL, 25.f), run(3.f)));
  STEM(snd::TAG_SEAL, "weddell_seal_trills", place(B_POLAR, 0.5f, 15.f, 0.f, 21400.f), (sfxCritter(CRIT_SEAL, 15.f), run(4.f), sfxCritter(CRIT_SEAL, 30.f), run(4.5f)));
  STEM(snd::TAG_SERPENT, "leviathan_growl", place(B_LAIR, 0.5f, 8.f, 0.f, 22400.f), (sfxCritter(CRIT_SERPENT, 15.f), run(5.f), sfxCritter(CRIT_SERPENT, 40.f), run(5.f)));
  STEM(snd::TAG_BOLT, "creature_darting_away", place(B_REEF, 0.45f, 8.f, 0.f, 23400.f), (sfxCritter(CRIT_BOLT, 5.f), run(1.5f), sfxCritter(CRIT_BOLT, 15.f), run(2.f)));
  // ---------------- the score: a long steady cruise for the groove, a moment of awe for the rest ----------------
  // the conductor only brings the groove in now and then; for the stems, hold a full passage with every voice
  #define GROOVE(tag, name) STEM(tag, name, (place(B_REEF, 0.45f, 8.f, 0.f, 24400.f), helmSetThrottle(0.6f), run(70.f), snd::musicDebugPassage(snd::musicDebugAllVoices()), run(10.f)), (helmSetThrottle(0.6f), run(40.f)))
  GROOVE(snd::TAG_M_DRONE, "music_drone");
  GROOVE(snd::TAG_M_PAD, "music_pad");
  GROOVE(snd::TAG_M_STAB, "music_dub_stab");
  GROOVE(snd::TAG_M_SHAKER, "music_shaker");
  GROOVE(snd::TAG_M_KICK, "music_kick");
  GROOVE(snd::TAG_M_BASS, "music_bass");
  #define AWE(tag, name) STEM(tag, name, (place(B_RUINS, 0.31f, 5.f, 0.f, 30400.f), g_force.prism = 1.f, run(20.f)), (sfxMoment(MOMENT_PRISM), run(10.f), sfxMoment(MOMENT_BIOME), run(14.f)))
  GROOVE(snd::TAG_M_PLUCK, "music_plucks");
  AWE(snd::TAG_M_BELL, "music_bells");
  AWE(snd::TAG_M_CHOIR, "music_choir");
  snd::g_solo = -1;
  return 0;
}
