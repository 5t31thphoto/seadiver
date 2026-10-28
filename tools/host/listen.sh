#!/bin/sh
# A ~70 s listening tour through the real firmware's sound -> snaps/tour.wav and snaps/tour.mp3 (needs ffmpeg).
set -e
cd "$(dirname "$0")"
S=../../src
SRCS="$S/helm.cpp $S/haptics.cpp $S/universe.cpp $S/journey.cpp $S/terrain.cpp $S/field.cpp $S/scene.cpp $S/render.cpp $S/props.cpp $S/display.cpp $S/ui.cpp $S/databank.cpp $S/dsp.cpp $S/audio.cpp $S/ocean.cpp $S/music.cpp"
g++ -std=gnu++11 -O2 -fno-math-errno -DHOST -w -Istubs -I$S -I. listen.cpp $SRCS -o listen
mkdir -p snaps
./listen
ffmpeg -loglevel error -y -i snaps/tour.wav -codec:a libmp3lame -b:a 160k snaps/tour.mp3 && echo "snaps/tour.mp3"
rm -f listen
