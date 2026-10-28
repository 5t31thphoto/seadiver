// Size over speed here: this code runs per object or per frame, not per pixel or per sample, and the app shares
// flash with others (see README, "Footprint").
#pragma GCC optimize("Os")
// Mantis Seadiver: a submarine through an ocean that dreams itself, on an M5Stack Core2.
// MantisPlanet's dream (the world is sampled from where you're going, nothing behind you is kept), taken
// underwater, flown with SpaceMantis's controls. Sound and the LCD push run on core 0; everything else here.
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <M5Unified.h>
#include "helm.h"
#include "universe.h"
#include "journey.h"
#include "terrain.h"
#include "field.h"
#include "scene.h"
#include "render.h"
#include "display.h"
#include "haptics.h"
#include "audio.h"
#include "databank.h"

static float subX = 0.f, subY = 0.f, subZ = -5.f;
static float subSpeed = 0.f;
static constexpr float SPEED_TAU = 1.1f;      // seconds to reach the throttle's speed (the hull's weight in the water)
static float distanceTravelled = 0.f;
static bool lightsOn = true, lightsAuto = true;
static bool wasBumped = false;
static int lastBeat = -1;
static float fpsAvg = 30.f;
static bool showFps = false;
static uint8_t lastBiome = 255;
static int lastZone = 0;
static float bannerT = 0.f;
static const char* bannerText = nullptr;
static float msgT = 0.f;
static const char* msgText = nullptr;
static uint32_t muteIconAt = 0;
static bool bootMuted = false;
static float echoAt = -1.f, echoStr = 0.f;
static float creakT = 20.f, critterSoundT = 8.f, cardT = 0.f;
// the scanner
static uint8_t scanKind = 0;          // 0 none, 1 critter, 2 school, 3 object
static int scanIdx = -1;
static uint8_t scanSp = 255;
static float scanT = 0.f, scanTick = 0.f, scanDoneT = 0.f;
static int scanPx = 0, scanPy = 0;

static void message(const char* s, float sec) { msgText = s; msgT = sec; }

// where the thing being read is right now (false: gone)
static bool scanTarget(float& wx, float& wy, float& wz) {
    if (scanKind == 1) { const mantis::Critter& c = mantis::fieldCritters()[scanIdx]; if (!c.on) return false; wx = c.x; wy = c.y; wz = c.z; return true; }
    if (scanKind == 2) { const mantis::School& s = mantis::fieldSchools()[scanIdx]; if (!s.on) return false; wx = s.x; wy = s.y; wz = s.z; return true; }
    if (scanKind == 3) {
        const mantis::FieldObj& o = mantis::fieldObjects()[scanIdx];
        if (o.type == mantis::OBJ_NONE) return false;
        float h = o.type == mantis::OBJ_KELP ? 2.f : o.type == mantis::OBJ_PILLAR ? o.b * 0.4f : o.top > 0.f ? o.top * 0.5f : o.a * 0.4f;
        wx = o.x; wy = o.y; wz = o.z + h; return true;
    }
    return false;
}

// A tap: read whatever living (or strange) thing is nearest your finger.
static void tryScan(int tx, int ty) {
    using namespace mantis;
    float best = 1e9f;
    uint8_t bk = 0; int bi = -1; uint8_t bsp = 255;
    auto consider = [&](uint8_t k, int i, float wx, float wy, float wz, float radius, uint8_t sp) {
        int px, py; float d;
        if (!renderProject(wx, wy, wz, px, py, d) || d > 32.f) return;
        float rpx = radius * 260.f / d;
        float dd = sqrtf((float)((px - tx) * (px - tx) + (py - ty) * (py - ty)));
        float slack = 22.f + fminf(rpx, 60.f);
        if (dd < slack && dd - rpx * 0.5f < best) { best = dd - rpx * 0.5f; bk = k; bi = i; bsp = sp; }
    };
    const Critter* cr = fieldCritters();
    for (int i = 0; i < CRIT_N; i++) if (cr[i].on) consider(1, i, cr[i].x, cr[i].y, cr[i].z, cr[i].kind == CR_WHALE ? 6.f : cr[i].kind == CR_SERPENT ? 4.f : 1.f, cr[i].kind);
    const School* sc = fieldSchools();
    for (int i = 0; i < SCHOOL_N; i++) if (sc[i].on) consider(2, i, sc[i].x, sc[i].y, sc[i].z, sc[i].spread, (uint8_t)(SP_SCHOOL0 + sc[i].kind));
    if (bi < 0 || best > 8.f) {                                    // nothing alive there: the things that grow and stand
        const FieldObj* o = fieldObjects();
        for (int i = 0; i < FIELD_N; i++) {
            uint8_t sp = speciesForObject(o[i].type, o[i].var);
            if (sp == 255) continue;
            float h = o[i].type == OBJ_KELP ? 2.f : o[i].top > 0.f ? o[i].top * 0.5f : o[i].a * 0.4f;
            consider(3, i, o[i].x, o[i].y, o[i].z + h, fmaxf(0.5f, o[i].a * 0.4f), sp);
        }
    }
    if (bi < 0) { hx::pop(0.1f, 0.01f); return; }
    scanKind = bk; scanIdx = bi; scanSp = bsp; scanT = 0.001f; scanTick = 0.f; scanDoneT = 0.f;
    hx::scanTick(0.5f);
    sfxUi(UI_SCAN_TICK);
}

