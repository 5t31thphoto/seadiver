// The ocean: the bed you swim through. Everything is synthesized; nothing is sampled.
#include <math.h>
#include "audio_int.h"
#include "audio.h"
#include "universe.h"
#include <string.h>

namespace mantis {
namespace snd {

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
    dsp::SVF f;
};
struct Ping {
    bool on = false;
    int delay = 0;
    uint32_t ph = 0;
    float dph = 0, mul = 1, env = 0, dec = 0, amp = 0, rev = 0.15f, dly = 0.f;
};
static Grain s_gr[28];
static Ping s_pg[20];

static inline float decFor(float tauSec) { return expf(-1.f / (tauSec * dsp::SR)); }

static void grain(float delaySec, uint8_t mode, float fc0, float fc1, float q, float attSec, float tauSec, float amp, float rev = 0.15f) {
    for (Grain& g : s_gr) {
        if (g.on) continue;
        g.on = true;
        g.delay = (int)(delaySec * dsp::SR);
        g.att = (int)(attSec * dsp::SR) + 1;
        g.age = 0; g.env = 0.f; g.dec = decFor(tauSec); g.amp = amp; g.rev = rev; g.mode = mode;
        g.fc0 = fc0; g.fc1 = fc1; g.q = q;
        g.sweepN = (int)((attSec + tauSec * 2.5f) * dsp::SR / BLOCK) + 1;
        g.sweep = 0.f;
        g.f.reset(); g.f.set(fc0, q);
        return;
    }
}
static void ping(float delaySec, float hz, float endMul, float sweepSec, float tauSec, float amp, float rev = 0.2f, float dly = 0.f) {
    for (Ping& p : s_pg) {
        if (p.on) continue;
        p.on = true;
        p.delay = (int)(delaySec * dsp::SR);
        p.ph = 0;
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
        for (int i = g.delay; i < n; i++) {
            float e = g.age < g.att ? g.amp * (float)g.age / g.att : (g.env *= g.dec);
            if (g.age == g.att) g.env = g.amp;
            g.age++;
            float l, bp;
            g.f.tick(rng.bi(), l, bp);
            float v = (g.mode ? l : bp) * e;
            b.dry[i] += v; b.rev[i] += v * g.rev;
        }
        g.delay = 0;
        if (g.age > g.att && g.env < 0.0005f) g.on = false;
    }
    for (Ping& p : s_pg) {
        if (!p.on) continue;
        if (p.delay >= n) { p.delay -= n; continue; }
        for (int i = p.delay; i < n; i++) {
            float v = dsp::sine(p.ph) * p.env;
            p.ph += (uint32_t)p.dph;
            p.dph *= p.mul;
            p.env *= p.dec;
            b.dry[i] += v; b.rev[i] += v * p.rev; b.dly[i] += v * p.dly;
        }
        p.delay = 0;
        if (p.env < 0.0005f) p.on = false;
    }
}

// A bubble is a sine that rises as it pops free: a few ms, pitch climbing.
static void bubble(float delay, float size, float amp) {
    float hz = 3200.f / (0.6f + size * 2.4f);
    ping(delay, hz, 1.5f + 0.4f * rng.uni(), 0.03f, 0.018f + 0.02f * size, amp, 0.25f);
}

// ================= long voices: moans (whales, the leviathan), chirps (dolphins, seals) =================
struct Moan {
    bool on = false;
    float t = 0, dur = 1, f0 = 200, f1 = 300, amp = 0, vib = 0, rough = 0;
    uint32_t ph = 0, ph2 = 0, vph = 0;
    dsp::SVF f;
};
static Moan s_moan[3];
static void moan(float delay, float f0, float f1, float dur, float amp, float rough) {
    for (Moan& m : s_moan) {
        if (m.on) continue;
        m.on = true; m.t = -delay; m.dur = dur; m.f0 = f0; m.f1 = f1; m.amp = amp; m.rough = rough;
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
        for (int i = 0; i < n; i++) {
            m.vph += vinc;
            float v = 1.f + m.vib * dsp::sine(m.vph);
            m.ph += (uint32_t)(inc * v); m.ph2 += (uint32_t)(inc2 * v);
            float x = dsp::sine(m.ph) + 0.55f * dsp::sine(m.ph2) + m.rough * rng.bi() * 0.6f;
            if (m.rough > 0.f) x = dsp::softclip(x * (1.f + 2.f * m.rough));
            x = m.f.bp(x) * env * 0.22f;
            b.dry[i] += x * 0.35f; b.rev[i] += x * 1.f; b.dly[i] += x * 0.15f;
        }
    }
}
struct Chirp { bool on = false; float t = 0, dur = 0.5f, f0 = 5000, f1 = 7000, amp = 0, trill = 0; uint32_t ph = 0, tph = 0; };
static Chirp s_ch[3];
static void chirp(float delay, float f0, float f1, float dur, float amp, float trill) {
    for (Chirp& c : s_ch) { if (c.on) continue; c.on = true; c.t = -delay; c.dur = dur; c.f0 = f0; c.f1 = f1; c.amp = amp; c.trill = trill; return; }
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
        for (int i = 0; i < n; i++) {
            c.ph += inc; c.tph += tinc;
            float am = c.trill > 0.f ? 0.5f + 0.5f * dsp::sine(c.tph) : 1.f;
            float x = dsp::sine(c.ph) * env * am * 0.05f;
            b.dry[i] += x * 0.5f; b.rev[i] += x * 0.9f;
        }
    }
}

