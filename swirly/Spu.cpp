// Spu.cpp: implementation of the Spu class.
//
//////////////////////////////////////////////////////////////////////

#include "Spu.h"
#include "SHCpu.h"
#include <stdio.h>

Spu::Spu(class SHCpu *cpu)
{
	this->cpu = cpu;
}

Spu::~Spu()
{

}

Dword Spu::hook(int eventType, Dword addr, Dword data)
{
	static bool flip = false;
// printf("SPU access at %08x, PC=%08X\n", addr, cpu->PC);
	flip = !flip;
	if(flip)
		return 0x3;
	else
		return 0xffffffff;
		
}
