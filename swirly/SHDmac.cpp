#include "SHDmac.h"

SHDmac::SHDmac(class SHCpu *cpu)
{
	this->cpu = cpu;	
}

SHDmac::~SHDmac()
{
	
}

Dword SHDmac::hook(int event, Dword addr, Dword data)
{
	// nothing is implemented, except a complaint about if DMAC addresses are read... 
	if(event&MMU_READ)
		cpu->debugger->print("Dmac: address %08x read\n", addr);
	return 0;
}
