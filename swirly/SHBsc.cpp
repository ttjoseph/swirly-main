// SHBsc.cpp: implementation of the SHBsc class.
//
// This is the SH4's BSC unit.
//////////////////////////////////////////////////////////////////////

#include "SHBsc.h"
#include "SHMmu.h"

SHBsc::SHBsc(class SHCpu *cpu)
{
	this->cpu = cpu;
	regs[PCTRA] = regs[PCTRB] = 0;
}

SHBsc::~SHBsc()
{

}

Dword SHBsc::hook(int event, Dword addr, Dword data)
{
	addr &= 0xff;
	switch(event)
	{
	case MMU_READ_DWORD:
	case MMU_READ_WORD:
		update();
		return regs[addr];
	case MMU_WRITE_WORD:
		regs[addr] = (Word) data;
		update();
		return 0;
	case MMU_WRITE_DWORD:
		regs[addr] = data;
		update();
		return 0;
	default:
		return 0;
	}
}

void SHBsc::update()
{
	// update PORT8 and PORT9 of PDTRA, which the DC video cable is hooked to
	// 0: VGA
	// 2: RGB
	// 3: composite
	regs[PDTRA] = 0 << 8;
}
