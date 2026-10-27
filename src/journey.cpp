#include "journey.h"
#include "universe.h"
#include "mem.h"
#include <math.h>
#include <string.h>

namespace mantis {

static BiomeParams* DEF = nullptr;            // runtime buffer (internal RAM: biomeAt is a hot path)

static const char* const NAMES[B_ALL_N] = {
    "CORAL GARDENS", "KELP CATHEDRAL", "SEAGRASS SHALLOWS",
    "THE BLUE DROP-OFF", "LIGHT CANYON", "BONE REEF",
    "LANTERN GARDEN", "ABYSSAL PLAIN", "BLACK SMOKERS",
    "THE SUNKEN CITY", "FIRE BENEATH", "JELLYFISH CATHEDRAL", "IRON GRAVEYARD",
    "BRINE LAKES", "AMETHYST DEEP", "UNDER THE ICE", "LEVIATHAN HALL"};
const char* biomeName(uint8_t b) { return b < B_ALL_N ? NAMES[b] : ""; }

typedef int R4[4][3];
static void ramp(float (&r)[4][3], const R4& c) { for (int i = 0; i < 4; i++) for (int k = 0; k < 3; k++) r[i][k] = (float)c[i][k]; }
static void water(BiomeParams& b, int dr, int dg, int db, int mr, int mg, int mb, int br, int bg, int bb) {
    b.water[0][0] = dr; b.water[0][1] = dg; b.water[0][2] = db;
    b.water[1][0] = mr; b.water[1][1] = mg; b.water[1][2] = mb;
    b.water[2][0] = br; b.water[2][1] = bg; b.water[2][2] = bb;
}
static void absorb(BiomeParams& b, float r, float g, float bl) { b.absR = r; b.absG = g; b.absB = bl; }
static void glow(BiomeParams& b, int r, int g, int bl) { b.glow[0] = r; b.glow[1] = g; b.glow[2] = bl; }

// Shared ramps. Shadows lean cool, highlights lean warm, as in MantisPlanet's artist ramps.
static const R4 SAND_W   = {{60, 74, 96}, {138, 146, 146}, {206, 200, 172}, {248, 240, 210}};    // white coral sand
static const R4 SAND_G   = {{40, 46, 52}, {92, 92, 84}, {150, 142, 118}, {204, 194, 160}};      // grey-gold sand
static const R4 OOZE     = {{30, 30, 38}, {74, 70, 76}, {122, 116, 116}, {176, 170, 160}};      // pale abyssal ooze
static const R4 BASALT   = {{16, 16, 24}, {44, 42, 52}, {80, 76, 86}, {124, 118, 124}};
static const R4 REEFROCK = {{50, 40, 62}, {104, 86, 96}, {158, 132, 124}, {208, 186, 164}};
static const R4 GREYROCK = {{34, 38, 56}, {78, 82, 96}, {128, 130, 136}, {180, 180, 176}};
static const R4 KELP_R   = {{34, 28, 18}, {96, 72, 28}, {170, 128, 44}, {226, 196, 104}};
static const R4 GRASS_R  = {{18, 50, 40}, {46, 104, 50}, {104, 166, 66}, {178, 214, 104}};
static const R4 STONE_R  = {{40, 40, 58}, {90, 88, 100}, {146, 140, 140}, {200, 194, 180}};
static const R4 CORAL_R  = {{230, 90, 120}, {250, 150, 70}, {150, 90, 210}, {250, 220, 90}};   // pink, orange, violet, yellow

static void defineBiomes() {
    if (DEF) return;
    DEF = (BiomeParams*)memFast(sizeof(BiomeParams) * B_ALL_N);
    if (!DEF) return;
    for (int i = 0; i < B_ALL_N; i++) {
        BiomeParams& b = DEF[i];
        memset(&b, 0, sizeof(b));
        b.floorBase = 20.f; b.mounds = 4.f; b.vis = 40.f; absorb(b, 6.f, 30.f, 60.f);
        water(b, 6, 24, 48, 20, 80, 120, 100, 190, 220);
        b.caust = 0.6f; b.shafts = 0.6f; b.snow = 0.2f; b.murk = 0.2f; b.surge = 0.4f; b.rockMix = 0.25f; b.bubbles = 0.3f;
        ramp(b.sed, SAND_G); ramp(b.rock, GREYROCK); ramp(b.flora, GRASS_R); ramp(b.stone, STONE_R); ramp(b.coral, CORAL_R);
        glow(b, 80, 230, 255);
        b.details = 0.2f; b.rocks = 0.2f;
    }
    BiomeParams* b;
    // ---------------- sunlit ----------------
    // Coral gardens: white sand, coral heads in every colour, reef fish, turtles, the crackle of snapping shrimp
    b = &DEF[B_REEF];      b->zone = 0; b->floorBase = 15.f; b->mounds = 4.f; b->dunes = 0.6f; b->ridges = 1.5f; b->rockMix = 0.3f;
                           b->vis = 62.f; absorb(*b, 7.f, 42.f, 90.f); water(*b, 6, 40, 72, 18, 112, 150, 120, 226, 232);
                           b->caust = 1.f; b->shafts = 0.8f; b->snow = 0.05f; b->murk = 0.1f; b->surge = 0.5f;
                           ramp(b->sed, SAND_W); ramp(b->rock, REEFROCK);
                           b->coral_ = 1.8f; b->anemone = 0.4f; b->sponge = 0.25f; b->rocks = 0.15f; b->grass = 0.1f; b->details = 0.45f;
                           b->fishReef = 1.f; b->fishSilver = 0.3f; b->turtle = 0.5f; b->manta = 0.25f; b->shark = 0.2f; b->dolphin = 0.25f; b->octopus = 0.15f; b->eel = 0.3f;
                           b->shrimp = 1.f;
    // Kelp cathedral: golden stalks to the surface, cathedral shafts, rockfish and garibaldi, a swell that sways it all
    b = &DEF[B_KELP];      b->zone = 0; b->floorBase = 24.f; b->mounds = 6.f; b->ridges = 3.f; b->rockMix = 0.45f;
                           b->vis = 38.f; absorb(*b, 5.f, 26.f, 34.f); water(*b, 4, 26, 34, 18, 82, 80, 96, 176, 150);
                           b->caust = 0.7f; b->shafts = 1.5f; b->snow = 0.15f; b->murk = 0.25f; b->surge = 0.9f;
                           { const R4 S = {{30, 34, 36}, {74, 72, 66}, {124, 116, 96}, {178, 166, 130}}; ramp(b->sed, S); }
                           ramp(b->rock, GREYROCK); ramp(b->flora, KELP_R);
                           b->kelp = 1.f; b->rocks = 0.4f; b->grass = 0.15f; b->anemone = 0.25f; b->details = 0.3f; b->coral_ = 0.08f;
                           b->fishReef = 0.4f; b->fishSilver = 0.7f; b->shark = 0.15f; b->whale = 0.1f; b->turtle = 0.1f; b->octopus = 0.2f; b->dolphin = 0.15f;
                           b->shrimp = 0.4f; b->whaleSong = 0.2f;
    // Seagrass shallows: a bright meadow over sand, rays and turtles grazing, a shark cruising the edge
    b = &DEF[B_SEAGRASS];  b->zone = 0; b->floorBase = 11.f; b->mounds = 2.f; b->dunes = 1.f; b->rockMix = 0.12f;
                           b->vis = 52.f; absorb(*b, 6.f, 36.f, 64.f); water(*b, 8, 46, 56, 26, 118, 116, 130, 222, 198);
                           b->caust = 1.2f; b->shafts = 0.6f; b->snow = 0.08f; b->surge = 0.7f;
                           ramp(b->sed, SAND_W);
                           b->grass = 1.f; b->rocks = 0.1f; b->details = 0.45f; b->coral_ = 0.05f;
                           b->turtle = 0.8f; b->manta = 0.45f; b->shark = 0.4f; b->fishReef = 0.3f; b->fishSilver = 0.4f; b->dolphin = 0.3f; b->crab = 0.3f;
                           b->shrimp = 0.5f;
    // ---------------- twilight ----------------
    // The blue drop-off: ledges stepping down into blue, jellyfish drifting, an octopus on the rocks
    b = &DEF[B_DROPOFF];   b->zone = 1; b->floorBase = 90.f; b->cliffs = 45.f; b->ridges = 10.f; b->mounds = 8.f; b->rockMix = 0.6f;
                           b->vis = 42.f; absorb(*b, 5.f, 24.f, 56.f); water(*b, 3, 10, 30, 10, 34, 78, 44, 100, 160);
                           b->caust = 0.2f; b->shafts = 0.4f; b->snow = 0.45f; b->murk = 0.2f; b->surge = 0.2f;
                           ramp(b->rock, BASALT);
                           b->rocks = 0.5f; b->pillars = 0.2f; b->sponge = 0.3f; b->anemone = 0.2f; b->coral_ = 0.15f; b->details = 0.2f;
                           b->jelly = 0.8f; b->octopus = 0.6f; b->shark = 0.3f; b->whale = 0.25f; b->fishSilver = 0.5f; b->coelacanth = 0.2f; b->manta = 0.2f;
                           b->whaleSong = 0.6f;
    // Light canyon: a winding gorge, light falling into it from far above, an anglerfish's lure in the shade
    b = &DEF[B_CANYON];    b->zone = 1; b->floorBase = 110.f; b->canyon = 60.f; b->ridges = 14.f; b->mounds = 6.f; b->rockMix = 0.55f;
                           b->vis = 36.f; absorb(*b, 5.f, 26.f, 50.f); water(*b, 2, 14, 18, 8, 50, 56, 40, 120, 112);
                           b->caust = 0.15f; b->shafts = 1.3f; b->snow = 0.4f; b->murk = 0.25f; b->surge = 0.15f;
                           { const R4 R = {{20, 30, 34}, {48, 64, 66}, {86, 104, 100}, {130, 146, 134}}; ramp(b->rock, R); }
                           b->pillars = 0.4f; b->rocks = 0.4f; b->sponge = 0.2f; b->anemone = 0.15f;
                           b->angler = 0.4f; b->fishSilver = 0.3f; b->eel = 0.4f; b->coelacanth = 0.3f; b->jelly = 0.3f;
    // Bone reef: a bleached reef, pale and quiet, jellyfish drifting over it like ghosts
    b = &DEF[B_BONEREEF];  b->zone = 1; b->floorBase = 60.f; b->mounds = 5.f; b->ridges = 3.f; b->rockMix = 0.3f;
                           b->vis = 46.f; absorb(*b, 14.f, 40.f, 60.f); water(*b, 12, 22, 36, 46, 72, 96, 130, 160, 180);
                           b->caust = 0.3f; b->shafts = 0.7f; b->snow = 0.3f; b->surge = 0.2f;
                           ramp(b->sed, OOZE);
                           { const R4 C = {{220, 222, 226}, {200, 206, 214}, {236, 232, 220}, {184, 192, 204}}; ramp(b->coral, C); }
                           b->coral_ = 1.f; b->details = 0.3f; b->rocks = 0.15f;
                           b->jelly = 0.9f; b->fishSilver = 0.2f; b->shark = 0.1f;
                           b->hush = 0.5f; b->eerie = 0.2f;
    // ---------------- midnight ----------------
    // Lantern garden: no sunlight reaches it; the corals make their own, and the fish carry lamps
    b = &DEF[B_LANTERN];   b->zone = 2; b->floorBase = 210.f; b->mounds = 10.f; b->ridges = 8.f; b->rockMix = 0.4f;
                           b->vis = 30.f; absorb(*b, 4.f, 20.f, 40.f); water(*b, 1, 5, 14, 4, 16, 38, 14, 40, 80);
                           b->caust = 0.f; b->shafts = 0.f; b->snow = 0.6f; b->murk = 0.2f; b->surge = 0.05f; b->glowAmt = 1.f;
                           ramp(b->rock, BASALT); ramp(b->sed, OOZE);
                           { const R4 C = {{80, 240, 160}, {120, 140, 255}, {210, 90, 240}, {90, 220, 255}}; ramp(b->coral, C); }
                           b->glowcoral = 1.8f; b->anemone = 0.4f; b->rocks = 0.3f; b->details = 0.4f;
                           b->fishGlow = 1.f; b->angler = 0.5f; b->jelly = 0.4f; b->octopus = 0.2f;
                           b->eerie = 0.3f;
    // Abyssal plain: soft ooze to the end of the light, marine snow falling forever, tripod fish waiting
    b = &DEF[B_ABYSS];     b->zone = 2; b->floorBase = 320.f; b->mounds = 3.f; b->dunes = 0.4f; b->rockMix = 0.08f;
                           b->vis = 30.f; absorb(*b, 4.f, 20.f, 40.f); water(*b, 1, 3, 10, 4, 10, 26, 12, 24, 54);
                           b->caust = 0.f; b->shafts = 0.f; b->snow = 1.f; b->murk = 0.15f; b->surge = 0.f; b->glowAmt = 0.3f;
                           ramp(b->sed, OOZE); ramp(b->rock, BASALT);
                           b->sponge = 0.5f; b->clams = 0.3f; b->bones = 0.12f; b->details = 0.45f; b->rocks = 0.1f;
                           b->tripod = 0.6f; b->isopod = 0.5f; b->fishDark = 0.4f; b->whale = 0.15f; b->angler = 0.3f;
                           b->eerie = 0.7f; b->hush = 0.6f; b->whaleSong = 0.3f;
    // Black smokers: chimneys pouring black water, shimmering heat, white crabs and tube worms
    b = &DEF[B_VENTS];     b->zone = 2; b->floorBase = 260.f; b->mounds = 12.f; b->ridges = 14.f; b->cones = 10.f; b->rockMix = 0.7f;
                           b->vis = 26.f; absorb(*b, 4.f, 18.f, 30.f); water(*b, 5, 5, 9, 18, 18, 26, 46, 46, 60);
                           b->caust = 0.f; b->shafts = 0.f; b->snow = 0.5f; b->murk = 0.45f; b->plumes = 1.f; b->glowAmt = 0.2f; b->bubbles = 0.8f;
                           ramp(b->rock, BASALT); { const R4 S = {{24, 22, 22}, {60, 54, 50}, {104, 92, 80}, {150, 136, 116}}; ramp(b->sed, S); }
                           b->vents = 2.f; b->rocks = 0.4f; b->details = 0.5f; b->anemone = 0.2f;
                           b->crab = 1.f; b->fishDark = 0.3f; b->octopus = 0.3f;
                           b->ventRumble = 1.f; b->eerie = 0.3f;
    // ---------------- specials ----------------
    // The sunken city (east): terraces and colonnades, coral on the steps, the light breaking into rainbows
    b = &DEF[B_RUINS];     b->zone = 0; b->floorBase = 34.f; b->terrace = 3.f; b->mounds = 4.f; b->rockMix = 0.5f;
                           b->vis = 50.f; absorb(*b, 6.f, 34.f, 56.f); water(*b, 4, 30, 34, 16, 96, 92, 90, 200, 180);
                           b->caust = 0.9f; b->shafts = 1.f; b->snow = 0.1f; b->surge = 0.4f; b->prism = 1.f;
                           { const R4 R = {{40, 54, 60}, {88, 108, 104}, {140, 158, 144}, {196, 208, 186}}; ramp(b->rock, R); ramp(b->stone, R); }
                           ramp(b->sed, SAND_W);
                           b->ruins = 1.f; b->monolith = 0.2f; b->coral_ = 0.4f; b->grass = 0.3f; b->anemone = 0.2f;
                           b->fishReef = 0.6f; b->turtle = 0.4f; b->nautilus = 0.3f; b->manta = 0.3f;
    // Fire beneath (southeast): basalt and lava, a volcano breathing a glowing green plume, crabs among the vents
    b = &DEF[B_VOLCANO];   b->zone = 1; b->floorBase = 140.f; b->cones = 36.f; b->ridges = 12.f; b->mounds = 6.f; b->rockMix = 0.8f;
                           b->vis = 30.f; absorb(*b, 4.f, 20.f, 32.f); water(*b, 8, 4, 8, 28, 16, 22, 70, 46, 50);
                           b->caust = 0.f; b->shafts = 0.f; b->snow = 0.5f; b->murk = 0.4f; b->lava = 1.f; b->plumes = 1.f; b->bubbles = 1.f;
                           ramp(b->rock, BASALT); ramp(b->sed, BASALT); glow(*b, 90, 255, 150);
                           b->basalt = 0.6f; b->rocks = 0.4f; b->vents = 0.9f; b->volcano = 1.f;
                           b->crab = 0.6f; b->fishDark = 0.3f;
                           b->ventRumble = 0.8f;
    // The jellyfish cathedral (south): open blue water and a bloom of glowing bells
    b = &DEF[B_JELLY];     b->zone = 1; b->floorBase = 300.f; b->mounds = 10.f; b->rockMix = 0.2f;
                           b->vis = 48.f; absorb(*b, 5.f, 24.f, 60.f); water(*b, 6, 4, 30, 20, 18, 84, 70, 70, 180);
                           b->caust = 0.f; b->shafts = 0.5f; b->snow = 0.3f; b->glowAmt = 0.6f; glow(*b, 220, 120, 255);
                           b->jelly = 3.f; b->fishSilver = 0.25f; b->whale = 0.1f;
                           b->hush = 0.3f; b->whaleSong = 0.4f;
    // The iron graveyard (southwest): rusted girders and broken hulls in murky water, eels in the pipes
    b = &DEF[B_WRECK];     b->zone = 1; b->floorBase = 70.f; b->mounds = 4.f; b->rockMix = 0.2f;
                           b->vis = 30.f; absorb(*b, 5.f, 24.f, 34.f); water(*b, 4, 16, 18, 18, 56, 60, 70, 130, 128);
                           b->caust = 0.2f; b->shafts = 0.8f; b->snow = 0.4f; b->murk = 0.45f;
                           { const R4 S = {{36, 34, 32}, {80, 72, 62}, {122, 110, 90}, {170, 156, 126}}; ramp(b->sed, S); }
                           { const R4 R = {{40, 22, 16}, {96, 50, 28}, {156, 86, 44}, {206, 140, 82}}; ramp(b->stone, R); }   // rust
                           b->wreck = 1.f; b->rocks = 0.2f; b->sponge = 0.2f; b->anemone = 0.3f;
                           b->eel = 0.5f; b->fishSilver = 0.5f; b->octopus = 0.3f; b->shark = 0.2f;
                           b->eerie = 0.5f;
    // Brine lakes (west): lakes on the seabed with their own shimmering surface, drowned trees, a toxic glow
    b = &DEF[B_BRINE];     b->zone = 1; b->floorBase = 110.f; b->mounds = 6.f; b->pools = 1.f; b->rockMix = 0.3f;
                           b->vis = 30.f; absorb(*b, 4.f, 22.f, 22.f); water(*b, 4, 14, 8, 14, 50, 34, 50, 120, 70);
                           b->caust = 0.f; b->shafts = 0.5f; b->snow = 0.3f; b->murk = 0.35f; b->brine = 1.f; glow(*b, 170, 255, 60);
                           { const R4 S = {{20, 30, 22}, {50, 66, 44}, {88, 104, 64}, {132, 146, 96}}; ramp(b->sed, S); }
                           b->deadtrees = 0.6f; b->bones = 0.2f; b->rocks = 0.2f;
                           b->eel = 0.3f; b->fishDark = 0.2f; b->isopod = 0.3f;
                           b->eerie = 0.6f;
    // Amethyst deep (northwest): violet crystal spires that glow and chime, blue fish in the light
    b = &DEF[B_CRYSTAL];   b->zone = 2; b->floorBase = 220.f; b->ridges = 20.f; b->mounds = 8.f; b->rockMix = 0.6f;
                           b->vis = 34.f; absorb(*b, 4.f, 18.f, 36.f); water(*b, 6, 3, 16, 28, 14, 56, 86, 50, 140);
                           b->caust = 0.f; b->shafts = 0.f; b->snow = 0.4f; b->glowAmt = 0.7f; glow(*b, 200, 120, 255);
                           { const R4 R = {{20, 14, 34}, {50, 36, 72}, {90, 70, 120}, {140, 116, 170}}; ramp(b->rock, R); }
                           { const R4 C = {{170, 90, 240}, {220, 140, 255}, {120, 70, 220}, {250, 200, 255}}; ramp(b->coral, C); }
                           b->crystals = 1.f; b->pillars = 0.3f; b->rocks = 0.3f;
                           b->fishGlow = 0.4f; b->jelly = 0.2f;
                           b->iceSong = 0.7f;
    // Under the ice (north): the surface is a shelf of ice, cracks of light in it, narwhals, snow falling up
    b = &DEF[B_POLAR];     b->zone = 0; b->floorBase = 46.f; b->mounds = 6.f; b->ridges = 8.f; b->rockMix = 0.4f;
                           b->vis = 70.f; absorb(*b, 7.f, 30.f, 50.f); water(*b, 6, 16, 34, 30, 70, 110, 150, 200, 230);
                           b->caust = 0.2f; b->shafts = 0.8f; b->snow = 0.5f; b->iceCeiling = 1.f; b->auroraBoost = 1.f;
                           { const R4 S = {{60, 70, 100}, {120, 136, 166}, {186, 200, 220}, {236, 244, 250}}; ramp(b->sed, S); }
                           { const R4 R = {{40, 50, 76}, {84, 100, 130}, {140, 160, 186}, {200, 216, 236}}; ramp(b->rock, R); ramp(b->stone, R); }
                           b->ice = 1.f; b->rocks = 0.3f; b->details = 0.2f;
                           b->narwhal = 0.8f; b->fishSilver = 0.3f; b->jelly = 0.3f; b->whale = 0.2f;
                           b->iceSong = 1.f;
    // Leviathan hall (northeast): pillars of a drowned temple, glyph stones, nautilus, and the serpent at home
    b = &DEF[B_LAIR];      b->zone = 2; b->floorBase = 260.f; b->mounds = 8.f; b->ridges = 10.f; b->terrace = 4.f; b->rockMix = 0.6f;
                           b->vis = 24.f; absorb(*b, 4.f, 18.f, 28.f); water(*b, 2, 8, 10, 8, 28, 30, 26, 70, 66);
                           b->caust = 0.f; b->shafts = 0.f; b->snow = 0.4f; b->murk = 0.3f; b->glowAmt = 0.4f; glow(*b, 90, 255, 220);
                           ramp(b->rock, BASALT);
                           { const R4 R = {{26, 34, 34}, {58, 70, 66}, {98, 110, 100}, {146, 156, 140}}; ramp(b->stone, R); }
                           b->pillars = 0.8f; b->monolith = 0.4f; b->ruins = 0.3f; b->rocks = 0.4f; b->details = 0.3f;
                           b->serpent = 1.f; b->nautilus = 0.6f; b->fishDark = 0.3f;
                           b->eerie = 1.f;
}

// ---------------- the journey ----------------
static uint32_t s_gen = 1;
uint32_t journeyGeneration() { return s_gen; }
static void memoReset();
static constexpr int MAXSEG = 512;
static constexpr float BLEND = 80.f;            // metres over which one biome becomes the next (the seabed shelves)
static uint32_t s_seed = 1;
static float s_start[MAXSEG + 1];
static uint8_t s_biome[MAXSEG];
static int s_cur = 0;
static float s_hx = 0.f, s_hy = 0.f, s_hlen = 0.f;    // heading accumulated through the current biome
static float s_dsum = 0.f, s_dlen = 0.f;              // how deep you cruised through it (fraction of its floor)
static int s_streak = 0, s_streakOct = -1;
static int s_wish = 0;

static inline float hashf01(uint32_t x) { return (hash32(x) >> 8) * (1.f / 16777216.f); }

static uint8_t pickInZone(int i, int zone, uint8_t avoid) {
    uint8_t c[3];
    int n = 0;
    for (int k = 0; k < B_ORDINARY_N; k++) if ((int)DEF[k].zone == zone && k != avoid) c[n++] = (uint8_t)k;
    if (!n) return (uint8_t)(zone * 3);
    return c[(int)(hashf01(s_seed ^ (uint32_t)i * 0x9E3779B1u) * n) % n];
}

void journeyInit(uint32_t seed, float distance) {
    defineBiomes();
    memoReset();
    s_seed = hash32(seed ^ 0x5EAD17u);
    s_start[0] = distance - 60.f;
    for (int i = 0; i < MAXSEG; i++) s_start[i + 1] = s_start[i] + 700.f + hashf01(s_seed + (uint32_t)i * 7919u) * 400.f;
    s_biome[0] = hashf01(s_seed) < 0.55f ? B_REEF : B_KELP;     // the dive begins in sunlight
    for (int i = 1; i < MAXSEG; i++) s_biome[i] = pickInZone(i, 0, s_biome[i - 1]);
    s_cur = 0;
    s_hx = s_hy = s_hlen = 0.f; s_dsum = s_dlen = 0.f;
    s_streak = 0; s_streakOct = -1; s_wish = 0;
}

static int segAt(float D) {
    int i = s_cur;
    while (i > 0 && D < s_start[i]) i--;
    while (i < MAXSEG - 1 && D >= s_start[i + 1]) i++;
    return i;
}

static uint8_t specialFor(int oct) {
    static const uint8_t SP[8] = {B_RUINS, B_VOLCANO, B_JELLY, B_WRECK, B_BRINE, B_CRYSTAL, B_POLAR, B_LAIR};
    return oct >= 0 && oct < 8 ? SP[oct] : 255;
}

void journeyUpdate(float distance, float yaw, float moved, float depth) {
    if (moved > 0.f) {
        s_hx += cosf(yaw) * moved; s_hy += sinf(yaw) * moved; s_hlen += moved;
        float fb = fmaxf(6.f, DEF[s_biome[s_cur]].floorBase);
        s_dsum += fminf(1.2f, depth / fb) * moved; s_dlen += moved;
    }
    int i = segAt(distance);
    float into = (distance - s_start[s_cur]) / (s_start[s_cur + 1] - s_start[s_cur]);
    // the depth director: until the next biome starts being dreamt, it follows how deep you've been cruising
    if (s_dlen > 60.f) {
        float f = s_dsum / s_dlen;
        s_wish = f > 0.55f ? 1 : (f < 0.22f ? -1 : 0);
        if (into < 0.8f && s_cur + 1 < MAXSEG && s_biome[s_cur + 1] < B_ORDINARY_N) {
            int zc = (int)(DEF[s_biome[s_cur]].zone + 0.5f);
            int want = zc + s_wish;
            want = want < 0 ? 0 : (want > 2 ? 2 : want);
            if ((int)(DEF[s_biome[s_cur + 1]].zone + 0.5f) != want) {
                s_biome[s_cur + 1] = pickInZone(s_cur + 1 + (int)(s_dlen * 0.01f), want, s_biome[s_cur]);
                s_gen++;
            }
        }
    }
    // two biomes held on one heading, and still holding it well into the third: the next one is special
    if (s_streak >= 2 && s_cur + 1 < MAXSEG && s_biome[s_cur + 1] < B_ORDINARY_N && s_biome[s_cur] < B_ORDINARY_N) {
        float straight = s_hlen > 1.f ? sqrtf(s_hx * s_hx + s_hy * s_hy) / s_hlen : 0.f;
        if (into > 0.35f && into < 0.8f && straight > 0.45f) {
            int oct = ((int)floorf(atan2f(s_hy, s_hx) / 0.7853982f + 0.5f) % 8 + 8) % 8;
            uint8_t sp = specialFor(oct);
            if (sp != 255 && ((oct - s_streakOct + 9) % 8) <= 2 && s_biome[s_cur + 1] != sp) { s_biome[s_cur + 1] = sp; s_gen++; }
        }
    }
    while (s_cur < i) {                                       // we've crossed into the next biome
        float straight = s_hlen > 1.f ? sqrtf(s_hx * s_hx + s_hy * s_hy) / s_hlen : 0.f;
        int oct = -1;
        if (straight > 0.45f) oct = ((int)floorf(atan2f(s_hy, s_hx) / 0.7853982f + 0.5f) % 8 + 8) % 8;
        if (oct >= 0 && (s_streakOct < 0 || ((oct - s_streakOct + 9) % 8) <= 2)) { s_streak++; s_streakOct = oct; }
        else { s_streak = oct >= 0 ? 1 : 0; s_streakOct = oct; }
        s_cur++;
        if (s_biome[s_cur] >= B_ORDINARY_N) { s_streak = 0; s_streakOct = -1; }
        s_gen++;
        if (s_cur + 1 < MAXSEG) {                             // provisional: the depth director refines it as you go
            int zc = (int)(DEF[s_biome[s_cur]].zone + 0.5f);
            s_biome[s_cur + 1] = pickInZone(s_cur + 1, zc, s_biome[s_cur]);
        }
        s_hx = s_hy = s_hlen = 0.f; s_dsum = s_dlen = 0.f;
    }
}

static void lerpParams(BiomeParams& o, const BiomeParams& a, const BiomeParams& b, float t) {
    const float* pa = &a.floorBase; const float* pb = &b.floorBase; float* po = &o.floorBase;
    const int n = (int)(sizeof(BiomeParams) / sizeof(float));
    for (int k = 0; k < n; k++) po[k] = pa[k] + (pb[k] - pa[k]) * t;
}

// Near a border every lookup blends two biomes. The last blends are remembered (quantised to 1/128: invisible).
struct BlendMemo { int a, b, q; BiomeParams p; };
static constexpr int MEMO_N = 8;
static BlendMemo* s_memo = nullptr;
static void memoReset() {
    if (!s_memo) s_memo = (BlendMemo*)memFast(sizeof(BlendMemo) * MEMO_N);
    if (s_memo) for (int i = 0; i < MEMO_N; i++) s_memo[i].q = -1;
    s_gen++;
}
static int s_memoNext = 0;
static const BiomeParams& blendOf(uint8_t ba, uint8_t bb, float t) {
    int q = (int)(t * 128.f + 0.5f);
    if (!s_memo) memoReset();
    for (int i = 0; i < MEMO_N; i++) { BlendMemo& m = s_memo[i]; if (m.q == q && m.a == ba && m.b == bb) return m.p; }
    BlendMemo& m = s_memo[s_memoNext];
    s_memoNext = (s_memoNext + 1) & (MEMO_N - 1);
    float tq = q / 128.f;
    m.a = ba; m.b = bb; m.q = q;
    lerpParams(m.p, DEF[ba], DEF[bb], tq * tq * (3.f - 2.f * tq));
    return m.p;
}

const BiomeParams& biomeAt(float D) {
    if (!DEF) defineBiomes();
    int i = segAt(D);
    float into = D - s_start[i], left = s_start[i + 1] - D;
    if (left < BLEND * 0.5f && i + 1 < MAXSEG) return blendOf(s_biome[i], s_biome[i + 1], 0.5f - left / BLEND);
    if (into < BLEND * 0.5f && i > 0) return blendOf(s_biome[i - 1], s_biome[i], 0.5f + into / BLEND);
    return DEF[s_biome[i]];
}
const BiomeParams& biomeDef(uint8_t b) { if (!DEF) defineBiomes(); return DEF[b < B_ALL_N ? b : 0]; }

uint8_t journeyBiome() { return s_biome[s_cur]; }
uint8_t biomeIdAt(float D) { return s_biome[segAt(D)]; }
float biomeStart(float D) { return s_start[segAt(D)]; }
float biomeEnd(float D) { return s_start[segAt(D) + 1]; }
void journeyForce(float D, uint8_t b) { s_biome[segAt(D)] = b; memoReset(); }
int journeyStreak() { return s_streak; }
int journeyOctant() { return s_streakOct; }
int journeyNextZoneWish() { return s_wish; }

}
