#include "SHIntc.h"
#include "SHMmu.h"
#include "Debugger.h"
#include "SHCpu.h"
#include "Maple.h"
#include "Gpu.h"
#include "SHTmu.h"

#include <list>

using namespace std;

typedef pair<Dword, Dword> info;
typedef list<info> intr_list; 
intr_list interrupts;

// time in terms of CPU opcode counts: this is a wild guess
#define FRAME    0x8000

SHIntc::SHIntc(): exceptionsPending(0)
{
    reset();
    addInterrupt(FRAME, 0);    // first SDL event
    addInterrupt(0x1000, 1);   // first VBL event
}

void SHIntc::printInterrupts()
{
    intr_list::iterator i = interrupts.begin();
    for(i = interrupts.begin(); i != interrupts.end(); i++)
    {
	switch (i->second) 
        {
	case 0: printf("SDL event in 0x%x iterations\n", i->first); break;
	case 1: printf("VBL event in 0x%x iterations\n", i->first); break;
	case 2: printf("TMU TCNT0 in 0x%x iterations\n", i->first); break;
	case 3: printf("TMU TCNT1 in 0x%x iterations\n", i->first); break;
	}
    }
}

void SHIntc::checkInterrupt()
{
    intr_list::iterator i = interrupts.begin();
    while (i->first <= cpu->numIterations) 
    {
	switch (i->second) 
        {
	case 0: 
	    gpu->draw2DFrame();
	    maple->controller->checkInput();
	    //externalInt9a(MAPLE_DMA); 

	    // XXX: fix the below line when we do correct timings
	    maple->setAsicA(SCANINT1);
	    addInterrupt(FRAME, 0); // next SDL event
	    break;
	case 1:
	    externalInt9a(VBL);
	    addInterrupt(FRAME, 1); // next VBL event (60 fps)
	    break;
	case 2:
	    tmu->updateTCNT0();
	    break;
	case 3:
	    tmu->updateTCNT1();
	    break;
	default:
	    printf("Unknown Interrupt Type: %d\n", i->second);
	}
	interrupts.erase(i);
	i = interrupts.begin();	
    }
}

void SHIntc::addInterrupt(Dword at, Dword type)
{
    interrupts.insert(interrupts.end(), info(cpu->numIterations + at, type));
}

// XXX: externalIntb and externalIntc could be folded into this function...
// (see KOS asic.c for an example)
void SHIntc::externalInt9a(int newAsic) 
{
    maple->setAsicA(newAsic);

    if(cpu->SR & F_SR_BL) return;

    if((maple->asicAckA & maple->asic9a) ||
       (maple->asicAckA & maple->asicBa) ||
       (maple->asicAckA & maple->asicDa))
    {
	int imask = (cpu->SR >> 4) & 0x0f;

	if(imask < 6) // interrupt 9 only
	{
	    cpu->SPC = cpu->PC;
	    cpu->SSR = cpu->SR;
	    cpu->setSR(cpu->SR|F_SR_MD|F_SR_BL|F_SR_RB);

	    cpu->PC = cpu->VBR + 0x600; // offset
	    CCNREG(INTEVT) = 0x320;     // interrupt code (irq 9)
	} 
    }
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
	default: debugger->flamingDeath("Unknown interrupt in SHIntc::internalInt");
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

struct exceptions 
{
    int handle;
    char *id;
};

struct exceptions exceptionsList[] = 
{
    {E_USER_BREAK_BEFORE_INSTRUCTION_EXECUTION, "E_USER_BREAK_BEFORE_INSTR_EXECUTION"},
    {E_INSTRUCTION_ADDRESS_ERROR, "E_INSTRUCTION_ADDRESS_ERROR"},
    {E_INSTRUCTION_TLB_MISS, "E_INSTRUCTION_TLB_MISS"},
    {E_INSTRUCTION_TLB_PROTECTION_VIOLATION, "E_INSTRUCTION_TLB_PROTECTION_VIOLATION"},
    {E_GENERAL_ILLEGAL_INSTRUCTION, "E_GENERAL_ILLEGAL_INSTRUCTION"},
    {E_SLOT_ILLEGAL_INSTRUCTION, "E_SLOT_ILLEGAL_INSTRUCTION"},
    {E_GENERAL_FPU_DISABLE, "E_GENERAL_FPU_DISABLE"},
    {E_SLOT_FPU_DISABLE, "E_SLOT_FPU_DISABLE"},
    {E_DATA_ADDRESS_ERROR_READ, "E_DATA_ADDRESS_ERROR_READ"},
    {E_DATA_ADDRESS_ERROR_WRITE, "E_DATA_ADDRESS_ERROR_WRITE"},
    {E_DATA_TLB_MISS_READ, "E_DATA_TLB_MISS_READ"},
    {E_DATA_TLB_MISS_WRITE, "E_DATA_TLB_MISS_WRITE"},
    {E_DATA_TLB_PROTECTION_VIOLATION_READ, "E_DATA_TLB_PROTECTION_VIOLATION_READ"},
    {E_DATA_TLB_PROTECTION_VIOLATION_WRITE, "E_DATA_TLB_PROTECTION_VIOLATION_WRITE"},
    {E_TRAP, "E_TRAP"},
    {E_FPU, "E_FPU"},
    {E_INITAL_PAGE_WRITE, "E_INITAL_PAGE_WRITE"},
    {-1, "[-1 is not an exception]"}
};

void SHIntc::exception(Dword type, Dword addr, Dword data, char *datadesc) 
{
    exceptionsPending++;

    printf("Obtained a general exception %s\n", getExceptionName(type));

    if(type==E_GENERAL_ILLEGAL_INSTRUCTION) 
	debugger->flamingDeath("Illegal instruction");

    Dword offset = 0x100;

    if((type == E_INSTRUCTION_TLB_MISS) || (type == E_DATA_TLB_MISS_READ) ||
       (type == E_DATA_TLB_MISS_WRITE)) 
    {
	offset = 0x400;
    } 
	
    cpu->SPC = cpu->PC;
    cpu->SSR = cpu->SR;

    CCNREG(EXPEVT) = type;

    cpu->setSR(cpu->SR | F_SR_BL | F_SR_MD | F_SR_RB);

    if(cpu->PC == (cpu->VBR + offset)) 
	goto done; // forget it: we're at the same place!

    cpu->PC = cpu->VBR + offset;

    return;

 done:
    if(datadesc == 0) 
    {
	debugger->flamingDeath("Exception of type 0x%x (%s) at %08X",
			       type, getExceptionName(type),
			       addr, data);
    } 
    else 
    {
	debugger->flamingDeath("Exception of type 0x%x (%s) at %08X\n%s: %08x",
			       type, getExceptionName(type),
			       addr, datadesc, data);
    }
}

char* SHIntc::getExceptionName(int exception)
{
	struct exceptions *ptr = exceptionsList;
	while(ptr->handle != -1)
	{
		if(ptr->handle == exception)
			return ptr->id;
		ptr++;
	}

	debugger->flamingDeath("Unknown exception %0x caught\n", exception);
	return "[Unknown exception]";
}