static void serviceScan(float dt) {
    using namespace mantis;
    if (scanDoneT > 0.f) { scanDoneT -= dt; if (scanDoneT <= 0.f) scanT = 0.f; }
    if (scanKind == 0) return;
    float wx, wy, wz; int px, py; float d;
    if (!scanTarget(wx, wy, wz) || !renderProject(wx, wy, wz, px, py, d) || d > 40.f || px < 0 || px >= 320 || py < 0 || py >= 240) {
        scanKind = 0; scanT = 0.f; message("SIGNAL LOST", 1.5f); hx::pop(0.15f, 0.02f);
        return;
    }
    scanPx = px; scanPy = py;
    scanT += dt / 1.3f;
    scanTick += dt;
    if (scanTick > 0.14f) { scanTick = 0.f; sfxUi(UI_SCAN_TICK); hx::scanTick(scanT); }
    if (scanT >= 1.f) {
        bool fresh = databankAdd(scanSp);
        sfxUi(fresh ? UI_SCAN_NEW : UI_SCAN_OK);
        hx::scanDone(fresh);
        cardOpen(CARD_SCAN, scanSp, fresh);
        cardT = 4.5f;
        scanKind = 0; scanT = 1.01f; scanDoneT = 0.8f;
    }
}

static void sonarPing() {
    using namespace mantis;
    g_scene.sonarAge = 0.f;
    g_scene.sonarRange = 70.f;
    // the echo: off the seabed below and ahead, or anything solid in front
    float best = fmaxf(1.f, g_scene.altitude);
    float cy = cosf(helmYaw()), sy = sinf(helmYaw());
    for (float r = 4.f; r < 80.f; r += 4.f) {
        float fz = terrainFloor(subX + cy * r, subY + sy * r);
        if (fz > subZ - r * 0.2f) { best = fminf(best, r); break; }
    }
    float delay = fminf(1.4f, best / 55.f + 0.05f);
    float str = fmaxf(0.f, 1.f - best / 90.f);
    sfxSonar(delay, str);
    hx::sonar();
    echoAt = g_scene.t + delay; echoStr = str;
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;
    cfg.internal_spk = true;
    cfg.internal_mic = false;                    // the Core2 shares one I2S port: the speaker keeps it
    M5.begin(cfg);
    mantis::audioSpeakerBegin();                 // claim the speaker's DMA buffers before anything else allocates
    mantis::audioBegin();                        // ...and the sound task's stack and buffers: internal RAM goes first come, first served
    M5.Display.setRotation(1);
    M5.Display.setBrightness(110);
    if (M5.Imu.isEnabled()) {
        M5.Imu.loadOffsetFromNVS();
        for (int i = 0; i < 15; i++) { M5.Imu.update(); delay(4); }
    }
    M5.BtnA.setHoldThresh(600);
    M5.BtnB.setHoldThresh(600);
    M5.BtnC.setHoldThresh(600);

    // A new sea every time you wake it, but never one that starts you inside a rock.
    float yaw0 = 0.f;
    for (int tries = 0; tries < 12; tries++) {
        mantis::universeInit(esp_random() | 1u);
        mantis::journeyInit(mantis::worldSeed(), 0.f);
        float fl = mantis::terrainShape(0.f, 0.f, 0.f, nullptr);
        if (fl < -9.f) break;
    }
    mantis::terrainInit(subX, subY, distanceTravelled);
    float fl = mantis::terrainFloor(subX, subY);
    subZ = fminf(-4.f, fl + 0.55f * (-fl));                       // start in the middle of the water, near the light
    mantis::helmInit(yaw0);
    mantis::renderInit();
    mantis::fieldInit(subX, subY, yaw0, distanceTravelled);
    mantis::sceneInit();
    mantis::displayInit();
    hx::begin();
    mantis::databankLoad();
    if (mantis::databankMuted()) { mantis::audioSetMuted(true); bootMuted = true; }
    mantis::sfxMoment(mantis::MOMENT_INTRO);
    hx::heartbeat();
}

