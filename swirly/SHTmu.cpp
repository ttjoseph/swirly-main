// SHTmu.cpp: implementation of the SH4 TMU.
//
// This is by no means complete, or even correct!
//////////////////////////////////////////////////////////////////////

#include "SHTmu.h"
#include <string.h>

SHTmu::SHTmu(SHCpu *cpu)
{
	this->cpu = cpu;
	reset();
}

SHTmu::~SHTmu()
{

}

// this function updates the TMU registers according to what
// logical time it is.  it only needs to be called whenever the
// registers are accessed, which hook() does automatically.
void SHTmu::update()
{
	currTicks = SDL_GetTicks();
#if 0
#define UPDATETIMER(a, b, c, d) if(TSTR & c) a = d - (delta * (ticks - b))
	UPDATETIMER(TCNT0, tcnt0StartTime, D0, TCOR0);
	UPDATETIMER(TCNT1, tcnt1StartTime, D1, TCOR1);
	UPDATETIMER(TCNT2, tcnt2StartTime, D2, TCOR2);
#undef UPDATETIMER
#endif

	updateTcnt(&TCNT0, 0, tcnt0StartTime);
	updateTcnt(&TCNT1, 1, tcnt1StartTime);
	updateTcnt(&TCNT2, 2, tcnt2StartTime);
}

// XXX: boy this looks retarded
void SHTmu::updateTcnt(Dword *tcnt, int id, Dword starttime)
{
	static const int Delta = 500000; // arbitrary value
	Dword tstrbit = 1 << id, *tcor;
	Word *tcr;
	switch(id)
	{
	case 0: tcor = &TCOR0; tcr = &TCR0; break;
	case 1: tcor = &TCOR1; tcr = &TCR1; break;
	case 2: tcor = &TCOR2; tcr = &TCR2; break;
	default: cpu->debugger->flamingDeath("SHTmu::updateTcnt: Tried to update TCNT%d, which doesn't exist", id);
	}
	if(TSTR & tstrbit)
	{
		Dword oldtcnt = *tcnt;
		*tcnt = *tcor - (Delta * (currTicks - starttime));
		if(*tcnt > oldtcnt) // check for underflow
		{
			*tcr |= 0x100; // turn on TCRx.UNF
			*tcnt = *tcor;
		}
//		cpu->debugger->print("SHTmu::updateTcnt: Set tcnt to %08x\n", *tcnt);
	}
}

// call this on memory accesses
Dword SHTmu::hook(int event, Dword addr, Dword data)
{
//	printf("TMU access at %08x\n", addr);

	Dword *realaddr = 0;
	Byte oldTstr = TSTR;

	update();

	switch(addr & 0xff)
	{
	case 0x00: realaddr = (Dword*)&TOCR; break;
	case 0x04: realaddr = (Dword*)&TSTR; break;

	case 0x08: realaddr = (Dword*)&TCOR0; break;
	case 0x0c: realaddr = (Dword*)&TCNT0; break;
	case 0x10: realaddr = (Dword*)&TCR0; break;

	case 0x14: realaddr = (Dword*)&TCOR1; break;
	case 0x18: realaddr = (Dword*)&TCNT1; break;
	case 0x1c: realaddr = (Dword*)&TCR1; break;

	case 0x20: realaddr = (Dword*)&TCOR2; break;
	case 0x24: realaddr = (Dword*)&TCNT2; break;
	case 0x28: realaddr = (Dword*)&TCR2; break;

	case 0x2c: realaddr = (Dword*)&TCPR2; break;
	}

	switch(event)
	{
	case MMU_READ_BYTE: return *((Byte*)realaddr);
	case MMU_READ_WORD: return *((Word*)realaddr);
	case MMU_READ_DWORD: return *realaddr;

	case MMU_WRITE_BYTE: *((Byte*)realaddr) = (Byte)data; break;
	case MMU_WRITE_WORD: *((Word*)realaddr) = (Word)data; break;
	case MMU_WRITE_DWORD: *(realaddr) = data; break;
	}

#define DOTIMERTHING(a, b) if((TSTR & a) && !(oldTstr & a)) b = SDL_GetTicks()
	DOTIMERTHING(D0, tcnt0StartTime);
	DOTIMERTHING(D1, tcnt1StartTime);
	DOTIMERTHING(D2, tcnt2StartTime);
#undef DOTIMERTHING

	return 0;
}

void SHTmu::reset()
{
	TOCR = TSTR = 0;
	TCR0 = TCR1 = TCR2 = 0;
	TCOR1 = TCNT1 = TCOR2 = TCNT2 = TCOR0 = TCNT0 = 0xffffffff;
	tcnt0StartTime = tcnt1StartTime = tcnt2StartTime = 0;
}
