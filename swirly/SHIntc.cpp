// implementation of the SH4 INTC

#include "SHIntc.h"

SHIntc::SHIntc(class SHCpu *cpu)
{
	this->cpu = cpu;	
}

SHIntc::~SHIntc()
{
	
}

Dword SHIntc::hook(int event, Dword addr, Dword data)
{
	printf("INTC access at %08X, PC=%08X\n", addr, cpu->PC);
	return 0xdeadbeef;
}