void loop() {
    using namespace mantis;
    static uint32_t prev = micros();
    uint32_t now = micros();
    float dt = (now - prev) / 1e6f;
    if (dt > 0.05f) dt = 0.05f;
    if (dt < 0.001f) dt = 0.001f;
    prev = now;
    const float t = millis() / 1000.0f;
    fpsAvg += (1.f / dt - fpsAvg) * 0.05f;

    // ---------------- the helm ----------------
    HelmEvents ev;
    bool modal = cardModal();
    helmUpdate(dt, !modal, ev);
    if (cardKind() != CARD_NONE) {
        bool close = ev.tap || ev.aClick || ev.bClick || ev.cClick || ev.aHold || ev.bHold || ev.cHold;
        if (cardKind() == CARD_SCAN) { cardT -= dt; if (cardT <= 0.f) close = true; }
        if (close) { cardClose(); ev = HelmEvents(); if (modal) hx::pop(0.15f, 0.01f); }
    }
    if (ev.longPress) {
        bool m = !audioMuted();
        audioSetMuted(m); databankSetMuted(m); hx::confirmMute(m); muteIconAt = millis();
    }
    if (!modal) {
        if (ev.aHold) { cardOpen(CARD_CHART, 0, false); sfxUi(UI_CARD); hx::pop(0.25f, 0.02f); }
        else if (ev.aClick) sonarPing();
        if (ev.cHold) { cardOpen(CARD_BANK, 0, false); sfxUi(UI_CARD); hx::pop(0.25f, 0.02f); }
        else if (ev.cClick) { helmSetThrottle(0.5f); message("CRUISE", 0.9f); sfxUi(UI_CRUISE); hx::pop(0.2f, 0.02f); }
        if (ev.bHold) { lightsOn = !lightsOn; lightsAuto = false; sfxUi(lightsOn ? UI_LIGHTS_ON : UI_LIGHTS_OFF); message(lightsOn ? "FLOODLIGHTS ON" : "FLOODLIGHTS OFF", 1.f); hx::pop(0.3f, 0.03f); }
        else if (ev.bClick) { helmLevel(); message("LEVEL", 0.9f); sfxUi(UI_LEVEL); hx::pop(0.2f, 0.02f); }
        if (ev.tap) tryScan(ev.tapX, ev.tapY);
    }
    // floodlights come on by themselves as the light goes, until you choose
    if (lightsAuto) lightsOn = g_scene.light < 0.22f || g_scene.depth > 60.f;

    // ---------------- move ----------------
    float want = modal ? 0.f : helmSpeedWanted();
    subSpeed += (want - subSpeed) * (1.f - expf(-dt / SPEED_TAU));
    float yaw = helmYaw(), pitch = helmPitch();
    float cp = cosf(pitch);
    float vx = cosf(yaw) * cp * subSpeed, vy = sinf(yaw) * cp * subSpeed, vz = sinf(pitch) * subSpeed;
    float bumpMax = 0.f, movedH = 0.f;
    float left = 1.f;
    int steps = (int)(subSpeed * dt / 0.25f) + 1;
    for (int k = 0; k < steps && left > 0.f; k++) {
        float bx = subX, by = subY, bump = 0.f;
        fieldMove(subX, subY, subZ, vx * dt / steps, vy * dt / steps, vz * dt / steps, &bump);
        movedH += sqrtf((subX - bx) * (subX - bx) + (subY - by) * (subY - by));
        bumpMax = fmaxf(bumpMax, bump);
    }
    bool bumped = bumpMax > 0.25f;
    if (bumped && !wasBumped) { hx::bump(bumpMax); sfxBump(bumpMax); subSpeed *= 0.35f; fieldScare(subX, subY, subZ, 12.f); }
    wasBumped = bumped;
    distanceTravelled += movedH;
    journeyUpdate(distanceTravelled, yaw, movedH, -subZ);
    terrainUpdate(subX, subY, distanceTravelled);
    float realSpeed = movedH / dt;
    sceneUpdate(t, dt, subX, subY, subZ, yaw, pitch, distanceTravelled, fmaxf(realSpeed, subSpeed * 0.9f), helmThrottle());
    Scene& sc = g_scene;
    sc.lights = lightsOn;
    fieldTick(subX, subY, subZ, yaw, distanceTravelled, t, dt, subSpeed);
    databankNoteDepth(sc.depth);
    serviceScan(dt);

    // ---------------- places, and the life you can hear ----------------
    if (sc.biome != lastBiome) {
        if (lastBiome != 255) { bannerText = biomeName(sc.biome); bannerT = 5.f; }
        int z = (int)(biomeDef(sc.biome).zone + 0.5f);
        if (lastBiome != 255 && z > lastZone) sfxMoment(MOMENT_DEEPER);
        lastZone = z;
        lastBiome = sc.biome;
    }
    if (t > 3.f && t < 3.1f && !bannerText) { bannerText = biomeName(sc.biome); bannerT = 5.f; }
    if (t > 8.5f && t < 8.6f) {
        if (bootMuted && audioMuted()) message("SOUND OFF - HOLD THE SCREEN TO TURN ON", 5.f);
    }
    critterSoundT -= dt;
    if (critterSoundT <= 0.f) {
        critterSoundT = 5.f + (esp_random() % 1000) * 0.008f;
        const Critter* cr = fieldCritters();
        for (int i = 0; i < CRIT_N; i++) {
            if (!cr[i].on) continue;
            float dx = cr[i].x - subX, dy = cr[i].y - subY, d = sqrtf(dx * dx + dy * dy);
            if (cr[i].kind == CR_DOLPHIN && d < 50.f) { sfxCritter(CRIT_DOLPHIN, d); break; }
            if (cr[i].kind == CR_SERPENT && d < 60.f) { sfxCritter(CRIT_SERPENT, d); hx::eruption(0.4f); break; }
            if (cr[i].kind == CR_NARWHAL && d < 50.f) { sfxCritter(CRIT_NARWHAL, d); break; }
            if (cr[i].kind == CR_WHALE && d < 80.f) { sfxCritter(CRIT_WHALE, d); hx::swell(); break; }
        }
    }
    // the hull feels the pressure past a hundred metres
    if (sc.depth > 100.f) {
        creakT -= dt * (0.5f + sc.depth / 200.f);
        if (creakT <= 0.f) { creakT = 12.f + (esp_random() % 1000) * 0.02f; sfxUi(UI_CREAK); hx::creak(sc.depth / 350.f); }
    }

    // ---------------- the motor ----------------
    hx::engine(helmThrottle(), subSpeed);
    hx::rumble(sc.ventNear * sc.ventRumble * 0.45f + sc.eruption * 0.7f);
    int beat = (int)floorf(audioBeats());
    if (beat != lastBeat) { lastBeat = beat; float k = audioKick(); if (k > 0.1f) hx::kick(k); }
    if (g_thunder.pending && t >= g_thunder.at) { hx::thunder(g_thunder.power); g_thunder.pending = false; }
    if (echoAt > 0.f && t >= echoAt) { if (echoStr > 0.05f) hx::echo(echoStr); echoAt = -1.f; }
    static float lastAwe = 0.f, lastErupt = 0.f;
    if (sc.awe > 0.6f && lastAwe <= 0.6f) hx::swell();
    if (sc.eruption > 0.3f && lastErupt <= 0.3f) hx::eruption(1.f);
    lastAwe = sc.awe; lastErupt = sc.eruption;
    hx::update(dt);

    // ---------------- the picture ----------------
    if (bannerT > 0.f) bannerT -= dt;
    if (msgT > 0.f) msgT -= dt;
    g_hud.throttle = helmThrottle();
    g_hud.scanT = scanKind ? scanT : (scanDoneT > 0.f ? 1.01f : 0.f);
    g_hud.scanX = scanPx; g_hud.scanY = scanPy;
    if (msgT > 0.f) { g_hud.banner = msgText; g_hud.bannerA = msgT > 0.3f ? 1.f : msgT / 0.3f; }
    else { g_hud.banner = bannerText; g_hud.bannerA = bannerT > 4.f ? 5.f - bannerT : bannerT < 1.f ? fmaxf(0.f, bannerT) : 1.f; }
    g_hud.hintA = t < 4.f ? 0.f : t < 16.f ? 1.f : fmaxf(0.f, 1.f - (t - 16.f) / 2.f);
    g_hud.showFps = showFps; g_hud.fps = (int)(fpsAvg + 0.5f);
    g_hud.muteIcon = muteIconAt != 0; g_hud.muted = audioMuted();
    if (muteIconAt) {
        float age = (millis() - muteIconAt) / 1000.f;
        g_hud.muteA = age < 1.2f ? 1.f : 1.f - (age - 1.2f) / 0.8f;
        if (g_hud.muteA <= 0.f) muteIconAt = 0;
    }
    renderFrame(subX, subY, subZ, yaw, pitch, t);
    displayPresent();
}
