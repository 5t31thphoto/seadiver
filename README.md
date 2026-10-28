# MANTIS SEADIVER — a submarine through an ocean that dreams itself

The underwater sibling of **MantisPlanet**, flown with **SpaceMantis**'s controls, on the **M5Stack Core2**.
The sea is hallucinated ahead of you as you go — the seabed, the light, the life — and nothing behind you is
kept. Dive from sunlit reefs through the twilight zone into the midnight dark, where the only light is your
floodlights and the things that make their own.

## Build & install

* **CI**: push this folder (or a zip of it) to the repo. `GitHub/workflows/ci.yml` (move it to `.github/workflows/` in the repo) unpacks a root zip over the
  tree, builds with PlatformIO (`pio run -e m5stack-core2`), merges `firmware-merged.bin` and deploys the web
  installer in `website/`.
* **Local**: `pio run -e m5stack-core2 -t upload`.
* **Browser**: open the GitHub Pages site in Chrome / Edge and click Install (esp-web-tools).

## Controls

Hold the Core2 comfortably when it starts: that pose is neutral (B re-takes it any time).

| Input | Does |
|---|---|
| **Tilt** top away / toward you | nose down (dive) / nose up (climb) |
| **Tilt** left / right | turn left / right. Tilt sets the *rate*, with a submarine's weight; a resting hand holds course |
| **Slide** a finger on the screen | steer directly: yaw and pitch follow your finger |
| **Throttle slider** (right edge) | drag up / down. Soft click at cruise (5 m/s); bottom all stop, top flank (10 m/s) |
| **Tap** something | scan it (creature, coral, sponge, vent, crystal, glyph stone...): keep it in view while the brackets close |
| **Long press** the screen (1 s, still) | sound off / on (remembered across power-off) |
| **A** | sonar ping: the echo is heard and felt; creatures it finds are bracketed |
| **Hold A** | the chart: place, zone, depth, deepest, distance, heading, which way the sea is taking you |
| **B** | level off, and re-take the neutral tilt pose |
| **Hold B** | floodlights on / off (otherwise they come on by themselves as the light goes) |
| **C** | throttle to cruise |
| **Hold C** | the databank: everything you've scanned |
| Any button or a tap | closes a card (a scan card also closes by itself) |

The sub meets the seabed: a gentle slope lifts you over it, a wall stops you with a clank in your hands. The
surface and the ice shelf are ceilings.

## Tuning

Every knob is a named constant with a comment, in one block at the top of its file:

| What | Where | Constants |
|---|---|---|
| Tilt feel | `src/helm.cpp` | `PITCH_DEADZONE_DEG`, `PITCH_FULL_DEG`, `YAW_DEADZONE_DEG`, `YAW_FULL_DEG`, `STICK_EXPO`, `INVERT_PITCH`, `INVERT_YAW` |
| Response and weight | `src/helm.cpp` | `TURN_RATE`, `PITCH_RATE`, `ATTITUDE_MASS`, `STICK_SMOOTH`, `MAX_PITCH`, `LEVEL_TIME` |
| Slide and touch | `src/helm.cpp` | `SLIDE_YAW`, `SLIDE_PITCH`, `LONG_PRESS_MS`, `TAP_MS` |
| Speed | `src/helm.h`, `src/main.cpp` | `CRUISE_MS`, `FLANK_MS`; `SPEED_TAU` |
| How far the view tilts | `src/render.cpp` | `VIEW_PITCH_MAX` |
| Every sound's level and pitch | `src/ocean.cpp` | `LVL_*`, `PITCH_*`, `BUBBLE_RATE` (render them with `tools/host/stems.sh`) |

## Footprint

The firmware shares flash with other apps under a launcher, so it's kept small:

* Seadiver has no assets: every picture and sound is made live, so there's nothing to keep on the SD card. The
  ESP32 can only run code from flash, so the SD card can't hold the program itself either.
* Code that runs per object or per frame is compiled for size (`#pragma GCC optimize("Os")` at the top of those
  files); the per-pixel and per-sample paths (seabed, water, audio, display) stay compiled for speed. That cut our
  own code by about a quarter for ~3% on the object pass.
