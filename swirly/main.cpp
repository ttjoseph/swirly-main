// Swirly main
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "SHCpu.h"
#include "Overlord.h"

#include <SDL/SDL.h>

SHCpu *cpu;

void onDeath(int signal)
{
	cpu->debugger->flamingDeath("Oh no, a segfault.");
}

void sigintHandler(int signal)
{
	cpu->debugger->promptOn = true;
}

int main(int argc, char *argv[])
{
	printf(VERSION_STRING " - compiled on " __DATE__ " at " __TIME__ "\n");
	printf("Distributed under the GNU General Public License, version 2\n\n");

	// SIGINT isn't supposed to work on Win32 so we won't even try
#ifndef _WIN32
	signal(SIGINT, sigintHandler); // so we can hit Ctrl+C in *nix to exit
#endif
	signal(SIGSEGV, onDeath);
	
	// SDL init stuff
	SDL_Init(SDL_INIT_VIDEO);
	atexit(SDL_Quit);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

	SDL_Surface *icon = SDL_LoadBMP("../misc/swirly-icon.ico");
	if(icon != NULL)
		SDL_WM_SetIcon(icon, NULL);

	cpu = new SHCpu();
	Overlord *o = new Overlord(cpu);

#if 0
	// load the boot ROM
	printf("Loading boot ROM...");
	o->load("../fakebootrom/fbr.bin", 0xA0000000);
	printf("done.\n");

	// load the flash memory
	//printf("Loading flash (nonessential image)...");
	//o->loadToHost("../images/flash", (Dword) cpu->mmu->flash);
	//printf("done.\n");

	// load the images we're going to run
	printf("Loading images...");
	o->load("../images/IP.BIN", 0x8c008000);
	//o->load("../images/dreampac_unscrambled.bin", 0x8c010000);
	//o->loadSrec("../images/stars.srec");
	//o->loadSrec("../images/mapletest.srec");
	//o->loadSrec("../images/videotest.srec");
	//cpu->gdrom->startSector(0);
	//cpu->gdrom->load("something.iso");
	printf("done.\n");
#endif
	printf("Executing autoexec.script...\n");
	if(cpu->debugger->runScript("autoexec.script") == false)
		printf("Couldn't find autoexec.script!\n");

	cpu->gdrom->load("../images/linux.iso");
	printf("Fasten your seatbelts.\n\n");
	// PC should be 0xA0000000 at this point
	cpu->go();

	SDL_FreeSurface(icon); // as if we ever reach here anyway

	return 0;
}

/*
// these are so we can use ElectricFence
void* operator new(size_t sz)
{
	printf("new %d!", (int)sz);
	return malloc(sz);
}


void operator delete(void* p)
{
	printf("delete!");
	free(p);
}
*/
