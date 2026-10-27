#!/bin/sh
# Look check: every biome and its creatures -> snaps/sheet.png   (./tests.sh audio: also snaps/audio_*.wav)
set -e
cd "$(dirname "$0")"
S=../../src
SRCS="$S/helm.cpp $S/haptics.cpp $S/universe.cpp $S/journey.cpp $S/terrain.cpp $S/field.cpp $S/scene.cpp $S/render.cpp $S/props.cpp $S/display.cpp $S/ui.cpp $S/databank.cpp $S/dsp.cpp $S/audio.cpp $S/ocean.cpp $S/music.cpp"
g++ -std=gnu++11 -O2 -fno-math-errno -DHOST -Wall -Wno-unused-function -Istubs -I$S -I. snap.cpp $SRCS -o snap 2>&1 | grep -v "Serial" | grep -E "error|warning" || true
rm -rf snaps; mkdir -p snaps
./snap $1
python3 render.py
rm -f snap
