// header for SHIntc, the SH INTC
#ifndef _SHINTC_H_
#define _SHINTC_H_

#include "swirly.h"
#include "SHCpu.h"

class SHIntc
{
public:
	Dword hook(int event, Dword addr, Dword data);
	SHIntc(class SHCpu *cpu);
	virtual ~SHIntc();
	
private:
	SHIntc() {}
	
	class SHCpu *cpu;
};

#endif
