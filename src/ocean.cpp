// The ocean: the bed you swim through. Everything is synthesized; nothing is sampled.
// The rule (MantisPlanet's, learned on its leaves): a bed is never made of hard onsets. Continuous things are
// noise under a gliding flutter; one-shots fade in over a few milliseconds; nothing ticks for the sake of it.
#include <math.h>
#include "audio_int.h"
#include "audio.h"
#include "universe.h"
#include <string.h>

namespace mantis {
namespace snd {

// ============================== SOUND TUNING ==============================
// LVL_* multiply a sound's loudness (1 = as designed). PITCH_* multiply its frequencies (0.5 = an octave down).
// Render any sound on its own to check it: tools/host/stems.sh (see snaps/stems/INDEX.md).
// ---- the sea ----
constexpr float LVL_SEA = 1.0f;                                  // the slow breathing swell, always there
constexpr float LVL_WASH = 1.0f,   PITCH_WASH = 1.0f;           // waves overhead, near the surface
constexpr float LVL_RAIN = 1.0f,   PITCH_RAIN = 1.0f;           // rain heard through the surface (a hiss)
constexpr float LVL_THUNDER = 1.0f;
constexpr float LVL_VENT = 1.0f,   PITCH_VENT = 1.0f;           // black smokers' rumble and hiss
constexpr float LVL_ERUPT = 1.0f;
constexpr float LVL_CHIME = 1.0f,  PITCH_CHIME = 1.0f;          // crystal and ice chimes
constexpr float LVL_EERIE = 1.0f,  PITCH_EERIE = 1.0f;          // the abyss's groans and pings
// ---- the sub ----
constexpr float LVL_ENGINE = 1.0f, PITCH_ENGINE = 1.0f;         // the screw, and cavitation at flank
constexpr float LVL_BUBBLES = 1.0f, PITCH_BUBBLES = 1.0f;       // every bubble (hull, tanks, vents, eruption)
constexpr float BUBBLE_RATE = 1.0f;                              // how many drift off the hull
constexpr float LVL_CREAK = 1.0f,  PITCH_CREAK = 1.0f;
constexpr float LVL_BUMP = 1.0f,   PITCH_BUMP = 1.0f;
constexpr float LVL_SONAR = 1.0f,  PITCH_SONAR = 1.0f;
constexpr float LVL_SCAN = 1.0f,   PITCH_SCAN = 1.0f;
constexpr float LVL_UI = 1.0f,     PITCH_UI = 1.0f;             // lights, cards, cruise, the throttle detent
constexpr float LVL_TRIM = 1.0f;                                 // B: the tanks venting
// ---- life ----
constexpr float LVL_SHRIMP = 1.0f;                               // the reef's sizzle
constexpr float LVL_WHALE = 1.0f,  PITCH_WHALE = 1.0f;
constexpr float LVL_DOLPHIN = 1.0f, PITCH_DOLPHIN = 1.0f;
constexpr float LVL_NARWHAL = 1.0f, PITCH_NARWHAL = 1.0f;
constexpr float LVL_SEAL = 1.0f,   PITCH_SEAL = 1.0f;
constexpr float LVL_SERPENT = 1.0f;
// ==========================================================================

using dsp::clampf;
static inline float smooth(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0.f, 1.f); return t * t * (3.f - 2.f * t); }

// ================= one-shot pool: filtered noise grains and decaying pings =================
struct Grain {
    bool on = false;
    int delay = 0, att = 0, age = 0;
    float env = 0, dec = 0, amp = 0, rev = 0.1f;
    float fc0 = 1000, fc1 = 1000, q = 1, sweep = 0;
    int sweepN = 1;
    uint8_t mode = 0;                                     // 0 BP, 1 LP
    uint8_t tag = 0;
    dsp::SVF f;
};
struct Ping {
    bool on = false;
    int delay = 0, att = 1, age = 0;
    uint32_t ph = 0;
    float dph = 0, mul = 1, env = 0, dec = 0, amp = 0, rev = 0.15f, dly = 0.f;
    uint8_t tag = 0;
};
static uint8_t s_tag = TAG_SEA;                           // what's making the sounds being started now
static Grain s_gr[24];
static Ping s_pg[20];

static inline float decFor(float tauSec) { return expf(-1.f / (tauSec * dsp::SR)); }

// attSec: never shorter than 1.5 ms (a faster onset is a click)
static void grain(float delaySec, uint8_t mode, float fc0, float fc1, float q, float attSec, float tauSec, float amp, float rev = 0.15f) {
    for (Grain& g : s_gr) {
        if (g.on) continue;
        g.on = true; g.tag = s_tag;
        g.delay = (int)(delaySec * dsp::SR);
        g.att = (int)(fmaxf(attSec, 0.0015f) * dsp::SR) + 1;
        g.age = 0; g.env = 0.f; g.dec = decFor(tauSec); g.amp = amp; g.rev = rev; g.mode = mode;
        g.fc0 = fc0; g.fc1 = fc1; g.q = q;
        g.sweepN = (int)((attSec + tauSec * 2.5f) * dsp::SR / BLOCK) + 1;
        g.sweep = 0.f;
        g.f.reset(); g.f.set(fc0, q);
        return;
    }
}
// A sine that fades in over attSec (4 ms by default: a pure tone switched on is a click), then decays.
static void ping(float delaySec, float hz, float endMul, float sweepSec, float tauSec, float amp, float rev = 0.2f, float dly = 0.f, float attSec = 0.004f) {
    for (Ping& p : s_pg) {
        if (p.on) continue;
        p.on = true; p.tag = s_tag;
        p.delay = (int)(delaySec * dsp::SR);
        p.ph = 0; p.age = 0;
        p.att = (int)(fmaxf(attSec, 0.0015f) * dsp::SR) + 1;
        p.dph = hz * (4294967296.f / dsp::SR);
        p.mul = powf(endMul, 1.f / (sweepSec * dsp::SR + 1.f));
        p.env = amp; p.amp = amp; p.dec = decFor(tauSec); p.rev = rev; p.dly = dly;
        return;
    }
}
static void shotsBlock(Bus& b, int n) {
    for (Grain& g : s_gr) {
        if (!g.on) continue;
        if (g.delay >= n) { g.delay -= n; continue; }
        if (g.fc0 != g.fc1) { g.sweep = fminf(1.f, g.sweep + 1.f / g.sweepN); g.f.set(g.fc0 * powf(g.fc1 / g.fc0, g.sweep), g.q); }
        float gate = soloGain(g.tag);
        for (int i = g.delay; i < n; i++) {
            float e = g.age < g.att ? g.amp * (float)g.age / g.att : (g.env *= g.dec);
            if (g.age == g.att) g.env = g.amp;
            g.age++;
            float l, bp;
            g.f.tick(rng.bi(), l, bp);
            float v = (g.mode ? l : bp) * e * gate;
            b.dry[i] += v; b.rev[i] += v * g.rev;
        }
        g.delay = 0;
        if (g.age > g.att && g.env < 0.0005f) g.on = false;
    }
    for (Ping& p : s_pg) {
        if (!p.on) continue;
        if (p.delay >= n) { p.delay -= n; continue; }
        float gate = soloGain(p.tag);
        for (int i = p.delay; i < n; i++) {
            float a = p.age < p.att ? (float)p.age / p.att : 1.f;
            p.age++;
            float v = dsp::sine(p.ph) * p.env * a * gate;
            p.ph += (uint32_t)p.dph;
            p.dph *= p.mul;
            if (p.age > p.att) p.env *= p.dec;
            b.dry[i] += v; b.rev[i] += v * p.rev; b.dly[i] += v * p.dly;
        }
        p.delay = 0;
        if (p.env < 0.0005f) p.on = false;
    }
}

// A bubble breaking free: a big, low "glug" (Minnaert: the bigger the bubble, the lower), the pitch lifting a
// little as it rises, a soft onset, and a breath of low water noise around it. size 0..1.
static void bubble(float delay, float size, float amp) {
    float hz = (620.f - 440.f * size) * PITCH_BUBBLES;              // ~180 .. 620 Hz
    float a = amp * LVL_BUBBLES;
    ping(delay, hz, 1.22f, 0.07f, 0.045f + 0.06f * size, a, 0.35f, 0.f, 0.006f);
    ping(delay + 0.004f, hz * 2.02f, 1.18f, 0.06f, 0.02f + 0.02f * size, a * 0.18f, 0.3f, 0.f, 0.006f);
    grain(delay, 1, 520.f * PITCH_BUBBLES, 260.f * PITCH_BUBBLES, 1.2f, 0.008f, 0.05f, a * 0.35f, 0.3f);
}

// ================= long voices: moans (whales, the leviathan, the abyss), chirps (dolphins, seals) =================
struct Moan {
    bool on = false;
    float t = 0, dur = 1, f0 = 200, f1 = 300, amp = 0, vib = 0, rough = 0;
    uint8_t tag = 0;
    uint32_t ph = 0, ph2 = 0, vph = 0;
    dsp::SVF f;
};
static Moan s_moan[3];
static void moan(float delay, float f0, float f1, float dur, float amp, float rough) {
    for (Moan& m : s_moan) {
        if (m.on) continue;
        m.on = true; m.tag = s_tag; m.t = -delay; m.dur = dur; m.f0 = f0; m.f1 = f1; m.amp = amp; m.rough = rough;
        m.vib = 0.008f + 0.01f * rng.uni();
        m.f.reset();
        return;
    }
}
static void moansBlock(Bus& b, int n) {
    float dt = (float)n / dsp::SR;
    for (Moan& m : s_moan) {
        if (!m.on) continue;
        m.t += dt;
        if (m.t < 0.f) continue;
        float u = m.t / m.dur;
        if (u >= 1.f) { m.on = false; continue; }
        float shape = sinf(u * 3.14159f);                       // a rising-falling contour
        float hz = m.f0 + (m.f1 - m.f0) * (u < 0.6f ? u / 0.6f : 1.f - (u - 0.6f) * 1.2f);
        float env = m.amp * smooth(0.f, 0.15f, u) * (1.f - smooth(0.7f, 1.f, u)) * (0.8f + 0.2f * shape);
        m.f.set(hz * 2.2f, 2.5f);
        uint32_t inc = dsp::inc(hz), inc2 = dsp::inc(hz * 2.003f), vinc = dsp::inc(5.5f);
        float gate = soloGain(m.tag);
        for (int i = 0; i < n; i++) {
            m.vph += vinc;
            float v = 1.f + m.vib * dsp::sine(m.vph);
            m.ph += (uint32_t)(inc * v); m.ph2 += (uint32_t)(inc2 * v);
            float x = dsp::sine(m.ph) + 0.55f * dsp::sine(m.ph2) + m.rough * rng.bi() * 0.6f;
            if (m.rough > 0.f) x = dsp::softclip(x * (1.f + 2.f * m.rough));
            x = m.f.bp(x) * env * 0.22f * gate;
            b.dry[i] += x * 0.35f; b.rev[i] += x * 1.f; b.dly[i] += x * 0.15f;
        }
    }
}
struct Chirp { bool on = false; float t = 0, dur = 0.5f, f0 = 5000, f1 = 7000, amp = 0, trill = 0; uint32_t ph = 0, tph = 0; uint8_t tag = 0; };
static Chirp s_ch[3];
static void chirp(float delay, float f0, float f1, float dur, float amp, float trill) {
    for (Chirp& c : s_ch) { if (c.on) continue; c.on = true; c.tag = s_tag; c.t = -delay; c.dur = dur; c.f0 = f0; c.f1 = f1; c.amp = amp; c.trill = trill; return; }
}
static void chirpsBlock(Bus& b, int n) {
    float dt = (float)n / dsp::SR;
    for (Chirp& c : s_ch) {
        if (!c.on) continue;
        c.t += dt;
        if (c.t < 0.f) continue;
        float u = c.t / c.dur;
        if (u >= 1.f) { c.on = false; continue; }
        float hz = c.f0 * powf(c.f1 / c.f0, u) * (1.f + 0.05f * sinf(u * 18.f));
        float env = c.amp * smooth(0.f, 0.08f, u) * (1.f - smooth(0.75f, 1.f, u));
        uint32_t inc = dsp::inc(hz), tinc = dsp::inc(c.trill);
        float gate = soloGain(c.tag);
        for (int i = 0; i < n; i++) {
            c.ph += inc; c.tph += tinc;
            float am = c.trill > 0.f ? 0.55f + 0.45f * dsp::sine(c.tph) : 1.f;   // a trill that pulses, never gates hard
            float x = dsp::sine(c.ph) * env * am * 0.11f * gate;
            b.dry[i] += x * 0.5f; b.rev[i] += x * 0.9f;
        }
    }
}

// ================= the beds =================
static dsp::Pink s_pink;
static dsp::SVF s_seaF, s_washF, s_washLo, s_rainF, s_rainLo, s_engF, s_ventF, s_hissF, s_cavF, s_shrimpF;
static uint32_t s_surgePh = 0, s_engPh = 0, s_engPh2 = 0, s_engPh3 = 0, s_bladePh = 0;
static float s_wash = 0.f, s_engLvl = 0.f, s_ventLvl = 0.f, s_seaLvl = 0.f, s_rainLvl = 0.f, s_shrimpLvl = 0.f;
static float s_bubbleT = 0.f, s_lifeT = 4.f, s_chimeT = 3.f;
static float s_prevPitch = 0.f;
// flutters: a level that glides between random targets every few ms (texture without onsets)
struct Flutter {
    float v = 0.5f, target = 0.5f, glide = 0.006f; int n = 0, nMin = 100, nVar = 140;
    inline float tick() { if (--n <= 0) { n = nMin + (int)(rng.next() % (uint32_t)nVar); target = rng.uni(); } v += (target - v) * glide; return v; }
};
static Flutter s_rainFl, s_washFl, s_shrimpFl;
static float s_crack = 0.f, s_crackSm = 0.f;

void oceanInit() {
    for (Grain& g : s_gr) g.on = false;
    for (Ping& p : s_pg) p.on = false;
    for (Moan& m : s_moan) m.on = false;
    for (Chirp& c : s_ch) c.on = false;
    s_shrimpFl.nMin = 40; s_shrimpFl.nVar = 90; s_shrimpFl.glide = 0.01f;
    s_washFl.nMin = 600; s_washFl.nVar = 900; s_washFl.glide = 0.002f;
}

void oceanBump(float s) {
    s_tag = TAG_BUMP;
    // a steel hull meeting rock: a dull thud, the plating's low inharmonic clank, the hull ringing on
    float a = clampf(0.3f + s * 0.7f, 0.3f, 1.f) * LVL_BUMP;
    float pb = PITCH_BUMP;
    grain(0.f, 1, 320.f * pb, 140.f * pb, 0.7f, 0.003f, 0.09f, 0.75f * a, 0.25f);
    static const float P[4] = {187.f, 419.f, 761.f, 1153.f}, T[4] = {0.45f, 0.3f, 0.18f, 0.1f}, A[4] = {0.5f, 0.32f, 0.2f, 0.1f};
    for (int k = 0; k < 4; k++) ping(0.f, P[k] * pb * (0.97f + 0.06f * rng.uni()), 0.99f, 0.3f, T[k], A[k] * a * 0.45f, 0.45f, 0.f, 0.002f);
    s_tag = TAG_BUMP;
    for (int k = 0; k < 3; k++) bubble(0.05f + 0.07f * k, 0.6f + rng.uni() * 0.4f, 0.12f * a);
}
void oceanThunder(float delay, float power) {
    s_tag = TAG_THUNDER;
    float a = power * LVL_THUNDER;
    grain(delay, 1, 340.f, 160.f, 0.6f, 0.08f, 0.9f, 0.5f * a, 0.6f);
    grain(delay + 0.4f, 1, 240.f, 140.f, 0.6f, 0.4f, 1.6f, 0.4f * a, 0.7f);
}
void oceanCritter(uint8_t kind, float dist) {
    float near = clampf(1.f - dist / 80.f, 0.15f, 1.f);
    switch (kind) {
        case CRIT_WHALE: {                                    // a humpback's phrase: a rising moan, a fall, a whoop
            s_tag = TAG_WHALE;
            float base = (140.f + rng.uni() * 110.f) * PITCH_WHALE, a = near * LVL_WHALE;
            moan(0.f, base, base * 1.8f, 3.2f, 0.9f * a, 0.f);
            moan(3.6f, base * 2.1f, base * 1.1f, 2.4f, 0.7f * a, 0.f);
            if (rng.uni() < 0.6f) moan(6.6f, base * 1.2f, base * 2.6f, 1.4f, 0.6f * a, 0.f);
            break;
        }
        case CRIT_DOLPHIN: {                                  // whistles, then a soft click train
            s_tag = TAG_DOLPHIN;
            float p = PITCH_DOLPHIN, a = near * LVL_DOLPHIN;
            for (int k = 0; k < 3; k++) chirp(k * 0.5f + rng.uni() * 0.3f, (2800.f + rng.uni() * 1400.f) * p, (3800.f + rng.uni() * 2400.f) * p, 0.3f + rng.uni() * 0.4f, 0.6f * a, 0.f);
            for (int k = 0; k < 10; k++) grain(1.6f + k * 0.04f, 0, 2400.f * p, 2400.f * p, 2.f, 0.0015f, 0.004f, 0.18f * a, 0.1f);
            break;
        }
        case CRIT_SERPENT:                                    // the leviathan: a low, rough, rolling growl
            s_tag = TAG_SERPENT;
            moan(0.f, 92.f, 68.f, 2.8f, 1.f * near * LVL_SERPENT, 0.8f);
            grain(0.2f, 1, 280.f, 160.f, 0.7f, 0.4f, 1.2f, 0.4f * near * LVL_SERPENT, 0.8f);
            break;
        case CRIT_NARWHAL: {                                  // a click train, then a buzz
            s_tag = TAG_NARWHAL;
            float p = PITCH_NARWHAL, a = near * LVL_NARWHAL;
            for (int k = 0; k < 14; k++) grain(k * 0.05f, 0, 1700.f * p, 1700.f * p, 2.5f, 0.0015f, 0.005f, 0.16f * a, 0.12f);
            chirp(0.8f, 1250.f * p, 850.f * p, 0.6f, 0.45f * a, 55.f);
            break;
        }
        case CRIT_BOLT:                                       // something darting away: a swirl and a few bubbles
            s_tag = TAG_BOLT;
            grain(0.f, 1, 520.f, 200.f, 0.8f, 0.012f, 0.14f, 0.25f * near, 0.25f);
            for (int k = 0; k < 3; k++) bubble(0.03f * k, 0.4f + 0.4f * rng.uni(), 0.1f * near);
            break;
        case CRIT_SEAL:                                       // a Weddell seal's trill, falling through the water
            s_tag = TAG_SEAL;
            chirp(0.f, 2300.f * PITCH_SEAL, 210.f * PITCH_SEAL, 1.9f + rng.uni(), 0.55f * near * LVL_SEAL, 9.f + rng.uni() * 7.f);
            break;
        case CRIT_EERIE:                                      // the abyss: a groan, or a ping from nowhere
            s_tag = TAG_EERIE;
            if (rng.uni() < 0.5f) moan(0.f, (70.f + rng.uni() * 40.f) * PITCH_EERIE, (55.f + rng.uni() * 30.f) * PITCH_EERIE, 3.f + rng.uni() * 2.f, 0.4f * LVL_EERIE, 0.35f);
            else ping(0.f, (430.f + rng.uni() * 330.f) * PITCH_EERIE, 0.98f, 1.f, 1.6f, 0.06f * LVL_EERIE, 1.f, 0.4f, 0.02f);
            break;
    }
}
void oceanUi(uint8_t kind) {
    s_tag = (kind == UI_SCAN_TICK || kind == UI_SCAN_OK || kind == UI_SCAN_NEW) ? TAG_SCAN : kind == UI_LEVEL ? TAG_TRIM : kind == UI_CREAK ? TAG_CREAK : TAG_UI;
    const float ps = PITCH_SCAN, ls = LVL_SCAN, pu = PITCH_UI, lu = LVL_UI;
    switch (kind) {
        case UI_SCAN_TICK: ping(0.f, (1180.f + rng.uni() * 120.f) * ps, 1.05f, 0.03f, 0.035f, 0.1f * ls, 0.08f, 0.f, 0.003f); break;
        case UI_SCAN_OK:   ping(0.f, 659.3f * ps, 1.f, 0.1f, 0.14f, 0.24f * ls, 0.25f); ping(0.1f, 880.f * ps, 1.f, 0.1f, 0.22f, 0.24f * ls, 0.3f); break;
        case UI_SCAN_NEW:  ping(0.f, 587.3f * ps, 1.f, 0.1f, 0.16f, 0.24f * ls, 0.3f, 0.2f); ping(0.1f, 784.f * ps, 1.f, 0.1f, 0.16f, 0.24f * ls, 0.3f, 0.2f);
                           ping(0.2f, 1174.7f * ps, 1.f, 0.1f, 0.45f, 0.26f * ls, 0.5f, 0.3f); break;
        case UI_LIGHTS_ON: grain(0.f, 1, 1100.f * pu, 700.f * pu, 1.5f, 0.0015f, 0.012f, 0.35f * lu, 0.05f); ping(0.02f, 233.f * pu, 1.f, 0.2f, 0.35f, 0.1f * lu, 0.1f, 0.f, 0.03f); break;
        case UI_LIGHTS_OFF: grain(0.f, 1, 900.f * pu, 600.f * pu, 1.5f, 0.0015f, 0.012f, 0.3f * lu, 0.05f); break;
        case UI_CARD:      ping(0.f, 523.3f * pu, 1.f, 0.1f, 0.18f, 0.12f * lu, 0.3f); break;
        case UI_CRUISE:    ping(0.f, 440.f * pu, 1.f, 0.05f, 0.1f, 0.12f * lu, 0.1f); break;
        case UI_DETENT:    grain(0.f, 0, 850.f * pu, 850.f * pu, 2.f, 0.0015f, 0.008f, 0.2f * lu, 0.f); break;
        case UI_LEVEL:                                        // B: the trim tanks vent: a low rush of air, a few big glugs
            grain(0.f, 1, 700.f, 260.f, 0.8f, 0.03f, 0.35f, 0.3f * LVL_TRIM, 0.3f);
            for (int k = 0; k < 5; k++) bubble(0.05f + k * 0.07f + rng.uni() * 0.03f, 0.55f + rng.uni() * 0.45f, 0.14f * LVL_TRIM);
            break;
        case UI_CREAK: {                                      // the hull taking the pressure
            float pc = PITCH_CREAK, a = LVL_CREAK;
            grain(0.f, 0, 230.f * pc, 140.f * pc, 9.f, 0.15f, 0.38f, 0.55f * a, 0.5f);
            grain(0.25f, 0, 360.f * pc, 210.f * pc, 12.f, 0.08f, 0.28f, 0.32f * a, 0.5f);
            ping(0.4f, 880.f * pc, 0.9f, 0.4f, 0.5f, 0.05f * a, 0.6f, 0.f, 0.01f);
            break;
        }
    }
}
void oceanSonar(float delay, float strength) {
    s_tag = TAG_SONAR;
    float p = PITCH_SONAR, a = LVL_SONAR;
    ping(0.f, 880.f * p, 0.995f, 0.6f, 0.5f, 0.42f * a, 0.7f, 0.25f, 0.003f);
    ping(0.f, 1760.f * p, 0.995f, 0.6f, 0.14f, 0.07f * a, 0.5f, 0.1f, 0.003f);
    if (strength > 0.02f) ping(delay, 860.f * p, 0.995f, 0.6f, 0.4f, 0.3f * strength * a, 0.9f, 0.2f, 0.01f);
}
void oceanMoment(uint8_t kind) {
    switch (kind) {
        case MOMENT_INTRO:                                    // the hatch seals, the tanks flood
            oceanBump(0.35f);
            s_tag = TAG_INTRO;
            grain(0.3f, 1, 900.f, 220.f, 0.8f, 0.2f, 0.9f, 0.32f, 0.3f);
            for (int k = 0; k < 12; k++) bubble(0.45f + k * 0.08f + rng.uni() * 0.05f, 0.4f + rng.uni() * 0.6f, 0.16f);
            break;
        case MOMENT_ERUPT:                                    // the volcano breathes: a rumble, then a column of big bubbles
            s_tag = TAG_ERUPT;
            grain(0.f, 1, 240.f, 130.f, 0.6f, 0.4f, 2.5f, 0.8f * LVL_ERUPT, 0.7f);
            for (int k = 0; k < 18; k++) bubble(0.3f + k * 0.16f + rng.uni() * 0.1f, 0.7f + rng.uni() * 0.3f, 0.16f * LVL_ERUPT);
            break;
        default: break;
    }
}

void oceanBlock(Bus& b, int n, const Scene& s) {
    float dt = (float)n / dsp::SR;
    float nearSurf = clampf(1.f - s.depth / 25.f, 0.f, 1.f);
    // ---- block-rate decisions ----
    // surface wash: the waves overhead, louder the shallower you are (and in a storm)
    float washT = nearSurf * nearSurf * (0.25f + 0.75f * s.waves) * (s.ceiling < 0.f ? 0.2f : 1.f);
    s_wash += (washT - s_wash) * clampf(dt * 2.f, 0.f, 1.f);
    s_surgePh += dsp::inc(0.13f + 0.05f * s.waves) * n;
    float surge = 0.5f + 0.5f * dsp::sine(s_surgePh);
    s_washF.set((380.f + 520.f * surge) * PITCH_WASH, 0.6f);
    s_washLo.set(180.f * PITCH_WASH, 0.7f);
    // the sea's breath: always there, darker and quieter as you go down
    float seaT = 0.18f + 0.1f * s.surge;
    s_seaLvl += (seaT - s_seaLvl) * clampf(dt, 0.f, 1.f);
    s_seaF.set(240.f + 90.f * surge * s.surge - 60.f * clampf(s.depth / 200.f, 0.f, 1.f), 0.7f);
    // rain through the surface: a soft hiss, darker and fainter the deeper you are
    float rainT = s.rain * nearSurf * (s.ceiling < 0.f ? 0.f : 1.f);
    s_rainLvl += (rainT - s_rainLvl) * clampf(dt * 0.8f, 0.f, 1.f);
    s_rainF.set((850.f - 350.f * (1.f - nearSurf)) * PITCH_RAIN, 0.6f);   // water muffles the patter above into a low hiss
    s_rainLo.set(420.f * PITCH_RAIN, 0.7f);
    // snapping shrimp: a sizzle over the reef, densest at night and near the bottom
    float night = (s.tod > 0.8f || s.tod < 0.2f) ? 1.f : 0.f;
    float shrimpT = s.shrimp * (0.55f + 0.45f * night) * clampf(1.f - s.altitude / 30.f, 0.15f, 1.f);
    s_shrimpLvl += (shrimpT - s_shrimpLvl) * clampf(dt, 0.f, 1.f);
    s_shrimpF.set(3200.f, 0.9f);
    // the screw
    float engT = s.throttle > 0.02f ? 0.15f + 0.85f * clampf(s.speed / 10.f, 0.f, 1.f) : 0.f;
    s_engLvl += (engT - s_engLvl) * clampf(dt * 1.5f, 0.f, 1.f);
    float rpm = 2.f + s.speed * 1.6f;
    s_engF.set((210.f + 55.f * s.speed) * PITCH_ENGINE, 1.6f);
    s_cavF.set(2200.f * PITCH_ENGINE, 0.7f);
    // the vents
    float ventT = s.ventNear * s.ventRumble + s.eruption * 0.8f;
    s_ventLvl += (ventT - s_ventLvl) * clampf(dt, 0.f, 1.f);
    s_ventF.set(220.f * PITCH_VENT, 0.9f); s_hissF.set(1300.f * PITCH_VENT, 0.7f);

    // ---- one-shots driven by the scene ----
    float pitchMove = fabsf(s.pitch - s_prevPitch) / fmaxf(dt, 1e-3f);
    s_prevPitch = s.pitch;
    s_bubbleT += dt * BUBBLE_RATE * (0.12f + pitchMove * 3.f + s.speed * 0.06f + s.ventNear * 3.f + s.eruption * 6.f + (s.ceiling < 0.f ? 0.15f : 0.f));
    s_tag = TAG_BUBBLES;
    while (s_bubbleT > 1.f) { s_bubbleT -= 1.f + rng.uni(); bubble(rng.uni() * 0.05f, 0.3f + 0.7f * rng.uni(), 0.07f + 0.05f * rng.uni()); }
    // crystal chimes and singing ice
    s_chimeT -= dt * (s.iceSong * 0.6f + 0.05f);
    if (s_chimeT <= 0.f) {
        s_chimeT = 2.f + rng.uni() * 5.f;
        if (s.iceSong > 0.2f) {
            static const float PENT[5] = {1.f, 1.125f, 1.333f, 1.5f, 1.778f};
            float f = 587.3f * PITCH_CHIME * PENT[rng.next() % 5] * (rng.uni() < 0.3f ? 2.f : 1.f);
            s_tag = TAG_CHIME;
            ping(0.f, f, 1.f, 0.1f, 1.4f, 0.1f * s.iceSong * LVL_CHIME, 0.8f, 0.3f, 0.006f);
            ping(0.f, f * 2.76f, 1.f, 0.1f, 0.5f, 0.03f * s.iceSong * LVL_CHIME, 0.8f, 0.1f, 0.006f);
            s_tag = TAG_SEAL;
            if (s.ceiling < 0.f && rng.uni() < 0.35f)
                chirp(0.6f, 2300.f * PITCH_SEAL, 210.f * PITCH_SEAL, 1.9f + rng.uni(), 0.5f * s.iceSong * LVL_SEAL, 9.f + rng.uni() * 7.f);
        }
    }
    // life: whale song, the abyss's own groans
    s_lifeT -= dt;
    if (s_lifeT <= 0.f) {
        s_lifeT = 6.f + rng.uni() * 12.f;
        float r = rng.uni();
        s_tag = TAG_WHALE;
        if (r < s.whaleSong * 0.4f + s.whale * 0.6f) { float base = (140.f + rng.uni() * 140.f) * PITCH_WHALE; moan(0.f, base, base * (1.3f + rng.uni()), 2.5f + rng.uni() * 2.f, (0.35f + 0.5f * s.whale) * LVL_WHALE, 0.f); }
        else if (r < 0.75f && s.eerie > 0.2f && rng.uni() < s.eerie * 0.7f) oceanCritter(CRIT_EERIE, 40.f);
    }

    // ---- the per-sample beds ----
    float washA = s_wash * 0.22f * LVL_WASH * soloGain(TAG_WASH), seaA = s_seaLvl * 0.28f * LVL_SEA * soloGain(TAG_SEA);
    float engA = s_engLvl * 0.035f * LVL_ENGINE * soloGain(TAG_ENGINE), ventA = s_ventLvl * 0.25f * LVL_VENT * soloGain(TAG_VENT);
    float rainA = s_rainLvl * 0.11f * LVL_RAIN * soloGain(TAG_RAIN), shrimpA = s_shrimpLvl * 0.019f * LVL_SHRIMP * soloGain(TAG_SHRIMP);
    float cav = clampf((s.speed - 7.f) / 3.f, 0.f, 1.f);
    float f0 = (42.f + s.speed * 6.f) * PITCH_ENGINE;
    uint32_t engInc = dsp::inc(f0 * 3.f), engInc2 = dsp::inc(f0 * 4.98f), engInc3 = dsp::inc(f0 * 7.03f), bladeInc = dsp::inc(rpm);
    float pCrack = shrimpA > 0.0001f ? (220.f + 420.f * night) / dsp::SR : 0.f;   // sparks per sample in the sizzle
    for (int i = 0; i < n; i++) {
        float w = rng.bi();
        float p = s_pink.tick(w);
        float dry = 0.f, rev = 0.f;
        dry += s_seaF.bp(p) * seaA * (0.7f + 0.3f * surge);
        if (washA > 0.0005f) {                                      // waves overhead: a slow hush with a body under it
            float fl = s_washFl.tick();
            float x = (s_washF.bp(p) * 1.6f * (0.5f + 0.5f * fl) + s_washLo.lp(p) * 0.8f) * washA * (0.45f + 0.55f * surge * surge);
            dry += x; rev += x * 0.3f;
        }
        if (rainA > 0.0005f) {                                      // rain through the surface: a soft, fluttering hiss
            float fl = s_rainFl.tick();
            float x = (s_rainF.bp(p) * 1.6f * (0.65f + 0.35f * fl) + s_rainLo.lp(p) * 0.6f) * rainA;
            dry += x; rev += x * 0.25f;
        }
        if (shrimpA > 0.0001f) {                                    // the reef's sizzle: tiny sparks, smoothed to a fizz
            if (rng.uni() < pCrack) s_crack = 0.4f + 0.6f * rng.uni();
            s_crack *= 0.94f;
            s_crackSm += (s_crack - s_crackSm) * 0.35f;            // no edge sharper than ~0.1 ms of glide
            float x = s_shrimpF.bp(w) * (s_crackSm * 0.8f + 0.12f * s_shrimpFl.tick()) * shrimpA * 6.f;
            dry += x; rev += x * 0.15f;
        }
        if (engA > 0.0005f) {
            s_engPh += engInc; s_engPh2 += engInc2; s_engPh3 += engInc3; s_bladePh += bladeInc;
            float blade = 0.6f + 0.4f * dsp::sine(s_bladePh);
            float hum = dsp::sine(s_engPh) * 0.5f + dsp::sine(s_engPh2) * 0.3f + dsp::sine(s_engPh3) * 0.12f + s_engF.bp(p) * 2.f;
            dry += hum * engA * blade;
            if (cav > 0.f) dry += s_cavF.bp(w) * cav * engA * 1.1f * blade * blade;   // cavitation at flank
        }
        if (ventA > 0.001f) { float x = s_ventF.lp(p) * ventA * 1.4f + s_hissF.bp(w) * ventA * 0.1f; dry += x; rev += x * 0.3f; }
        b.dry[i] += dry; b.rev[i] += rev;
    }
    shotsBlock(b, n);
    moansBlock(b, n);
    chirpsBlock(b, n);
}

}
}
