#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <SDL/SDL.h>

#include "Dreamcast.h"
#include "Debugger.h"

#define AUTOEXEC "autoexec.script"

Dreamcast *dreamcast;
Debugger *debugger;
SDL_Surface *icon;

void onDeath(int signal) {
    debugger->flamingDeath("Oh no, a segfault.");
}

void onInterrupt(int signal) {
    debugger->promptOn = true;
}

void onExit() {
    delete dreamcast;
    SDL_FreeSurface(icon);
    SDL_Quit();
}

int main(int argc, char *argv[]) {

    printf(VERSION_STRING " - compiled on " __DATE__ " at " __TIME__ "\n");
    printf("Distributed under the GNU General Public License, version 2\n\n");

    // SIGINT isn't supposed to work on Win32 so we won't even try
#ifndef _WIN32
    signal(SIGINT, onInterrupt); 
#endif
    signal(SIGSEGV, onDeath);
    
    SDL_Init(SDL_INIT_VIDEO);
    atexit(onExit);
    
    icon = SDL_LoadBMP("../misc/swirly-icon.ico");
    if(icon != NULL)
	SDL_WM_SetIcon(icon, NULL);

    dreamcast = new Dreamcast(argc-1);
    debugger = new Debugger();

    if(debugger->runScript(AUTOEXEC) == false)
	printf("Possibly fatal error: could not find %s!\n", AUTOEXEC);
    
    dreamcast->run();

    return 0;
}


#if 0 // these are so we can use ElectricFence
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
#endif
