#  Copyright (C) 2005-2007 Fons Adriaensen <fons@kokkinizita.net>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.

#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


PREFIX = /usr/local
SUFFIX := $(shell uname -m | sed -e 's/^unknown/$//' -e 's/^i.86/$//' -e 's/^x86_64/$/64/')
LIBDIR = lib$(SUFFIX)
DISTDIR = jconv-$(VERSION)
VERSION = 0.8.1
LDFLAGS += -L$(PREFIX)/$(LIBDIR)
CPPFLAGS += -O3 -Wall -MMD -MP -DVERSION=\"$(VERSION)\"
#CPPFLAGS += -march=i686 -mtune=pentium4 -msse -msse2 -m3dnow -O3 -ffast-math


# Uncomment if you use a libsndfile version without support for setting/getting the Ambisonic flags.
#
#CPPFLAGS += -DNOAMBIS=1


all:	jconv fconv aconv mkwavex


JCONV_O =	jconv.o config.o jconfig.o impdata.o jclient.o
jconv:	LDLIBS += -lzita-convolver -lfftw3f -lsndfile -lclthreads -ljack -lpthread -lrt
jconv:	$(JCONV_O)
	g++ $(LDFLAGS) -o $@ $(JCONV_O) $(LDLIBS)
$(JCONV_O):
-include $(JCONV_O:%.o=%.d)



FCONV_O =	fconv.o config.o fconfig.o impdata.o
fconv:	LDLIBS += -lzita-convolver -lfftw3f -lsndfile -lpthread -lrt
fconv:	$(FCONV_O)
	g++ $(LDFLAGS) -o $@ $(FCONV_O) $(LDLIBS)
$(FCONV_O):
-include $(FCONV_O:%.o=%.d)

ACONV_O =	aconv.o config.o aconfig.o impdata.o
aconv:	LDLIBS += -lzita-convolver -lfftw3f -lsndfile -lpthread -lrt -lasound
aconv:	$(ACONV_O)
	g++ $(LDFLAGS) -o $@ $(ACONV_O) $(LDLIBS)
$(ACONV_O):
-include $(ACONV_O:%.o=%.d)


MKWAVEX_O =	mkwavex.o impdata.o
mkwavex : LDLIBS += -lsndfile -lrt
mkwavex:	$(MKWAVEX_O)
	g++ $(LDFLAGS) -o $@ $(MKWAVEX_O) $(LDLIBS)


install:	jconv fconv mkwavex
	/usr/bin/install -m 755 jconv $(PREFIX)/bin
	/usr/bin/install -m 755 fconv $(PREFIX)/bin
	/usr/bin/install -m 755 mkwavex $(PREFIX)/bin


clean:
	/bin/rm -f *~ *.o *.a *.d *.so jconv fconv mkwavex

