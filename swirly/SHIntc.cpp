// implementation (heh) of the SH4 INTC

#include "SHIntc.h"

SHIntc::SHIntc(class SHCpu *cpu)
{
	this->cpu = cpu;
	reset();
}

SHIntc::~SHIntc()
{
	
}

void SHIntc::externalInt(int irqnr, int ivt)
{
	if (cpu->SR&F_SR_BL) return;

	if ((Dword)(15-irqnr) > ((cpu->SR >> 4) & 0xf)) {
		cpu->SPC = cpu->PC;
		cpu->SSR = cpu->SR;
		CCNREG(INTEVT) = ivt; 
		cpu->setSR(cpu->SR|F_SR_MD|F_SR_BL|F_SR_RB);
		cpu->PC = cpu->VBR + 0x600;
	}
}

void SHIntc::internalInt(int_source is, int ivt)
{
	Dword IPL_irq = 0;
	
	if (cpu->SR&F_SR_BL) return;
	
	switch (is) {
	case TMU0: IPL_irq = (IPRA >> 12) & 0xf; break;
	case TMU1: IPL_irq = (IPRA >> 8) & 0xf; break;
	case TMU2: IPL_irq = (IPRA >> 4) & 0xf; break;
	case RTC:  IPL_irq = (IPRA >> 0) & 0xf; break;
	case WDT:  IPL_irq = (IPRB >> 12) & 0xf; break;
	case REF:  IPL_irq = (IPRB >> 8) & 0xf; break;
	case SCI1: IPL_irq = (IPRB >> 4) & 0xf; break;
	case GPIO: IPL_irq = (IPRC >> 12) & 0xf; break;
	case DMAC: IPL_irq = (IPRC >> 8) & 0xf; break;
	case SCIF: IPL_irq = (IPRC >> 4) & 0xf; break;
	case UDI:  IPL_irq = (IPRC >> 0) & 0xf; break;
	default: cpu->debugger->flamingDeath("Unknown interrupt in SHIntc::internalInt");
	}
	
	if (IPL_irq > ((cpu->SR >> 4) & 0xf)) {
		cpu->SPC = cpu->PC;
		cpu->SSR = cpu->SR;
		CCNREG(INTEVT) = ivt; 
		cpu->setSR(cpu->SR|F_SR_MD|F_SR_BL|F_SR_RB);
		cpu->PC = cpu->VBR + 0x600;
	}
}

Dword SHIntc::hook(int event, Dword addr, Dword data)
{
//	cpu->debugger->print("Intc: access at %08X, PC=%08X\n", addr, cpu->PC);

	Dword *realaddr = 0;

	switch(addr & 0xff)
	{
	case 0x00: realaddr = (Dword*)&ICR; break;

	case 0x04: realaddr = (Dword*)&IPRA; break;
	case 0x08: realaddr = (Dword*)&IPRB; break;
	case 0x0c: realaddr = (Dword*)&IPRC; break;
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

	return 0;
}

void SHIntc::reset()
{
	ICR = 0;
	IPRA = IPRB = IPRC = 0;
}
