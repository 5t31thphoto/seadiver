#!/bin/sh
# Every sound on its own -> snaps/stems/*.wav and *.mp3 (needs ffmpeg for the MP3s).
set -e
cd "$(dirname "$0")"
S=../../src
SRCS="$S/helm.cpp $S/haptics.cpp $S/universe.cpp $S/journey.cpp $S/terrain.cpp $S/field.cpp $S/scene.cpp $S/render.cpp $S/props.cpp $S/display.cpp $S/ui.cpp $S/databank.cpp $S/dsp.cpp $S/audio.cpp $S/ocean.cpp $S/music.cpp"
g++ -std=gnu++11 -O2 -fno-math-errno -DHOST -w -Istubs -I$S -I. stems.cpp $SRCS -o stems
rm -rf snaps/stems; mkdir -p snaps/stems
./stems
cp stems_index.md snaps/stems/INDEX.md
for f in snaps/stems/*.wav; do ffmpeg -loglevel error -y -i "$f" -codec:a libmp3lame -b:a 160k "${f%.wav}.mp3"; done
cp stems_index.md snaps/stems/INDEX.md
rm -f stems
