#pragma once
#include <stdint.h>

// The sound of the sea: the water itself, the sub, the life around you and a generative ambient-dub score,
// synthesized live at 24 kHz on core 0 and streamed to the Core2 speaker. The speaker can't do bass, so the
// motor plays the low end (see haptics.*).

namespace mantis {

constexpr int   AUDIO_SR = 24000;
constexpr float MUSIC_BPM = 84.f;

enum : uint8_t { MOMENT_INTRO, MOMENT_BIOME, MOMENT_AURORA, MOMENT_BLOOM, MOMENT_ERUPT, MOMENT_PRISM, MOMENT_DEEPER };
enum : uint8_t { CRIT_WHALE, CRIT_DOLPHIN, CRIT_SERPENT, CRIT_BOLT, CRIT_NARWHAL };
enum : uint8_t { UI_SCAN_TICK, UI_SCAN_OK, UI_SCAN_NEW, UI_LIGHTS_ON, UI_LIGHTS_OFF, UI_CARD, UI_LEVEL, UI_CRUISE, UI_CREAK, UI_DETENT };

void audioSpeakerBegin();   // right after M5.begin(), before other allocations
void audioBegin();

// game -> audio (lock-free, safe from the main loop)
void sfxBump(float strength);
void sfxThunder(float delaySec, float power);
void sfxCritter(uint8_t kind, float dist);
void sfxMoment(uint8_t kind);
void sfxUi(uint8_t kind);
void sfxSonar(float echoDelaySec, float echoStrength);

// audio -> game
float audioBeats();     // musical beats as heard right now (output latency compensated)
float audioKick();      // 0..1: the groove has a kick (the motor plays it on the beat)
float audioGroove();

void audioSetMuted(bool muted);
bool audioMuted();
void audioRender(int16_t* out, int n);

}
