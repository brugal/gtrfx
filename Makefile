CC=gcc
CPP=g++
CPPFLAGS=-g -Wall
CFLAGS=-g -Wall -O2 -std=gnu99 `pkg-config --cflags gtk+-2.0` `pkg-config --cflags jack`
LDFLAGS=`pkg-config --libs gtk+-2.0 gthread-2.0` -g -lm -lvorbisenc -lzita-convolver -lsndfile -ldl -L/usr/lib -lvorbis -logg
OBJS=be-soft-clip.o be-gain.o be-linear-gain.o be-hard-clip.o be-clip-detect.o be-mono-to-stereo.o be-wav-file-input.o be-wav-file-output.o be-ogg-file-output.o be-sound-pipe-input.o be-triangle.o be-onlyminmax.o be-hard-gate.o common-string.o main.o tuneit.o be-arctan-distortion.o be-saw.o be-banana-distortion.o be-dcoffset.o be-valve-distortion.o be-falling-clip.o be-conv.o
HEADERS=builtin-effect.h common.h
DIRS=jconv

all: gtrfx-jack gtrfx

gtrfx: $(OBJS) sound-alsa.o
	$(CPP) $(LDFLAGS) -lasound $(OBJS) sound-alsa.o -o gtrfx

gtrfx-jack: $(OBJS) sound-jack.o
	$(CPP) $(LDFLAGS) $(OBJS) `pkg-config --libs jack` sound-jack.o -o gtrfx-jack

JCONV:
	cd jconv; $(MAKE)

$(OBJS): $(HEADERS)

test-plugin: test-plugin.o $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o test-plugin



clean:
	rm -f $(OBJS) gtrfx gtrfx-jack test-plugin.o test-plugin a.out sound-alsa.o sound-jack.o