// ================= the beds =================
static dsp::Pink s_pink;
static dsp::SVF s_seaF, s_washF, s_engF, s_ventF, s_hissF, s_cavF;
static uint32_t s_surgePh = 0, s_engPh = 0, s_engPh2 = 0, s_bladePh = 0;
static float s_wash = 0.f, s_engLvl = 0.f, s_ventLvl = 0.f, s_seaLvl = 0.f;
static float s_bubbleT = 0.f, s_shrimpT = 0.f, s_lifeT = 4.f, s_rainT = 0.f, s_chimeT = 3.f;
static float s_prevPitch = 0.f;

void oceanInit() {
    for (Grain& g : s_gr) g.on = false;
    for (Ping& p : s_pg) p.on = false;
    for (Moan& m : s_moan) m.on = false;
    for (Chirp& c : s_ch) c.on = false;
}

void oceanBump(float s) {
    // a metal hull meeting rock: an inharmonic clank, a thud, and the hull ringing on
    float a = clampf(0.3f + s * 0.7f, 0.3f, 1.f);
    static const float P[4] = {223.f, 561.f, 1127.f, 1811.f}, T[4] = {0.35f, 0.25f, 0.16f, 0.09f}, A[4] = {0.5f, 0.35f, 0.25f, 0.15f};
    for (int k = 0; k < 4; k++) ping(0.f, P[k] * (0.97f + 0.06f * rng.uni()), 0.985f, 0.3f, T[k], A[k] * a * 0.5f, 0.4f);
    grain(0.f, 1, 500.f, 200.f, 0.7f, 0.003f, 0.08f, 0.6f * a, 0.2f);
    for (int k = 0; k < 4; k++) bubble(0.03f + 0.05f * k, 0.4f + rng.uni() * 0.6f, 0.12f * a);
}
void oceanThunder(float delay, float power) {
    grain(delay, 1, 380.f, 170.f, 0.6f, 0.08f, 0.9f, 0.5f * power, 0.6f);
    grain(delay + 0.4f, 1, 260.f, 150.f, 0.6f, 0.4f, 1.6f, 0.4f * power, 0.7f);
}
void oceanCritter(uint8_t kind, float dist) {
    float near = clampf(1.f - dist / 80.f, 0.15f, 1.f);
    switch (kind) {
        case CRIT_WHALE: {                                    // a humpback's phrase: a rising moan, a fall, a whoop
            float base = 160.f + rng.uni() * 120.f;
            moan(0.f, base, base * 1.8f, 3.2f, 0.9f * near, 0.f);
            moan(3.6f, base * 2.2f, base * 1.1f, 2.4f, 0.7f * near, 0.f);
            if (rng.uni() < 0.6f) moan(6.6f, base * 1.2f, base * 3.f, 1.4f, 0.6f * near, 0.f);
            break;
        }
        case CRIT_DOLPHIN:
            for (int k = 0; k < 3; k++) chirp(k * 0.5f + rng.uni() * 0.3f, 5000.f + rng.uni() * 3000.f, 7000.f + rng.uni() * 5000.f, 0.25f + rng.uni() * 0.4f, 0.6f * near, 0.f);
            for (int k = 0; k < 10; k++) grain(1.6f + k * 0.035f, 0, 4500.f, 4500.f, 3.f, 0.0005f, 0.002f, 0.3f * near, 0.1f);   // a click train
            break;
        case CRIT_SERPENT:                                    // the leviathan: a low, rough, rolling growl
            moan(0.f, 95.f, 70.f, 2.8f, 1.f * near, 0.8f);
            grain(0.2f, 1, 300.f, 180.f, 0.7f, 0.4f, 1.2f, 0.4f * near, 0.8f);
            break;
        case CRIT_NARWHAL:                                    // clicks and a buzz
            for (int k = 0; k < 14; k++) grain(k * 0.05f, 0, 3500.f, 3500.f, 4.f, 0.0005f, 0.002f, 0.25f * near, 0.1f);
            chirp(0.8f, 2200.f, 1500.f, 0.6f, 0.4f * near, 60.f);
            break;
        case CRIT_BOLT:                                       // something darting away
            grain(0.f, 1, 900.f, 300.f, 0.8f, 0.01f, 0.12f, 0.25f * near, 0.2f);
            for (int k = 0; k < 3; k++) bubble(0.02f * k, 0.3f + 0.4f * rng.uni(), 0.1f * near);
            break;
    }
}
void oceanUi(uint8_t kind) {
    switch (kind) {
        case UI_SCAN_TICK: ping(0.f, 2400.f + rng.uni() * 300.f, 1.1f, 0.03f, 0.03f, 0.12f, 0.05f); break;
        case UI_SCAN_OK:   ping(0.f, 1318.5f, 1.f, 0.1f, 0.12f, 0.25f, 0.2f); ping(0.1f, 1760.f, 1.f, 0.1f, 0.2f, 0.25f, 0.3f); break;
        case UI_SCAN_NEW:  ping(0.f, 1174.7f, 1.f, 0.1f, 0.14f, 0.25f, 0.3f, 0.2f); ping(0.1f, 1568.f, 1.f, 0.1f, 0.14f, 0.25f, 0.3f, 0.2f);
                           ping(0.2f, 2349.3f, 1.f, 0.1f, 0.4f, 0.28f, 0.5f, 0.3f); break;
        case UI_LIGHTS_ON: grain(0.f, 0, 3000.f, 3000.f, 2.f, 0.001f, 0.01f, 0.4f, 0.05f); ping(0.02f, 120.f * 4.f, 1.f, 0.2f, 0.3f, 0.1f, 0.1f); break;
        case UI_LIGHTS_OFF: grain(0.f, 0, 2400.f, 2400.f, 2.f, 0.001f, 0.01f, 0.35f, 0.05f); break;
        case UI_CARD:      ping(0.f, 880.f, 1.f, 0.1f, 0.15f, 0.12f, 0.3f); break;
        case UI_LEVEL:     for (int k = 0; k < 6; k++) bubble(k * 0.04f, 0.5f + rng.uni() * 0.5f, 0.12f); break;     // trim tanks venting
        case UI_CRUISE:    ping(0.f, 660.f, 1.f, 0.05f, 0.08f, 0.12f, 0.1f); break;
        case UI_DETENT:    grain(0.f, 0, 2000.f, 2000.f, 2.f, 0.001f, 0.006f, 0.2f, 0.f); break;
        case UI_CREAK:                                        // the hull taking the pressure
            grain(0.f, 0, 340.f, 190.f, 9.f, 0.15f, 0.35f, 0.5f, 0.5f);
            grain(0.25f, 0, 520.f, 300.f, 12.f, 0.08f, 0.25f, 0.3f, 0.5f);
            ping(0.4f, 1900.f, 0.9f, 0.4f, 0.5f, 0.05f, 0.6f);
            break;
    }
}
void oceanSonar(float delay, float strength) {
    ping(0.f, 1480.f, 0.995f, 0.6f, 0.45f, 0.42f, 0.7f, 0.25f);
    ping(0.f, 2960.f, 0.995f, 0.6f, 0.12f, 0.08f, 0.5f, 0.1f);
    if (strength > 0.02f) ping(delay, 1440.f, 0.995f, 0.6f, 0.35f, 0.3f * strength, 0.9f, 0.2f);
}
void oceanMoment(uint8_t kind) {
    switch (kind) {
        case MOMENT_INTRO:                                    // the hatch seals, the tanks flood
            oceanBump(0.35f);
            grain(0.3f, 1, 2400.f, 400.f, 0.8f, 0.2f, 0.9f, 0.3f, 0.3f);
            for (int k = 0; k < 16; k++) bubble(0.4f + k * 0.06f + rng.uni() * 0.05f, rng.uni(), 0.18f);
            break;
        case MOMENT_ERUPT:
            grain(0.f, 1, 260.f, 140.f, 0.6f, 0.4f, 2.5f, 0.8f, 0.7f);
            for (int k = 0; k < 24; k++) bubble(0.3f + k * 0.12f + rng.uni() * 0.1f, 0.5f + rng.uni() * 0.5f, 0.15f);
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
    s_washF.set(700.f + 900.f * surge, 0.6f);
    // the sea's breath: always there, darker and quieter as you go down
    float seaT = 0.18f + 0.1f * s.surge;
    s_seaLvl += (seaT - s_seaLvl) * clampf(dt, 0.f, 1.f);
    s_seaF.set(260.f + 120.f * surge * s.surge - 80.f * clampf(s.depth / 200.f, 0.f, 1.f), 0.7f);
    // the screw
    float engT = s.throttle > 0.02f ? 0.15f + 0.85f * clampf(s.speed / 10.f, 0.f, 1.f) : 0.f;
    s_engLvl += (engT - s_engLvl) * clampf(dt * 1.5f, 0.f, 1.f);
    float rpm = 2.f + s.speed * 1.6f;
    s_engF.set(380.f + 120.f * s.speed, 1.8f);
    s_cavF.set(5200.f, 0.8f);
    // the vents
    float ventT = s.ventNear * s.ventRumble + s.eruption * 0.8f;
    s_ventLvl += (ventT - s_ventLvl) * clampf(dt, 0.f, 1.f);
    s_ventF.set(240.f, 0.9f); s_hissF.set(2600.f, 0.7f);

    // ---- one-shots driven by the scene ----
    float pitchMove = fabsf(s.pitch - s_prevPitch) / fmaxf(dt, 1e-3f);
    s_prevPitch = s.pitch;
    s_bubbleT += dt * (0.4f + pitchMove * 6.f + s.speed * 0.25f + s.ventNear * 6.f + s.eruption * 10.f + (s.ceiling < 0.f ? 0.3f : 0.f));
    while (s_bubbleT > 1.f) { s_bubbleT -= 1.f + rng.uni(); bubble(rng.uni() * 0.05f, rng.uni(), 0.06f + 0.06f * rng.uni()); }
    // snapping shrimp: the reef's frying-pan crackle, louder at night
    float night = (s.tod > 0.8f || s.tod < 0.2f) ? 1.f : 0.f;
    s_shrimpT += dt * s.shrimp * (14.f + 18.f * night) * clampf(1.f - s.altitude / 30.f, 0.2f, 1.f);
    while (s_shrimpT > 1.f) { s_shrimpT -= 0.5f + rng.uni(); grain(rng.uni() * dt, 0, 5000.f + rng.uni() * 3000.f, 6000.f, 2.f, 0.0003f, 0.0015f, 0.15f + 0.2f * rng.uni(), 0.05f); }
    // rain on the surface, heard from below: a soft hiss of drops
    s_rainT += dt * s.rain * nearSurf * 60.f;
    while (s_rainT > 1.f) { s_rainT -= 1.f; grain(rng.uni() * dt, 0, 7000.f + rng.uni() * 3000.f, 9000.f, 1.5f, 0.0005f, 0.004f, 0.05f, 0.1f); }
    // crystal chimes and singing ice
    s_chimeT -= dt * (s.iceSong * 0.6f + 0.05f);
    if (s_chimeT <= 0.f) {
        s_chimeT = 2.f + rng.uni() * 5.f;
        if (s.iceSong > 0.2f) {
            static const float PENT[5] = {1.f, 1.125f, 1.333f, 1.5f, 1.778f};
            float f = 1174.7f * PENT[rng.next() % 5] * (rng.uni() < 0.3f ? 2.f : 1.f);
            ping(0.f, f, 1.f, 0.1f, 1.2f, 0.08f * s.iceSong, 0.8f, 0.3f);
            ping(0.f, f * 2.76f, 1.f, 0.1f, 0.5f, 0.03f * s.iceSong, 0.8f, 0.1f);
            if (s.ceiling < 0.f && rng.uni() < 0.35f)            // a Weddell seal's trill, falling through the water
                chirp(0.6f, 4200.f, 350.f, 1.8f + rng.uni(), 0.5f * s.iceSong, 12.f + rng.uni() * 10.f);
        }
    }
    // life: whale song, dolphins, the abyss's own groans
    s_lifeT -= dt;
    if (s_lifeT <= 0.f) {
        s_lifeT = 6.f + rng.uni() * 12.f;
        float r = rng.uni();
        if (r < s.whaleSong * 0.4f + s.whale * 0.6f) { float base = 140.f + rng.uni() * 160.f; moan(0.f, base, base * (1.3f + rng.uni()), 2.5f + rng.uni() * 2.f, 0.35f + 0.5f * s.whale, 0.f); }
        else if (r < 0.75f && s.eerie > 0.2f && rng.uni() < s.eerie * 0.7f) {
            if (rng.uni() < 0.5f) moan(0.f, 70.f + rng.uni() * 40.f, 55.f + rng.uni() * 30.f, 3.f + rng.uni() * 2.f, 0.4f * s.eerie, 0.35f);
            else ping(0.f, 900.f + rng.uni() * 700.f, 0.98f, 1.f, 1.6f, 0.05f * s.eerie, 1.f, 0.4f);   // a ping from nowhere
        }
    }

    // ---- the per-sample beds ----
    float washA = s_wash * 0.22f, seaA = s_seaLvl * 0.16f, engA = s_engLvl * 0.035f, ventA = s_ventLvl * 0.25f;
    float cav = clampf((s.speed - 7.f) / 3.f, 0.f, 1.f);
    uint32_t engInc = dsp::inc(55.f + s.speed * 9.f), engInc2 = dsp::inc((55.f + s.speed * 9.f) * 3.01f), bladeInc = dsp::inc(rpm);
    for (int i = 0; i < n; i++) {
        float w = rng.bi();
        float p = s_pink.tick(w);
        float dry = 0.f, rev = 0.f;
        dry += s_seaF.bp(p) * seaA * (0.7f + 0.3f * surge);
        if (washA > 0.001f) { float x = s_washF.bp(w) * washA * (0.4f + 0.6f * surge * surge); dry += x; rev += x * 0.3f; }
        if (engA > 0.0005f) {
            s_engPh += engInc; s_engPh2 += engInc2; s_bladePh += bladeInc;
            float blade = 0.6f + 0.4f * dsp::sine(s_bladePh);
            float hum = (dsp::sine(s_engPh) * 0.4f + dsp::sine(s_engPh2) * 0.6f) + s_engF.bp(p) * 2.f;
            dry += hum * engA * blade;
            if (cav > 0.f) dry += s_cavF.bp(w) * cav * engA * 1.5f * blade * blade;   // cavitation hiss at flank
        }
        if (ventA > 0.001f) { float x = s_ventF.lp(p) * ventA * 1.4f + s_hissF.bp(w) * ventA * 0.12f; dry += x; rev += x * 0.3f; }
        b.dry[i] += dry; b.rev[i] += rev;
    }
    shotsBlock(b, n);
    moansBlock(b, n);
    chirpsBlock(b, n);
}

}
}
