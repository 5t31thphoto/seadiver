// Shared host runtime: clock, M5 stub instance, then the real firmware main.
#include "M5Unified.h"
#include <cstdlib>
#include <cmath>
#include <vector>
static uint64_t g_us = 1000000;
uint32_t millis() { return (uint32_t)(g_us / 1000); }
uint32_t micros() { return (uint32_t)g_us; }
void delay(uint32_t ms) { g_us += (uint64_t)ms * 1000; }
M5Class M5;
uint32_t esp_random() { static uint32_t x = 0x9E3779B9u; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
M5Canvas* g_hostLastCanvas = nullptr;
#include "main.cpp"
namespace mantis {
void fieldDebugSpawn(uint8_t kind, float yaw, float dist, float dz);
void fieldDebugSchool(uint8_t kind, float yaw, float dist, bool bait);
void fieldDebugClear();
}
static std::vector<int16_t> g_wav;
static bool g_recordAudio = false;
static double g_audioDebt = 0.0;

// The unit held at a comfortable angle (the neutral pose): gravity along y and z.
static const float NEUTRAL_AY = 0.70f, NEUTRAL_AZ = 0.71f;
// One firmware frame. stickX: lean (+ right), stickY: tip (+ top away = dive), in g of accel offset.
static void frame(float stickX = 0.f, float stickY = 0.f, uint32_t frameUs = 33333) {
  g_us += frameUs;
  M5.Imu.data.accel = {-stickX, NEUTRAL_AY - stickY, NEUTRAL_AZ};
  M5.Imu.data.gyro = {0, 0, 0};
  loop();
  M5.BtnA.clear(); M5.BtnB.clear(); M5.BtnC.clear();
  g_audioDebt += frameUs * (mantis::AUDIO_SR / 1e6);
  int16_t blk[256];
  while (g_audioDebt >= 256.0) {
    mantis::audioRender(blk, 256);
    if (g_recordAudio) g_wav.insert(g_wav.end(), blk, blk + 256);
    g_audioDebt -= 256.0;
  }
}
static void writeWav(const char* path) {
  FILE* f = fopen(path, "wb");
  uint32_t n = (uint32_t)g_wav.size(), bytes = n * 2, sr = mantis::AUDIO_SR, br = sr * 2, sz = 36 + bytes;
  fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  uint32_t fmtLen = 16; uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  fwrite(&fmtLen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
  fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
  fwrite(g_wav.data(), 2, n, f); fclose(f);
}
static void writePPM(const char* path) {   // exactly what the screen shows (frame, palette, overlay)
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6 320 240 255\n");
  uint16_t row[320];
  for (int y = 0; y < 240; y++) {
    mantis::compositeRow(y, row);
    for (int x = 0; x < 320; x++) { uint16_t v = row[x]; unsigned char c[3] = {(unsigned char)((v >> 11) << 3), (unsigned char)(((v >> 5) & 63) << 2), (unsigned char)((v & 31) << 3)}; fwrite(c, 1, 3, f); }
  }
  fclose(f);
}
