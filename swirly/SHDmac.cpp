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
	printf("DMAC access at %08X, PC=%08X\n", addr, cpu->PC);
	return 0xdeadbeef;
}
