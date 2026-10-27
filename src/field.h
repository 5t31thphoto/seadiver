#pragma once
#include <stdint.h>

// The near field: everything you can swim up to. It lives in world coordinates around you (radius FIELD_R).
// Whatever falls out behind is forgotten and re-dreamt at the far edge ahead, from the biome at that
// distance along the journey, so the only thing that persists is what is around you right now.

namespace mantis {

enum ObjType : uint8_t {
    OBJ_NONE, OBJ_KELP, OBJ_GRASS, OBJ_CORAL, OBJ_ANEMONE, OBJ_SPONGE, OBJ_ROCK, OBJ_PILLAR, OBJ_BASALT,
    OBJ_VENT, OBJ_RUIN, OBJ_MONOLITH, OBJ_WRECK, OBJ_ICE, OBJ_CRYSTAL, OBJ_DEADTREE, OBJ_BONES, OBJ_CLAM,
    OBJ_DETAIL, OBJ_TYPES
};

struct FieldObj {
    float x, y, z;     // world position of its foot on the seabed
    float a, b;        // sizes (height, width ... per type)
    float radius;      // collision radius (solid things)
    float top;         // collision height above its foot
    uint32_t seed;
    uint8_t type, var;
    bool solid;
};

enum : uint8_t {
    CR_MANTA, CR_TURTLE, CR_SHARK, CR_WHALE, CR_JELLY, CR_OCTOPUS, CR_ANGLER, CR_SERPENT, CR_NARWHAL, CR_CRAB,
    CR_ISOPOD, CR_TRIPOD, CR_NAUTILUS, CR_COELACANTH, CR_DOLPHIN, CR_EEL, CR_KINDS
};
struct Critter {
    bool on;
    uint8_t kind, mood, count;       // count: a group (jellies, dolphins, crabs)
    float x, y, z, vx, vy, vz;
    float tx, ty, tz;                // where it's heading
    float size, phase, t0, timer;
    uint32_t seed;
    bool faceRight;                  // set by the renderer from how it moves across the view
};
enum : uint8_t { MOOD_WANDER, MOOD_FLEE, MOOD_CURIOUS, MOOD_PASS };
constexpr int CRIT_N = 6;

enum : uint8_t { SCH_REEF, SCH_SILVER, SCH_GLOW, SCH_DARK, SCH_KINDS };
constexpr int SCHOOL_N = 3, FISH_N = 22;
struct School {
    bool on;
    uint8_t kind, n;
    bool bait;                       // a bait ball: wheeling in a tight sphere
    float x, y, z, vx, vy, vz, tx, ty, tz;
    float spread, t0;
    uint32_t seed;
    float fx[FISH_N], fy[FISH_N], fz[FISH_N], fvx[FISH_N], fvy[FISH_N];   // each fish, world
};

constexpr int   FIELD_N = 260;
constexpr float FIELD_R = 46.f;
constexpr float HULL_R  = 1.1f;      // the sub, as a sphere

void fieldInit(float x, float y, float yaw, float D);
// Forget what's behind, dream what's ahead, move every creature. `D` = distance travelled.
void fieldTick(float x, float y, float z, float yaw, float D, float t, float dt, float speed);
// Move the sub by (dx, dy, dz), meeting the seabed, the surface or ice, and anything solid. Returns the
// fraction of the move that happened; *bump = how hard something stopped you (0 = nothing).
float fieldMove(float& x, float& y, float& z, float dx, float dy, float dz, float* bump);

const FieldObj* fieldObjects();
const Critter*  fieldCritters();
const School*   fieldSchools();
float fieldNearest(uint8_t objType);     // metres to the nearest of a type (999 if none)
float fieldWhaleNear();                  // 0..1: a whale is passing
float fieldBaitBall();                   // 0..1: a bait ball is wheeling nearby
void  fieldScare(float x, float y, float z, float radius);   // sonar, a bump: things nearby bolt

}
