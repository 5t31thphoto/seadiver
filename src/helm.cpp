// Size over speed here: this code runs per object or per frame, not per pixel or per sample, and the app shares
// flash with others (see README, "Footprint").
#pragma GCC optimize("Os")
#include <stdlib.h>
#include "helm.h"
#include "haptics.h"
#include <M5Unified.h>
#include <math.h>

namespace mantis {

// ======================= TUNING: how she handles =======================
// Tilt is read as an angle from the pose you held at the start (or when you last pressed B), so it works the
// same whether you hold the Core2 upright, reclined or flat.
// Stick: past the deadzone, the tilt ramps to full stick at FULL_DEG (expo in between).
constexpr float PITCH_DEADZONE_DEG = 3.f;     // tip less than this: nothing (a resting hand holds course)
constexpr float PITCH_FULL_DEG     = 22.f;    // tip this far past the deadzone: full dive / climb
constexpr float YAW_DEADZONE_DEG   = 3.f;
constexpr float YAW_FULL_DEG       = 22.f;
constexpr float STICK_EXPO         = 0.55f;   // 0 = linear, 1 = all expo (gentle near centre, sharp at the edge)
constexpr bool  INVERT_PITCH       = false;   // true: tip the top away to climb (a flight-sim "pull back")
constexpr bool  INVERT_YAW         = false;
// Response: tilt sets how fast she turns. Bigger rates = quicker; bigger mass = heavier, slower to start and stop.
constexpr float TURN_RATE      = 1.10f;       // rad/s of yaw at full stick
constexpr float PITCH_RATE     = 0.95f;       // rad/s of pitch at full stick
constexpr float ATTITUDE_MASS  = 0.24f;       // seconds to answer the stick (the heavy feel)
constexpr float STICK_SMOOTH   = 0.08f;       // seconds of smoothing on the raw tilt
constexpr float MAX_PITCH      = 1.35f;       // rad (about 77 degrees) nose up or down: steep enough for a drop-off
constexpr float LEVEL_TIME     = 0.35f;       // seconds for B to bring the nose level
// Slide on the screen: radians per panel pixel of finger travel.
constexpr float SLIDE_YAW      = 0.0095f;
constexpr float SLIDE_PITCH    = 0.011f;
// Touch timings.
constexpr uint32_t LONG_PRESS_MS = 1000;      // hold still this long for sound on/off
constexpr uint32_t TAP_MS        = 450;       // shorter than this (and still) is a tap: scan
// Also: speed is CRUISE_MS / FLANK_MS in helm.h, how quickly she reaches it is SPEED_TAU in main.cpp, and how
// far the view tilts with the nose is VIEW_PITCH_MAX in render.cpp.
// =======================================================================

static float s_yaw = 0.f, s_pitch = 0.f, s_throttle = 0.5f;
static float s_rateYaw = 0.f, s_ratePitch = 0.f;
static float s_tiltX = 0.f, s_tiltY = 0.f;
static float s_neutralPitch = 0.f, s_neutralRoll = 0.f;     // radians
static bool s_neutralValid = false;
static int s_settle = 0;
static float s_levelT = 0.f;
// touch
static bool s_touchDown = false, s_dragging = false, s_sliding = false, s_longFired = false;
static int s_x0 = 0, s_y0 = 0, s_lx = 0, s_ly = 0;
static uint32_t s_t0 = 0;

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void helmInit(float yaw) {
    s_yaw = yaw; s_pitch = -0.08f; s_throttle = 0.5f;
    s_rateYaw = s_ratePitch = 0.f; s_tiltX = s_tiltY = 0.f;
    s_neutralValid = false; s_settle = 0; s_levelT = 0.f;
    s_touchDown = s_dragging = s_sliding = s_longFired = false;
}

float helmYaw() { return s_yaw; }
float helmPitch() { return s_pitch; }
float helmThrottle() { return s_throttle; }
float helmTurnRate() { return s_rateYaw; }
float helmPitchRate() { return s_ratePitch; }
bool helmDragging() { return s_dragging; }
void helmSetThrottle(float t) { s_throttle = clampf(t, 0.f, 1.f); }
void helmSetAttitude(float yaw, float pitch) { s_yaw = yaw; s_pitch = pitch; s_rateYaw = s_ratePitch = 0.f; }
void helmNudge(float dyaw, float dpitch) { s_yaw += dyaw; s_pitch = clampf(s_pitch + dpitch, -1.2f, 1.2f); }

// 0 .. 0.5 ramps stop .. cruise, 0.5 .. 1 ramps cruise .. flank
float helmSpeedWanted() {
    return s_throttle <= 0.5f ? CRUISE_MS * (s_throttle / 0.5f) : CRUISE_MS + (FLANK_MS - CRUISE_MS) * (s_throttle - 0.5f) * 2.f;
}

// The device's attitude from gravity: pitch is the tip about its horizontal axis (top away = +), roll the lean.
// atan2 keeps both meaningful at any holding angle (raw accel.y is symmetric near upright: tipping either way
// lowers it, which is why tipping back once read as another dive).
static void attitudeFrom(float ax, float ay, float az, float& pitch, float& roll) {
    pitch = atan2f(az, ay);
    roll = atan2f(ax, sqrtf(ay * ay + az * az));
}
static void captureNeutral() {
    if (M5.Imu.update()) {
        auto d = M5.Imu.getImuData();
        attitudeFrom(d.accel.x, d.accel.y, d.accel.z, s_neutralPitch, s_neutralRoll);
        s_neutralValid = true;
    }
}

void helmLevel() {
    captureNeutral();
    s_tiltX = s_tiltY = 0.f; s_rateYaw = s_ratePitch = 0.f;
    s_levelT = 1.2f;
}

static void setThrottleFromY(int y) {
    float t = 1.f - (float)(y - SLIDER_Y0) / (SLIDER_Y1 - SLIDER_Y0);
    t = clampf(t, 0.f, 1.f);
    if (fabsf(t - 0.5f) < 0.04f) t = 0.5f;                      // a soft detent at cruise
    if ((s_throttle < 0.5f) != (t < 0.5f) || (s_throttle == 0.5f) != (t == 0.5f)) hx::pop(0.15f, 0.01f);
    s_throttle = t;
}

void helmUpdate(float dt, bool flying, HelmEvents& ev) {
    M5.update();
    ev = HelmEvents();
    // ---------------- touch ----------------
    auto td = M5.Touch.getDetail();
    uint32_t now = millis();
    if (td.wasPressed()) {
        s_touchDown = true; s_dragging = false; s_sliding = false; s_longFired = false;
        s_x0 = s_lx = td.x; s_y0 = s_ly = td.y; s_t0 = now;
        if (flying && td.x >= SLIDER_X - 6 && td.y >= SLIDER_Y0 - 12 && td.y <= SLIDER_Y1 + 12) { s_sliding = true; setThrottleFromY(td.y); }
    }
    if (s_touchDown && td.isPressed()) {
        int dx = td.x - s_lx, dy = td.y - s_ly;
        if (s_sliding) setThrottleFromY(td.y);
        else {
            if (!s_dragging && (abs(td.x - s_x0) + abs(td.y - s_y0)) > 7 && flying && !s_longFired) s_dragging = true;
            if (s_dragging && flying) {
                // slide = steer directly: yaw and pitch, the world moves with your finger
                s_yaw += dx * SLIDE_YAW;
                s_pitch = clampf(s_pitch + dy * SLIDE_PITCH, -MAX_PITCH, MAX_PITCH);
                s_levelT = 0.f;
            }
            if (!s_dragging && !s_longFired && td.y < 240 && now - s_t0 > LONG_PRESS_MS) { s_longFired = true; ev.longPress = true; }
        }
        s_lx = td.x; s_ly = td.y;
    }
    if (td.wasReleased()) {
        if (s_touchDown && !s_dragging && !s_sliding && !s_longFired && now - s_t0 < TAP_MS) { ev.tap = true; ev.tapX = s_x0; ev.tapY = s_y0; }
        s_touchDown = false; s_dragging = false; s_sliding = false;
    }

    // ---------------- buttons (quick taps act on release, so a hold never also does the tap) ----------------
    ev.aHold = M5.BtnA.wasHold(); ev.aClick = M5.BtnA.wasClicked();
    ev.bHold = M5.BtnB.wasHold(); ev.bClick = M5.BtnB.wasClicked();
    ev.cHold = M5.BtnC.wasHold(); ev.cClick = M5.BtnC.wasClicked();
    ev.anyButton = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed();

    // ---------------- tilt ----------------
    if (!s_neutralValid) {                                        // let the IMU settle, then take the pose you hold
        if (++s_settle > 8) captureNeutral();
    } else if (M5.Imu.update()) {
        auto d = M5.Imu.getImuData();
        float p, r;
        attitudeFrom(d.accel.x, d.accel.y, d.accel.z, p, r);
        float dp = p - s_neutralPitch, dr = r - s_neutralRoll;
        if (dp > 3.14159265f) dp -= 6.2831853f; else if (dp < -3.14159265f) dp += 6.2831853f;
        // degrees -> stick: a deadzone, then expo up to full
        auto shape = [](float rad, float dz, float full) {
            float a = fabsf(rad) * 57.29578f;
            if (a < dz) return 0.f;
            float u = clampf((a - dz) / full, 0.f, 1.25f);
            return (rad < 0.f ? -1.f : 1.f) * (u * (1.f - STICK_EXPO) + u * u * STICK_EXPO);
        };
        float k0 = 1.f - expf(-dt / STICK_SMOOTH);
        float sp = shape(dp, PITCH_DEADZONE_DEG, PITCH_FULL_DEG) * (INVERT_PITCH ? -1.f : 1.f);   // + = top away = dive
        float sy = shape(dr, YAW_DEADZONE_DEG, YAW_FULL_DEG) * (INVERT_YAW ? -1.f : 1.f);
        s_tiltY += (sp - s_tiltY) * k0;
        s_tiltX += (sy - s_tiltX) * k0;
    }
    if (!flying) { s_rateYaw = s_ratePitch = 0.f; return; }
    // attitude: tilt sets the rates, with a submarine's mass
    float k = 1.f - expf(-dt / ATTITUDE_MASS);
    s_rateYaw += (-s_tiltX * TURN_RATE - s_rateYaw) * k;              // lean right: turn right (SpaceMantis's sense)
    s_ratePitch += (-s_tiltY * PITCH_RATE - s_ratePitch) * k;          // tip the top away: nose down (pitch is nose-up +)
    s_yaw += s_rateYaw * dt;
    s_pitch += s_ratePitch * dt;
    if (s_levelT > 0.f) {                                         // B: the trim brings her level
        s_levelT -= dt;
        s_pitch += (0.f - s_pitch) * (1.f - expf(-dt / LEVEL_TIME));
    }
    s_pitch = clampf(s_pitch, -MAX_PITCH, MAX_PITCH);
    if (s_yaw > 1000.f || s_yaw < -1000.f) s_yaw = fmodf(s_yaw, 6.28318531f);
}

}
