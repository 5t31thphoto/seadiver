#!/bin/sh
# Static RAM budget, then the soak (a bot pilot flies the real firmware loop). No ESP32 needed.
"$(dirname "$0")/ramcheck.sh" || exit 1
set -e
cd "$(dirname "$0")"
S=../../src
SRCS="$S/helm.cpp $S/haptics.cpp $S/universe.cpp $S/journey.cpp $S/terrain.cpp $S/field.cpp $S/scene.cpp $S/render.cpp $S/props.cpp $S/display.cpp $S/ui.cpp $S/databank.cpp $S/dsp.cpp $S/audio.cpp $S/ocean.cpp $S/music.cpp"
g++ -std=gnu++11 -O2 -fno-math-errno -DHOST -Wall -Wno-unused-function -Istubs -I$S -I. soak.cpp $SRCS -o soak 2>&1 | grep -v Serial | grep -E "error|warning" || true
for t in tilt stress; do g++ -std=gnu++11 -O2 -fno-math-errno -DHOST -w -Istubs -I$S -I. $t.cpp $SRCS -o $t; done
./tilt
./stress | grep -E "baseline|worst|SLOW"
./soak ${1:-10}
rm -f soak tilt stress
