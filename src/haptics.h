#pragma once
#include <stdint.h>

// The motor is the low end of the sound system: the Core2 speaker can't move air below ~200 Hz, so the
// weight of the sea is felt instead. Taps, a continuous rumble and keyframed gestures are max-mixed,
// remapped above the motor's dead zone, and written only when the level really changes.

namespace hx {
void begin();
void pop(float level, float seconds);             // a short tap (detents, targeting, UI)
void bump(float strength);                        // the hull met something
void kick(float level);                           // the groove's kick, felt in the hands
void swell();                                     // a slow breath for a moment of awe
void heartbeat();                                 // lub-dub (the dive begins)
void confirmMute(bool muted);
void sonar();                                     // the ping leaves the hull
void echo(float strength);                        // ...and comes back
void creak(float depthK);                         // the hull complaining at depth
void thunder(float power);                        // a storm far above, muffled by the water
void scanTick(float level);                       // the scanner reading
void scanDone(bool fresh);                        // logged (a fresh entry gets a flourish)
void eruption(float power);                       // the volcano breathes
void engine(float throttle, float speed);         // continuous: the screw turning, per frame
void rumble(float amount);                        // continuous: vents, turbulence, per frame
void update(float dt);
}
