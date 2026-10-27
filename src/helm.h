#pragma once
#include <stdint.h>

// The helm: SpaceMantis's flight controls, fitted to a submarine.
//  * Tilt aims the nose, relative to a neutral pose captured when you start (and whenever you press B):
//    tip the top away to dive, toward you to climb, lean left / right to turn. Tilt sets how fast she turns,
//    with a little mass, so a resting hand holds course.
//  * Slide a finger across the screen to steer directly (yaw and pitch, 1:1, like grabbing the view).
//  * The throttle is the slider on the right edge: drag it. A soft detent at cruise; bottom is all stop,
//    top is flank speed.
//  * Tap: scan whatever is under your finger. Long-press (still) anywhere: sound off / on.
//  * A: sonar ping.  Hold A: the chart (where you are).
//  * B: level off and re-centre the tilt pose.  Hold B: floodlights on / off.
//  * C: throttle to cruise.  Hold C: the databank (everything you've scanned).

namespace mantis {

constexpr int SLIDER_X = 320 - 22, SLIDER_Y0 = 44, SLIDER_Y1 = 196;   // panel pixels

struct HelmEvents {
    bool tap; int tapX, tapY;        // panel pixels
    bool longPress;                  // mute toggle
    bool aClick, aHold, bClick, bHold, cClick, cHold;
    bool anyButton;
};

void helmInit(float yaw);
// Reads M5 (call once per frame; it calls M5.update()). `flying` false while a card is up: no steering.
void helmUpdate(float dt, bool flying, HelmEvents& ev);

float helmYaw();              // heading, rad (east = 0, grows to the right)
float helmPitch();            // nose up +, rad
float helmThrottle();         // 0 stop, 0.5 cruise, 1 flank
float helmSpeedWanted();      // m/s
float helmTurnRate();         // rad/s (for the reticle and the motor)
float helmPitchRate();
bool  helmDragging();
void  helmSetThrottle(float t);
void  helmLevel();            // B: ease the nose level and capture a new neutral pose
void  helmSetAttitude(float yaw, float pitch);   // tests / previews
void  helmNudge(float dyaw, float dpitch);       // collisions push the nose

constexpr float CRUISE_MS = 5.f, FLANK_MS = 10.f;

}
