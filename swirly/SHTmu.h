// SHTmu.h: interface for the SHTmu class.
//
// This is an implementation of the SH4's TMU
//////////////////////////////////////////////////////////////////////

#ifndef _SHTMU_H_
#define _SHTMU_H_

#include "swirly.h"
#include "SHCpu.h"
#include "SHMmu.h"

class SHTmu
{
public:
	void reset();
	Dword hook(int event, Dword addr, Dword data);
	SHTmu(SHCpu *cpu);
	virtual ~SHTmu();

	class SHCpu *cpu;

	void updateTCNT0();
	void updateTCNT1();

private:
	SHTmu() {}
	void update();
	void updateTcnt(Dword *tcnt, int id, Dword starttime);

	Dword TCOR0, TCNT0, TCOR1, TCNT1, TCOR2, TCNT2, TCPR2;
	Word TCR0, TCR1, TCR2;
	Byte TOCR, TSTR;

	Dword tcnt0StartTime, tcnt1StartTime, tcnt2StartTime;
	int currTicks;
};

#endif
