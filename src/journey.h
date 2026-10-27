#pragma once
#include <stdint.h>

// The journey: biomes are a sequence along your path, not a map. Each lasts ~0.7-1.1 km and blends into the
// next (the seabed shelves down or rises up over the blend). Nothing is stored beyond the journey you're on.
//
// Two directors choose what comes next:
//  * Depth. Ordinary biomes live in three zones: sunlit, twilight and midnight. Cruise low through a biome,
//    near its floor, and the next one lies a zone deeper; climb toward the light and the world rises to meet
//    you. Hold the middle of the water and it stays in the zone you're in.
//  * Heading. Hold roughly one compass direction through three biomes and the next is that direction's
//    special biome (as in MantisPlanet). After a special, back to an ordinary one.
//
// Compass (world azimuth): east = 0, south = +pi/2, west = pi, north = -pi/2 (bearings grow to your right).

namespace mantis {

enum : uint8_t {
    B_REEF, B_KELP, B_SEAGRASS,          // sunlit zone
    B_DROPOFF, B_CANYON, B_BONEREEF,     // twilight zone
    B_LANTERN, B_ABYSS, B_VENTS,         // midnight zone
    B_ORDINARY_N,
    B_RUINS = B_ORDINARY_N,              // east: the sunken city, light breaking into colours
    B_VOLCANO,                           // southeast: fire beneath the sea
    B_JELLY,                             // south: the jellyfish cathedral
    B_WRECK,                             // southwest: the iron graveyard
    B_BRINE,                             // west: lakes under the sea
    B_CRYSTAL,                           // northwest: amethyst caverns
    B_POLAR,                             // north: under the ice shelf
    B_LAIR,                              // northeast: the leviathan's hall
    B_ALL_N
};

// Floats only: two biomes blend by lerping every field.
struct BiomeParams {
    // ---- the seabed ----
    float floorBase;          // metres below the surface
    float mounds, dunes, ridges, cliffs, canyon, terrace, cones, pools;   // relief, metres (terrace: step height)
    float rockMix;            // how much of the floor is rock (material B) rather than sediment (material A)
    // ---- the water ----
    float vis;                // metres you can see
    float absR, absG, absB;   // light's reach per colour, metres (red goes first)
    float water[3][3];        // the water's own colour: deep, middle, bright
    float caust, shafts, snow, murk, surge;   // caustics, light shafts, marine snow, particles, swell sway
    float iceCeiling;         // the surface is an ice shelf
    float glowAmt;            // bioluminescence everywhere
    float lava, plumes, brine, prism, auroraBoost, bubbles;
    // ---- colours (shadow -> highlight) ----
    float sed[4][3], rock[4][3], flora[4][3], stone[4][3], coral[4][3], glow[3];
    // ---- what grows and stands here ----
    float kelp, grass, coral_, sponge, anemone, rocks, pillars, basalt, vents, ruins, wreck, ice, crystals, deadtrees,
          bones, clams, glowcoral, monolith, details, volcano;
    // ---- who lives here ----
    float fishReef, fishSilver, fishGlow, fishDark, jelly, manta, turtle, shark, whale, octopus, angler, serpent,
          narwhal, crab, isopod, tripod, nautilus, coelacanth, dolphin, eel;
    // ---- sound ----
    float shrimp, whaleSong, iceSong, ventRumble, eerie, hush;
    float zone;               // 0 sunlit, 1 twilight, 2 midnight
};

void journeyInit(uint32_t seed, float distance);
// Every frame: the distance you covered, the heading you covered it in, and how deep you were.
void journeyUpdate(float distance, float yaw, float moved, float depth);
// Blended biome parameters at a distance along the journey (cheap; called a lot).
const BiomeParams& biomeAt(float D);
uint8_t journeyBiome();          // the biome you're in
uint8_t biomeIdAt(float D);      // which biome a distance falls in
float biomeStart(float D);       // where the biome containing D begins
float biomeEnd(float D);         // where it ends
void journeyForce(float D, uint8_t b);   // previews / tests
uint32_t journeyGeneration();    // bumps whenever the biome plan changes
int journeyStreak();
int journeyOctant();             // 0 = E, 1 = SE, 2 = S, 3 = SW, 4 = W, 5 = NW, 6 = N, 7 = NE
int journeyNextZoneWish();       // -1 rising, 0 holding, +1 sinking (from how deep you've been cruising)
const char* biomeName(uint8_t b);
const BiomeParams& biomeDef(uint8_t b);

}