* Most of the binary is the Arduino framework and the M5 libraries (drivers for every M5 board, autodetected at
  start), which every app on the launcher carries. Newer M5GFX releases can be told the board up front and skip
  that; they're after the 0.2.33 we're pinned to (MantisPlanet's working version), so that's a later step.


## How the dream works

* **The seabed** (`terrain.*`): a 128×128 grid of 2 m cells (±128 m) that scrolls with you. A cell is *dreamt*
  when it comes into range, from the biome at *(distance travelled + its range)* — MantisPlanet's near-field
  rule, applied to a heightfield. Mounds, sand waves, ridges, cliffs, winding canyons, terraces, volcanic cones
  with craters and lava, brine lakes with their own flat surface.
* **The near field** (`field.*`): kelp, seagrass, corals (branching, brain, table, fan, tube, pillar, glowing),
  anemones, sponges, rocks, pillars, basalt columns, black smokers and lava vents, ruins (columns, arches,
  walls, statues, temple fronts), glyph monoliths, wrecks, ice, crystals, drowned trees, whale falls, clams —
  forgotten behind you and re-dreamt at the far edge ahead. 16 kinds of creature with moods (they wander,
  come to look if you hold still, bolt from a fast pass), and fish schools (reef, sardine bait balls,
  lanternfish).
* **The journey** (`journey.*`): 17 biomes, each 0.7–1.1 km, blending over 80 m (the seabed shelves down or rises).
  - *Depth* chooses: cruise low through a biome and the next is a zone deeper; climb toward the light and it rises.
  - *Heading* chooses the specials (as in MantisPlanet): hold one compass direction through three places.

| Zone | Biomes |
|---|---|
| Sunlit | Coral Gardens · Kelp Cathedral · Seagrass Shallows |
| Twilight | The Blue Drop-off · Light Canyon · Bone Reef |
| Midnight | Lantern Garden · Abyssal Plain · Black Smokers |
| Specials | E The Sunken City · SE Fire Beneath · S Jellyfish Cathedral · SW Iron Graveyard · W Brine Lakes · NW Amethyst Deep · N Under the Ice · NE Leviathan Hall |

### Rendering (`render.cpp`, `props.cpp`)

* 160×120 palette indices, doubled to the panel (MantisPlanet's display pipeline, unchanged). The palette is
  rebuilt every frame: **light at depth is palette work** — red is absorbed first, then green; each biome has its
  own water and reach; dawn and dusk warm the shallows; the deep keeps a faint navy so shapes still read.
* Back to front: the water column → **the surface from below** (Snell's window of sky, ripples, rain dimples,
  aurora, the bent sun; under the ice, a cracked ice ceiling) → **the seabed** as a voxel heightfield with a
  depth buffer, animated caustics, diffuse top-down light, haze bands → everything that grows and swims,
  dissolving into the haze by ordered dither → marine snow and plankton (the sea lights up where you push
  through it on bloom nights) → **light shafts** (MantisPlanet's screen-space scattering, from the sun above;
  over the Sunken City they break into rainbows) → the floodlights' cone → sonar → instruments.

### Sound (`ocean.cpp`, `music.cpp`) and touch (`haptics.cpp`)

Synthesized live at 24 kHz on core 0: the sea's breath, surface wash and rain heard from below, the screw (and
cavitation at flank), bubbles, snapping shrimp, hull creaks past 100 m, humpback song, dolphin whistles and
click trains, Weddell seal trills under the ice, crystal chimes, vent rumble, the leviathan's growl, sonar ping
and echo, the scanner. MantisPlanet's tide conductor scores it: a long steady cruise builds a passage, drifting
still lets it go; the biome and depth pick the mode; the deep keeps it hushed. The motor is the low end: the
screw's pulse, bumps, sonar, pressure creaks, vents, eruptions, whale-song swells, the kick on the beat.

## Host tools (no ESP32 needed)

The real firmware (`main.cpp` included) builds on a desktop compiler against `tools/host/stubs`.

* `tools/host/run.sh` — static RAM budget, then the soak: a bot pilot flies 3 seeds × 10 minutes through the
  real loop. Fails on NaN, ending up inside the seabed or above the surface, getting stuck, or if the depth
  director never takes it to the midnight zone. Prints host ms/frame.
* `tools/host/tilt.cpp` (built and run by run.sh) — climbing and diving from upright, reclined and flat holds.
* `tools/host/stress.cpp` (run.sh) — the camera right up against every object and creature; flags slow frames.
* `tools/host/stems.sh` — every sound soloed (33 stems + INDEX.md + spectrograms) → `snaps/stems/`.
* `tools/host/listen.sh` — a 70 s listening tour through the real sound engine → `snaps/tour.mp3`.
* `tools/host/tests.sh [audio]` — every biome with its creatures, plus the cards → `snaps/sheet.png`
  (and listening previews `snaps/audio_*.wav`). `PERF=1` prints ms/frame per scene.

| File | Role |
|---|---|
| `main.cpp` | loop: helm → move / collide → journey → seabed → scene → field → scanner → motor → render → present |
| `helm.*` | SpaceMantis controls: tilt rates from a neutral pose, slide steering, throttle slider, buttons |
| `terrain.*` | the dreamt seabed grid, the shape function, the ice ceiling |
| `field.*` | objects, creatures, schools, collisions |
| `journey.*` | the 17 biomes and the depth / heading directors |
| `scene.*` | light, weather above, moments (shafts, aurora, bloom, eruption, prism, whale, bait ball) |
| `render.*`, `props.cpp` | the picture |
| `databank.*` | species, scans and deepest depth in flash; full-resolution cards |
| `audio.*`, `ocean.cpp`, `music.cpp`, `dsp.*` | sound |
| `display.*`, `ui.*`, `mem.h` | from MantisPlanet |

Note: this tree is verified on the desktop harness (renders, soak, audio, RAM budget). Flash it and tell me
how the tilt signs, frame rate and motor feel on the hardware; the constants are in `helm.cpp`.
