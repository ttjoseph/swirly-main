// header for SHDmac, implementation of SH4 DMAC

#ifndef _SHDMAC_H_
#define _SHDMAC_H_

#include "swirly.h"
#include "SHCpu.h"

class SHDmac
{
public:
	Dword hook(int event, Dword addr, Dword data);
	SHDmac(class SHCpu *cpu);
	virtual ~SHDmac();
	
private:
	SHDmac() {}
	class SHCpu *cpu;
};

#endif
