#ifndef _SHTMU_H_
#define _SHTMU_H_

#include "swirly.h"

class SHTmu
{
public:
	void reset();
	Dword hook(int event, Dword addr, Dword data);
	SHTmu();
	virtual ~SHTmu();

	void updateTCNT0();
	void updateTCNT1();
	void updateTCNT2();

private:
	void update();
	void updateTcnt(Dword *tcnt, int id, Dword starttime);

	Dword TCOR0, TCNT0, TCOR1, TCNT1, TCOR2, TCNT2, TCPR2;
	Word TCR0, TCR1, TCR2;
	Byte TOCR, TSTR;

	Dword tcnt0StartTime, tcnt1StartTime, tcnt2StartTime;
	int currTicks;
};

#endif
