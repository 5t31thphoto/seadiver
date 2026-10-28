# Changelog

## 1.0.0
- Rebuilt from MantisPlanet's engine (replacing the earlier integration stub): dreamt voxel seabed, pitchable
  camera, depth-lit palette, 17 marine biomes with depth and heading directors, 16 creatures and fish schools,
  scanning and a persistent databank, ocean soundscape, haptics, SpaceMantis controls.
- Host tools: snapshot sheet of every biome, soak test with a bot pilot, static RAM check, audio previews.

## 1.0.1
- Pinned the whole build to the first green build: espressif32 6.9.0, Arduino framework 3.20017.241212,
  M5Unified 0.2.28, M5GFX 0.2.35; CI on ubuntu-24.04 with exact action versions, Python 3.11.17, esptool 5.6.0,
  PlatformIO 6.1.18; esp-web-tools 10.0.1.
- CI prints the libraries it used and the site URL (in the log and the run summary).
- Website: a download link for the merged firmware, like MantisPlanet's and SpaceMantis's.

## 1.0.2
- Audio: the dub echo's buffer was shorter than a dotted eighth at the score's 84 BPM (it read from the wrong
  place); now sized from the tempo and clamped. Its fading tail no longer runs into denormals: the sound costs
  1.2 ms of CPU per second of audio on the host (was 7.3; MantisPlanet's is 2.5).

## 1.0.3
- Sound: the audio task now starts first in setup(), before the seabed and renderer claim internal RAM (its
  stack can't fall back to PSRAM; if it ever fails to start, the screen says so). Remembered mute is announced
  at boot. Long-press for mute is 1 s (a resting finger before a slide no longer mutes by accident).
- Smoother frames: no expf/atanf per pixel or per seabed sample (lookup tables); fast sine in the props; new
  seabed cells are dreamt 56 a frame instead of a whole column at once; the deepest-depth record no longer
  writes to flash mid-dive (flash writes pause both cores).
- Helm: tuning constants at the top of helm.cpp (and SPEED_TAU in main.cpp); a little quicker on pitch and yaw.
- Pinned to MantisPlanet's working libraries: M5Unified 0.2.26, M5GFX 0.2.33. LCD clock 40 MHz (MP_LCD_MHZ).
- tools/host/listen.sh: a 70 s listening tour -> snaps/tour.mp3.

## 1.0.4
- Every sound is tagged; tools/host/stems.sh renders each one soloed (33 stems, with an index) -> snaps/stems.
  The solo gate exists only in the desktop build (on the device it's a constant 1).
- Levels from the audit: seal trills and dolphin whistles x2.2, the sea's breath x1.75, the choir 0.44x.
- Tools: teleporting along the journey now catches the director up first (it was re-picking the forced biome),
  so snapshots, the listening tour and the stems are in the places they say.

## 1.1.0
- Helm: tilt is read as an angle (atan2 of gravity), so climbing works however you hold it. Raw accel.y is
  symmetric near upright, so tipping back read as another dive. Tilt deadzone and full stick are in degrees;
  invert switches; pitch up to 77 degrees, and the view now follows the nose to 60 (VIEW_PITCH_MAX).
- Freezes: rock and coral shaders and the temple, wall and column loops only touch on-screen pixels; a near
  plane fades things out between 1.6 and 0.9 m from the lens (kelp and grass excepted). The worst frame in the
  stress test went from 8x the baseline to 2x. The seabed takes fewer far samples and stops where it's all water.
- Footprint: per-object and per-frame code compiled for size; our code is ~24% smaller.
- Sound, a material pass: rain is a muffled hiss (no drops); bubbles are big low glugs (~180-620 Hz, were 1-5 kHz)
  with a soft onset; shrimp are a quieter sizzle; every ping fades in (no clicks); sonar, scanner, UI, chimes,
  creaks, bump, seals, dolphins and narwhals an octave or so lower. Level and pitch per sound at the top of
  ocean.cpp.
