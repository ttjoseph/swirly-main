XGCC=g++
CC=gcc
AS=nasm

SHELL=/bin/sh

ASFLAGS=-f elf
OPTFLAGS=-O3 -mcpu=i686 -march=i686 -g -Wall -fno-exceptions \
	 -felide-constructors -fomit-frame-pointer
GUIFLAGS=-L/usr/X11R6/lib -lSDL -lGL
CPPFLAGS=$(OPTFLAGS)
CFLAGS=

C_FILES=Controller.cpp Debugger.cpp Gdrom.cpp Gpu.cpp Maple.cpp Modem.cpp \
	Overlord.cpp SHBsc.cpp SHCpu.cpp SHDmac.cpp SHIntc.cpp SHMmu.cpp \
	Dreamcast.cpp SHSci.cpp SHTmu.cpp Spu.cpp main.cpp SHUbc.cpp

H_FILES=Controller.h Debugger.h Gdrom.h Gpu.h Maple.h Modem.h Overlord.h \
	SHBsc.h SHCpu.h SHDmac.h SHIntc.h SHMmu.h SHSci.h SHTmu.h Spu.h \
	swirly.h SHUbc.h Dreamcast.h 

O_FILES=Controller.o Debugger.o Gdrom.o Gpu.o Maple.o Modem.o Overlord.o \
	Dreamcast.o SHBsc.o SHCpu.o SHDmac.o SHIntc.o SHMmu.o SHSci.o SHTmu.o \
	Spu.o main.o SHUbc.o 

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

Controller.o: Controller.h
Controller.o: swirly.h
Controller.o: Maple.h
Debugger.o: Overlord.h
Debugger.o: swirly.h
Debugger.o: Debugger.h
Debugger.o: SHMmu.h
Debugger.o: Gpu.h
Debugger.o: SHCpu.h
Debugger.o: dcdis.h
Gdrom.o: Gdrom.h
Gdrom.o: swirly.h
Gdrom.o: Overlord.h
Gdrom.o: Debugger.h
Gdrom.o: SHMmu.h
Gdrom.o: SHCpu.h
Gpu.o: Gpu.h
Gpu.o: swirly.h
Gpu.o: Debugger.h
Gpu.o: SHMmu.h
Gpu.o: Maple.h
Gpu.o: Controller.h
Maple.o: Maple.h
Maple.o: swirly.h
Maple.o: Controller.h
Maple.o: SHMmu.h
Maple.o: Debugger.h
Modem.o: Modem.h
Modem.o: swirly.h
Modem.o: SHMmu.h
Overlord.o: Overlord.h
Overlord.o: swirly.h
Overlord.o: scramble.h
Overlord.o: SHMmu.h
SHBsc.o: SHBsc.h
SHBsc.o: swirly.h
SHBsc.o: SHCpu.h
SHBsc.o: SHMmu.h
SHCpu.o: Gdrom.h
SHCpu.o: swirly.h
SHCpu.o: Maple.h
SHCpu.o: Controller.h
SHCpu.o: SHIntc.h
SHCpu.o: SHTmu.h
SHCpu.o: SHCpu.h
SHCpu.o: SHMmu.h
SHCpu.o: SHDmac.h
SHCpu.o: Debugger.h
SHCpu.o: Overlord.h
SHCpu.o: Gpu.h
SHCpu.o: SHCpuTable.h
SHDmac.o: SHDmac.h
SHDmac.o: swirly.h
SHDmac.o: Debugger.h
SHDmac.o: SHMmu.h
SHIntc.o: SHIntc.h
SHIntc.o: swirly.h
SHIntc.o: SHMmu.h
SHIntc.o: Debugger.h
SHIntc.o: SHCpu.h
SHMmu.o: swirly.h
SHMmu.o: SHMmu.h
SHMmu.o: SHCpu.h
SHMmu.o: Debugger.h
SHMmu.o: SHUbc.h
SHMmu.o: SHTmu.h
SHMmu.o: Maple.h
SHMmu.o: Controller.h
SHMmu.o: Gpu.h
SHMmu.o: Modem.h
SHMmu.o: SHBsc.h
SHMmu.o: SHDmac.h
SHMmu.o: SHSci.h
SHMmu.o: SHIntc.h
SHMmu.o: Spu.h
Dreamcast.o: Dreamcast.h
Dreamcast.o: swirly.h
Dreamcast.o: SHCpu.h
Dreamcast.o: SHMmu.h
Dreamcast.o: SHTmu.h
Dreamcast.o: SHBsc.h
Dreamcast.o: SHSci.h
Dreamcast.o: SHUbc.h
Dreamcast.o: SHDmac.h
Dreamcast.o: SHIntc.h
Dreamcast.o: Gpu.h
Dreamcast.o: Modem.h
Dreamcast.o: Spu.h
Dreamcast.o: Gdrom.h
Dreamcast.o: Maple.h
Dreamcast.o: Controller.h
Dreamcast.o: Overlord.h
Dreamcast.o: Debugger.h
SHSci.o: SHSci.h
SHSci.o: swirly.h
SHSci.o: Debugger.h
SHSci.o: SHMmu.h
SHTmu.o: SHTmu.h
SHTmu.o: swirly.h
SHTmu.o: SHCpu.h
SHTmu.o: SHMmu.h
SHTmu.o: SHIntc.h
Spu.o: Spu.h
Spu.o: swirly.h
Spu.o: SHCpu.h
main.o: Dreamcast.h
main.o: swirly.h
main.o: Debugger.h
SHUbc.o: SHUbc.h
SHUbc.o: swirly.h
Controller.o: swirly.h
Controller.o: Maple.h
Controller.o: Controller.h
Debugger.o: swirly.h
Gdrom.o: swirly.h
Gpu.o: swirly.h
Maple.o: swirly.h
Maple.o: Controller.h
Maple.o: Maple.h
Modem.o: swirly.h
Overlord.o: swirly.h
SHBsc.o: swirly.h
SHBsc.o: SHCpu.h
SHCpu.o: swirly.h
SHDmac.o: swirly.h
SHIntc.o: swirly.h
SHMmu.o: swirly.h
SHSci.o: swirly.h
SHTmu.o: swirly.h
SHTmu.o: SHCpu.h
SHTmu.o: SHMmu.h
Spu.o: swirly.h
SHUbc.o: swirly.h
Dreamcast.o: swirly.h
