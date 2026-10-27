#pragma once
#include <stdint.h>

// The databank: every kind of thing the scanner can read, and which ones you've read. Kept in flash across
// power-off, with the deepest you've been. The cards draw at full resolution over the world (display overlay).

namespace mantis {

constexpr int SPECIES_N = 32;
// 0..15: the creatures (in Critter kind order), 16..19: the schools, 20..31: things that grow and stand
enum : uint8_t {
    SP_SCHOOL0 = 16, SP_KELP = 20, SP_CORAL, SP_LANTERNCORAL, SP_ANEMONE, SP_BARREL, SP_GLASS, SP_CLAM, SP_SMOKER,
    SP_AMETHYST, SP_GLYPH, SP_WHALEFALL, SP_SEAGRASS
};

const char* speciesName(uint8_t sp);
const char* speciesNote(uint8_t sp);
uint8_t speciesForObject(uint8_t objType, uint8_t var);   // 255: nothing to read
void databankLoad();
bool databankHas(uint8_t sp);
bool databankAdd(uint8_t sp);                             // true if it's new
int  databankCount();
void databankNoteDepth(float depth);
float databankDeepest();
bool databankMuted();
void databankSetMuted(bool m);

// cards
enum : uint8_t { CARD_NONE, CARD_SCAN, CARD_BANK, CARD_CHART };
void cardOpen(uint8_t kind, uint8_t arg, bool fresh);
void cardClose();
uint8_t cardKind();
bool cardModal();                     // the bank and the chart cover the view: the helm waits

}
