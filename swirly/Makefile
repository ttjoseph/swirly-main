XGCC=g++
CC=gcc
AS=nasm

SHELL=/bin/sh

ASFLAGS=-f elf
OPTFLAGS=-O3 -mcpu=i686 -march=i686 -g -Wall -fno-rtti -fno-exceptions \
	 -felide-constructors -fomit-frame-pointer
GUIFLAGS=-L/usr/X11R6/include -lGL -lSDL -lpthread
CPPFLAGS=$(OPTFLAGS)
CFLAGS=

C_FILES=Controller.cpp Debugger.cpp Gdrom.cpp Gpu.cpp Maple.cpp Modem.cpp \
	Overlord.cpp SHBsc.cpp SHCpu.cpp SHDmac.cpp SHIntc.cpp SHMmu.cpp \
	SHSci.cpp SHTmu.cpp Spu.cpp main.cpp

H_FILES=Controller.h Debugger.h Gdrom.h Gpu.h Maple.h Modem.h Overlord.h \
	SHBsc.h SHCpu.h SHDmac.h SHIntc.h SHMmu.h SHSci.h SHTmu.h Spu.h \
	swirly.h

O_FILES=Controller.o Debugger.o Gdrom.o Gpu.o Maple.o Modem.o Overlord.o \
	SHBsc.o SHCpu.o SHDmac.o SHIntc.o SHMmu.o SHSci.o SHTmu.o Spu.o \
	main.o

C_O_FILES=dcdis.o decode.o scramble.o

all: cfiles swirly

cfiles: $(C_O_FILES)

swirly: $(O_FILES)
	$(XGCC) -o swirly $(O_FILES) $(C_O_FILES) $(GUIFLAGS)

depend:
	makedepend -w10 -- $(CPPFLAGS) -- $(C_FILES) $(H_FILES)
	./fixdepend.pl Makefile

clean:
	-rm -f $(O_FILES) $(C_O_FILES)
	-rm -f a.out core swirly

distclean: clean

# DO NOT DELETE THIS LINE -- make depend depends on it.
