// Swirly main
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <SDL/SDL.h>

#include "SHCpu.h"

SHCpu *cpu;
SDL_Surface *icon;

void onDeath(int signal) {
    cpu->debugger->flamingDeath("Oh no, a segfault.");
}

void sigintHandler(int signal) {
    cpu->debugger->promptOn = true;
}

void exitHandler() {
    delete cpu;
    SDL_FreeSurface(icon);
    SDL_Quit();
}

int main(int argc, char *argv[]) {

    printf(VERSION_STRING " - compiled on " __DATE__ " at " __TIME__ "\n");
    printf("Distributed under the GNU General Public License, version 2\n\n");

    // SIGINT isn't supposed to work on Win32 so we won't even try
#ifndef _WIN32
    signal(SIGINT, sigintHandler); // so we can hit Ctrl+C in *nix to exit
#endif
    signal(SIGSEGV, onDeath);
    
    // SDL init stuff
    SDL_Init(SDL_INIT_VIDEO);
    atexit(exitHandler);
    
    icon = SDL_LoadBMP("../misc/swirly-icon.ico");
    if(icon != NULL)
	SDL_WM_SetIcon(icon, NULL);

    // for now: argc-1 indicates controller, otherwise keyboard/mouse
    cpu = new SHCpu(argc-1); 
    
    // *** EXAMPLES ***

    // to load the boot ROM:
    // cpu->overlord->load("../fakebootrom/fbr.bin", 0xA0000000);

    // to load flash memory (nonessential):
    // cpu->overlord->loadToHost("../images/flash", (Dword) cpu->mmu->flash);

    // to load a flat binary:
    // cpu->overlord->load("../images/dreampac_unscrambled.bin",0x8c010000);

    // to load a SREC file:
    // cpu->overlord->loadSrec("../images/stars.srec");

    // to load an ISO: 
    // cpu->gdrom->startSector(0);
    // cpu->gdrom->load("something.iso");

    printf("Executing autoexec.script...\n");
    if(cpu->debugger->runScript("autoexec.script") == false)
	printf("Couldn't find autoexec.script!\n");
    
    printf("Fasten your seatbelts.\n\n");
    
    //cpu->go_rec();
    cpu->go();
    
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
