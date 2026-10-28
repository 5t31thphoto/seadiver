#include "databank.h"
#include "display.h"
#include "ui.h"
#include "field.h"
#include "scene.h"
#include "journey.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifndef HOST
#include <Preferences.h>
#endif

namespace mantis {

struct Sp { const char* name; const char* note; };
static const Sp SP[SPECIES_N] = {
    {"MANTA RAY", "FLIES THROUGH WATER ON WINGS SIX METRES WIDE"},
    {"GREEN TURTLE", "GRAZES THE SEAGRASS. A HUNDRED YEARS OLD"},
    {"REEF SHARK", "PATROLS THE EDGE OF THE LIGHT"},
    {"HUMPBACK WHALE", "SINGS SONGS THAT CHANGE EVERY YEAR"},
    {"MOON JELLY", "NO BRAIN. NO HEART. ADRIFT FOR EVER"},
    {"REEF OCTOPUS", "TASTES WITH ITS ARMS. CHANGES ITS MIND"},
    {"ANGLERFISH", "CARRIES ITS OWN LIGHT, AND A TRAP"},
    {"THE LEVIATHAN", "LONGER THAN THE HALL IT SLEEPS IN"},
    {"NARWHAL", "ITS TUSK IS A TOOTH THAT LISTENS"},
    {"YETI CRAB", "FARMS BACTERIA ON ITS HAIRY ARMS"},
    {"GIANT ISOPOD", "CAN WAIT FIVE YEARS FOR A MEAL"},
    {"TRIPOD FISH", "STANDS ON STILTS, FACING THE CURRENT"},
    {"NAUTILUS", "A SHELL OLDER THAN THE DINOSAURS"},
    {"COELACANTH", "THOUGHT EXTINCT FOR 66 MILLION YEARS"},
    {"BOTTLENOSE DOLPHIN", "CALLS EACH OTHER BY NAME"},
    {"MORAY EEL", "A SECOND JAW IN ITS THROAT"},
    {"REEF FISH", "A HUNDRED COLOURS, ONE SHOAL"},
    {"SARDINE SCHOOL", "TEN THOUSAND MOVING AS ONE"},
    {"LANTERNFISH", "THE MOST NUMEROUS FISH ON EARTH"},
    {"ROCKFISH", "WAITS IN THE DARK BETWEEN STONES"},
    {"GIANT KELP", "GROWS HALF A METRE IN A DAY"},
    {"STONY CORAL", "AN ANIMAL THAT BUILDS ISLANDS"},
    {"LANTERN CORAL", "LIGHTS THE MIDNIGHT ZONE ITSELF"},
    {"SEA ANEMONE", "A FLOWER THAT STINGS"},
    {"BARREL SPONGE", "FILTERS A SWIMMING POOL A DAY"},
    {"GLASS SPONGE", "A SKELETON OF SILICA. 11,000 YEARS OLD"},
    {"GIANT CLAM", "ALGAE LIVE IN ITS MANTLE AND FEED IT"},
    {"BLACK SMOKER", "WATER AT 400 DEGREES. LIFE AROUND IT"},
    {"AMETHYST SPIRE", "THE CRYSTAL HUMS WHEN THE SONAR TOUCHES"},
    {"GLYPH STONE", "THE WRITING IS IN NO KNOWN SCRIPT"},
    {"WHALE FALL", "ONE WHALE FEEDS THE DEEP FOR DECADES"},
    {"SEAGRASS", "A MEADOW THAT BREATHES FOR THE SEA"},
};
const char* speciesName(uint8_t sp) { return sp < SPECIES_N ? SP[sp].name : ""; }
const char* speciesNote(uint8_t sp) { return sp < SPECIES_N ? SP[sp].note : ""; }
uint8_t speciesForObject(uint8_t t, uint8_t var) {
    switch (t) {
        case OBJ_KELP: return SP_KELP;
        case OBJ_CORAL: return (var & 8) ? SP_LANTERNCORAL : SP_CORAL;
        case OBJ_ANEMONE: return SP_ANEMONE;
        case OBJ_SPONGE: return var ? SP_GLASS : SP_BARREL;
        case OBJ_CLAM: return SP_CLAM;
        case OBJ_VENT: return SP_SMOKER;
        case OBJ_CRYSTAL: return SP_AMETHYST;
        case OBJ_MONOLITH: return SP_GLYPH;
        case OBJ_BONES: return SP_WHALEFALL;
        case OBJ_GRASS: return SP_SEAGRASS;
    }
    return 255;
}

static uint32_t s_bits = 0;
static float s_deep = 0.f;
static bool s_muted = false;
static uint32_t s_savedBits = 0;
static float s_savedDeep = 0.f;

static void save() {
#ifndef HOST
    Preferences p;
    if (p.begin("mantisdive", false)) { p.putUInt("scan", s_bits); p.putFloat("deep", s_deep); p.putBool("mute", s_muted); p.end(); }
#endif
    s_savedBits = s_bits; s_savedDeep = s_deep;
}
void databankLoad() {
#ifndef HOST
    Preferences p;
    if (p.begin("mantisdive", true)) { s_bits = p.getUInt("scan", 0); s_deep = p.getFloat("deep", 0.f); s_muted = p.getBool("mute", false); p.end(); }
#endif
    s_savedBits = s_bits; s_savedDeep = s_deep;
}
bool databankHas(uint8_t sp) { return sp < SPECIES_N && (s_bits >> sp) & 1u; }
bool databankAdd(uint8_t sp) {
    if (sp >= SPECIES_N || databankHas(sp)) return false;
    s_bits |= 1u << sp;
    save();
    return true;
}
int databankCount() { int n = 0; for (int i = 0; i < SPECIES_N; i++) n += (s_bits >> i) & 1u; return n; }
// Flash writes pause both cores for tens of milliseconds, so the deepest depth is only kept in memory here and
// written along with something else (a scan, the mute switch) or when a card is open (the helm is waiting anyway).
void databankNoteDepth(float d) { if (d > s_deep) s_deep = d; }
float databankDeepest() { return s_deep; }
bool databankMuted() { return s_muted; }
void databankSetMuted(bool m) { s_muted = m; save(); }

// ================= cards: full resolution, over the world =================
static uint8_t s_card = CARD_NONE;
static constexpr int OW = 320, OH = 240;
static void fill(uint16_t* o, int x0, int y0, int w, int h, uint16_t c) {
    for (int y = y0; y < y0 + h; y++) { if ((unsigned)y >= (unsigned)OH) continue; for (int x = x0; x < x0 + w; x++) if ((unsigned)x < (unsigned)OW) o[y * OW + x] = c; }
}
static void frame(uint16_t* o, int x0, int y0, int w, int h, uint16_t bg, uint16_t edge) {
    fill(o, x0, y0, w, h, bg);
    fill(o, x0, y0, w, 1, edge); fill(o, x0, y0 + h - 1, w, 1, edge); fill(o, x0, y0, 1, h, edge); fill(o, x0 + w - 1, y0, 1, h, edge);
    fill(o, x0 + 2, y0 + 2, 6, 1, edge); fill(o, x0 + 2, y0 + 2, 1, 6, edge);
    fill(o, x0 + w - 8, y0 + h - 3, 6, 1, edge); fill(o, x0 + w - 3, y0 + h - 8, 1, 6, edge);
}
static void centred(uint16_t* o, int y, const char* s, uint16_t c, int scale) { uiText(o, OW, OH, (OW - uiTextWidth(s, scale)) / 2, y, s, c, scale); }
static void wrapped(uint16_t* o, int x, int y, int cols, const char* s, uint16_t c) {
    char line[64];
    int ln = 0;
    while (*s && ln < 4) {
        int n = (int)strlen(s), cut = n;
        if (n > cols) { cut = cols; while (cut > cols / 3 && s[cut] != ' ') cut--; if (s[cut] != ' ') cut = cols; }
        memcpy(line, s, (size_t)cut); line[cut] = 0;
        uiText(o, OW, OH, x, y + ln * 11, line, c, 1);
        s += cut; while (*s == ' ') s++;
        ln++;
    }
}

static const char* todName(float tod) {
    if (tod < 0.2f || tod > 0.86f) return "NIGHT";
    if (tod < 0.3f) return "DAWN";
    if (tod < 0.45f) return "MORNING";
    if (tod < 0.58f) return "NOON";
    if (tod < 0.7f) return "AFTERNOON";
    return "DUSK";
}

void cardOpen(uint8_t kind, uint8_t arg, bool fresh) {
    uint16_t* o = overlayBuffer();
    if (!o) return;
    memset(o, 0, OW * OH * sizeof(uint16_t));
    const uint16_t BG = rgb565(6, 18, 26), EDGE = rgb565(70, 190, 190), TXT = rgb565(200, 236, 232), DIM = rgb565(70, 110, 120), AMB = rgb565(255, 196, 90);
    char b[64];
    s_card = kind;
    if (kind != CARD_SCAN && (s_deep > s_savedDeep + 1.f)) save();
    if (kind == CARD_SCAN) {
        frame(o, 18, 150, 284, 78, BG, fresh ? AMB : EDGE);
        uiText(o, OW, OH, 30, 160, speciesName(arg), fresh ? AMB : TXT, 2);
        wrapped(o, 30, 182, 44, speciesNote(arg), TXT);
        snprintf(b, sizeof b, fresh ? "NEW ENTRY  %d/%d" : "IN THE DATABANK  %d/%d", databankCount(), SPECIES_N);
        uiText(o, OW, OH, 30, 212, b, fresh ? AMB : DIM, 1);
    } else if (kind == CARD_BANK) {
        frame(o, 6, 6, 308, 228, BG, EDGE);
        snprintf(b, sizeof b, "DATABANK  %d/%d", databankCount(), SPECIES_N);
        centred(o, 14, b, AMB, 2);
        for (int i = 0; i < SPECIES_N; i++) {
            int col = i / 16, row = i % 16;
            int x = 16 + col * 152, y = 38 + row * 11;
            bool has = databankHas((uint8_t)i);
            uiText(o, OW, OH, x, y, has ? speciesName((uint8_t)i) : "? ? ? ? ?", has ? TXT : DIM, 1, false);
        }
        centred(o, 230 - 8, "ANY BUTTON TO CLOSE", DIM, 1);
    } else if (kind == CARD_CHART) {
        const Scene& sc = g_scene;
        frame(o, 6, 6, 308, 228, BG, EDGE);
        centred(o, 14, biomeName(sc.biome), AMB, 2);
        static const char* const ZN[3] = {"THE SUNLIT ZONE", "THE TWILIGHT ZONE", "THE MIDNIGHT ZONE"};
        centred(o, 36, ZN[(int)(sc.zone + 0.5f) > 2 ? 2 : (int)(sc.zone + 0.5f)], TXT, 1);
        int y = 58;
        snprintf(b, sizeof b, "DEPTH %dM   SEABED %dM BELOW", (int)sc.depth, (int)fmaxf(0.f, sc.altitude)); uiText(o, OW, OH, 22, y, b, TXT, 1); y += 14;
        snprintf(b, sizeof b, "DEEPEST %dM", (int)databankDeepest()); uiText(o, OW, OH, 22, y, b, TXT, 1); y += 14;
        snprintf(b, sizeof b, "TRAVELLED %.1f KM", sc.distance / 1000.f); uiText(o, OW, OH, 22, y, b, TXT, 1); y += 14;
        static const char* const DIRS[8] = {"EAST", "SOUTHEAST", "SOUTH", "SOUTHWEST", "WEST", "NORTHWEST", "NORTH", "NORTHEAST"};
        int oct = ((int)floorf(sc.yaw / 0.7853982f + 0.5f) % 8 + 8) % 8;
        snprintf(b, sizeof b, "HEADING %s", DIRS[oct]); uiText(o, OW, OH, 22, y, b, TXT, 1); y += 14;
        snprintf(b, sizeof b, "ABOVE THE WATER: %s", todName(sc.tod)); uiText(o, OW, OH, 22, y, b, TXT, 1); y += 20;
        int wish = journeyNextZoneWish();
        uiText(o, OW, OH, 22, y, wish > 0 ? "CRUISING LOW: THE SEA AHEAD FALLS AWAY" : wish < 0 ? "CLIMBING: THE SEABED AHEAD RISES" : "HOLDING THE MIDDLE WATER", DIM, 1); y += 13;
        int st = journeyStreak();
        if (st > 0 && sc.biome < B_ORDINARY_N) { snprintf(b, sizeof b, "HELD THIS HEADING: %d OF 3 PLACES", st > 3 ? 3 : st); uiText(o, OW, OH, 22, y, b, DIM, 1); y += 13; }
        snprintf(b, sizeof b, "SCANNED %d OF %d", databankCount(), SPECIES_N); uiText(o, OW, OH, 22, y, b, DIM, 1);
        centred(o, 222, "ANY BUTTON TO CLOSE", DIM, 1);
    }
    overlayEnable(true);
}
void cardClose() {
    if (s_card == CARD_NONE) return;
    s_card = CARD_NONE;
    overlayEnable(false);
    overlayClear();
}
uint8_t cardKind() { return s_card; }
bool cardModal() { return s_card == CARD_BANK || s_card == CARD_CHART; }

}
