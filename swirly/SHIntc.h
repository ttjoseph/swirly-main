// header for SHIntc, the SH INTC
#ifndef _SHINTC_H_
#define _SHINTC_H_

#include "swirly.h"
#include "SHCpu.h"

enum int_source {TMU0, TMU1, TMU2, RTC, WDT, REF, SCI1, GPIO, DMAC, SCIF, UDI};

class SHIntc
{
public:
	void reset();
	Dword hook(int event, Dword addr, Dword data);
	SHIntc(class SHCpu *cpu);
	virtual ~SHIntc();
	
	void internalInt(int_source is, int ivt);
	
private:
	SHIntc() {}
	
	Word ICR, IPRA, IPRB, IPRC;
	
	class SHCpu *cpu;
};

#endif
