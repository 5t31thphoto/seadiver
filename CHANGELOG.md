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
