// SHSci.cpp: implementation of the SHSci class.
//
// This is a quick hack.  Ignores a bunch of stuff.
// This is supposed to be both the SCI and SCIF.
//////////////////////////////////////////////////////////////////////

#include "SHSci.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SHSci::SHSci(SHCpu *cpu)
{
	this->cpu = cpu;

}

SHSci::~SHSci()
{

}

Dword SHSci::hook(int event, Dword addr, Dword data)
{
	switch(event)
	{
	case MMU_READ_BYTE:
	case MMU_READ_WORD:
	case MMU_READ_DWORD:
		//printf("SCI(F) read %08x, data=%08x, PC=%08x\n", addr, data, cpu->PC);
		return accessReg(REG_READ, addr, 0);

	case MMU_WRITE_BYTE:
	case MMU_WRITE_WORD:
	case MMU_WRITE_DWORD:
		//printf("SCI(F) write %08x, data=%08x, PC=%08x\n", addr, data, cpu->PC);
		return accessReg(REG_WRITE, addr, data);
		
	default:
		cpu->debugger->flamingDeath("Weird parameter passed to SHSci::hook!");
		return 0; // we never get here
	}
}

Dword SHSci::accessReg(int operation, Dword addr, Dword data)
{
	static Dword scifRegs[0xff];
	Dword *realaddr;

	// where are we looking?
	if((addr & 0x00ff0000) == 0x00e80000)
		realaddr = scifRegs;
	else
		return 0xffffffff; // return a junk value

	Dword regoffs = addr & 0xff;

	switch(operation)
	{
	case REG_READ:
		switch(regoffs)
		{
		case SCFSR2:
			// we're always done sending data.
			return realaddr[regoffs] | 0x20 | 0x40;
		}
		return realaddr[regoffs];
	case REG_WRITE:
		realaddr[regoffs] = data;

		switch(regoffs)
		{
		case SCFTDR2:
			putchar(data & 0xff);
			break;
		case SCSCR2:
		/*	printf("Write to SCSCR2: ");
			if(data & D7)
				printf("TIE ");
			if(data & D6)
				printf("RIE ");
			if(data & D5)
				printf("TE ");
			if(data & D4)
				printf("RE ");
			if(data & D3)
				printf("REIE ");
			if(data & D1)
				printf("CKE1");
			printf("\n");
		*/
			break;			
		}
		return 0;
	default:
		cpu->debugger->flamingDeath("Something bad happened in SHSci::accessReg.");
	}
	return 0;
}

void SHSci::reset()
{
	accessReg(REG_WRITE, 0xffe80000 | SCSMR2, 0x0);
	accessReg(REG_WRITE, 0xffe80000 | SCBRR2, 0xff);
	accessReg(REG_WRITE, 0xffe80000 | SCSCR2, 0x0000);
	accessReg(REG_WRITE, 0xffe80000 | SCFCR2, 0x0);
	accessReg(REG_WRITE, 0xffe80000 | SCFDR2, 0x0);
	accessReg(REG_WRITE, 0xffe80000 | SCSPTR2, 0x0);
	accessReg(REG_WRITE, 0xffe80000 | SCLSR2, 0x0);
	accessReg(REG_WRITE, 0xffe80000 | SCFSR2, 0x0060);
}
