#include "Modem.h"

Modem::Modem(class SHCpu *cpu) {
    this->cpu = cpu;
}

Dword Modem::hook(int eventType, Dword addr, Dword data) {

    // silently ignore writes
    if(eventType&MMU_READ)
	printf("Modem: read to %08x\n", addr);
    return 0;
}
