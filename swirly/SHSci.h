// SHSci.h: interface for the SHSci class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _SHSCI_H_
#define _SHSCI_H_

#include "swirly.h"
#include "SHMmu.h"
#include "SHCpu.h"

// offsets from 0xffe80000
#define SCSMR2 0
#define SCBRR2 4
#define SCSCR2 8
#define SCFTDR2 0xc
#define SCFSR2 0x10
#define SCFRDR2 0x14
#define SCFCR2 0x18
#define SCFDR2 0x1c
#define SCSPTR2 0x20
#define SCLSR2 0x24

class SHSci  
{
public:
	void reset();
	Dword hook(int event, Dword addr, Dword data);
	SHSci(SHCpu *cpu);
	virtual ~SHSci();

private:
	SHSci() {}

	Dword accessReg(int operation, Dword addr, Dword data);
	class SHCpu *cpu;
};

#endif
