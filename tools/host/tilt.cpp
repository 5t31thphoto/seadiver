// Tilt check: from upright, reclined and flat holds, tipping away must dive and tipping back must climb;
// leaning right must turn right. Fails loudly otherwise.
#include "harness_core.inc"
using namespace mantis;
static void hold(float tipDeg, float leanDeg, int frames) {
  float t = tipDeg * 0.0174533f, l = leanDeg * 0.0174533f;
  for (int i = 0; i < frames; i++) {
    g_us += 33333;
    M5.Imu.data.accel = {-sinf(l), cosf(t) * cosf(l), sinf(t) * cosf(l)};   // top away = +tip, right lean = -x
    loop(); M5.BtnA.clear(); M5.BtnB.clear(); M5.BtnC.clear();
  }
}
int main() {
  setup();
  int fails = 0;
  for (float base : {5.f, 40.f, 85.f}) {                 // upright, reclined, nearly flat
    for (int dir : {+1, -1}) {
      hold(base, 0.f, 40);                                // settle; B takes this pose as neutral
      M5.BtnB.clicked = true; hold(base, 0.f, 1); hold(base, 0.f, 40);
      helmSetAttitude(helmYaw(), 0.f);
      hold(base + dir * 15.f, 0.f, 45);                   // tip 15 degrees for 1.5 s
      float p = helmPitch();
      bool ok = dir > 0 ? p < -0.3f : p > 0.3f;
      printf("hold %2.0f deg, tip %s: pitch %+.2f rad %s\n", base, dir > 0 ? "away  " : "toward", p, ok ? "ok" : "<< WRONG");
      fails += !ok;
    }
    hold(base, 0.f, 30); helmSetAttitude(0.f, 0.f);
    float y0 = helmYaw(); hold(base, 15.f, 45);
    bool ok = helmYaw() - y0 > 0.3f;
    printf("hold %2.0f deg, lean right: yaw %+.2f rad %s\n", base, helmYaw() - y0, ok ? "ok" : "<< WRONG"); fails += !ok;
  }
  printf(fails ? "TILT FAILED\n" : "TILT PASS\n");
  return fails ? 1 : 0;
}
