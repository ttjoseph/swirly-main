#include "Modem.h"
#include "SHMmu.h"

#include <stdio.h>

Modem::Modem() {}

Dword modemRegs[0x1000];

Dword Modem::hook(int eventType, Dword addr, Dword data) {
    if(eventType&MMU_READ) 
	printf("Modem: read to %08x\n", addr);

    if(addr == 0xa05f7818) {
	return 0x80;
    }

    if(addr == 0xa05f7018) {
	return 0x80;
    }
    
    if(addr == 0xa05f7418) {
	return 0x80;
    }

    if(addr == 0xa05f709c) {
	return 0x80;
    }

    if(eventType&MMU_WRITE) {
	modemRegs[addr&0xfff] = data;
    }

    if(eventType&MMU_READ) {
	//printf("Modem: read to %08x\n", addr);
	return modemRegs[addr&0xfff];
    }

    return 0;
}
