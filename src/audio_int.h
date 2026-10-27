#pragma once
#include "dsp.h"
#include "scene.h"
#include <stdint.h>

namespace mantis {
namespace snd {

constexpr int BLOCK = 512;          // ~21 ms: enough slack that a busy frame can't starve the speaker
struct Bus { float dry[BLOCK], rev[BLOCK], dly[BLOCK]; };

extern dsp::Rng rng;
extern uint32_t samples;

// ocean.cpp
void oceanInit();
void oceanBlock(Bus& b, int n, const Scene& s);
void oceanBump(float strength);
void oceanThunder(float delaySec, float power);
void oceanCritter(uint8_t kind, float dist);
void oceanUi(uint8_t kind);
void oceanSonar(float delay, float strength);
void oceanMoment(uint8_t kind);

// music.cpp
void musicInit();
void musicBlock(Bus& b, int n, const Scene& s);
void musicMoment(uint8_t kind);
float musicKick();
float musicGroove();
float musicDelaySamples();

}
}
