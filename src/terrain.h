#pragma once
#include <stdint.h>

// The seabed: a heightfield you can fly down to and touch. It's kept only around you, in a 128 x 128 grid of
// 2 m cells (+-128 m) that scrolls as you travel: a cell that comes into range is dreamt then, from the biome
// at (distance travelled + its range), exactly as MantisPlanet dreams its near field at the far edge. Behind
// you it's forgotten. Heights are in decimetres (z up, the surface at 0).

namespace mantis {

constexpr int   TG_N = 128;              // cells per side (power of two)
constexpr int   TG_MASK = TG_N - 1;
constexpr float TG_CELL = 2.f;           // metres per cell

// material byte: bits 0-2 light (0..7), bits 3-4 rockiness (0..3), bits 5-7 feature
enum : uint8_t { TF_NONE = 0, TF_GRASS = 1, TF_LAVA = 2, TF_BRINE = 3, TF_GLOW = 4, TF_SNOW = 5, TF_RUBBLE = 6, TF_TUBES = 7 };
inline uint8_t tmShade(uint8_t m) { return m & 7; }
inline uint8_t tmRock(uint8_t m) { return (m >> 3) & 3; }
inline uint8_t tmFeat(uint8_t m) { return m >> 5; }

void terrainInit(float px, float py, float D);
void terrainUpdate(float px, float py, float D);   // scrolls and dreams new cells (bounded work per call)

// The seabed height under a point (bilinear), metres. Outside the grid, the shape function directly.
float terrainFloor(float x, float y);
// The underside of the ice shelf (under the ice), or 0 (the open surface).
float terrainCeiling(float x, float y);
// Shape function itself (tests, spawning beyond the grid).
float terrainShape(float x, float y, float D, uint8_t* mat);

// Raw access for the renderer's hot loop: cell (ix, iy) lives at [(iy & MASK) * N + (ix & MASK)].
const int16_t* terrainH();
const uint8_t* terrainM();
int terrainOx();                         // grid origin, in cells
int terrainOy();
uint32_t terrainGeneration();            // bumps when cells change

}
