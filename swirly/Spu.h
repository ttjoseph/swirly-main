// Spu.h: interface for the Spu class.
//
// This is supposed to be an implementation of the sound processor,
// which is basically an ARM7 core AFAIK.  At the moment there is
// no ARM emulation.
//////////////////////////////////////////////////////////////////////

#ifndef _SPU_H_
#define _SPU_H_

#include "swirly.h"

class Spu  
{
public:
	Dword hook(int eventType, Dword addr, Dword data);
	Spu(class SHCpu *cpu);
	virtual ~Spu();

private:
	Spu() {}

	SHCpu *cpu;
};

#endif
