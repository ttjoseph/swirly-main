#include "SHDmac.h"
#include "Debugger.h"
#include "SHMmu.h"
#include "SHIntc.h"
#include "Gpu.h"

#include <string.h>

#define SAR1  0x10
#define DAR1  0x14
#define TCR1  0x18
#define CHCR1 0x1c
#define SAR2  0x20
#define DAR2  0x24
#define TCR2  0x28
#define CHCR2 0x2c
#define SAR3  0x30
#define DAR3  0x34
#define TCR3  0x38
#define CHCR3 0x3c

#define DMAOR 0x40

SHDmac::SHDmac() {}

SHDmac::~SHDmac() {}

void SHDmac::startGdromDma(Dword dest, Dword count, Dword repeat)
{
    debugger->print("DMA transfer of %d bytes from %08x to %08x\n",
    		    count, dmaSrcAddr, dest);

    if(dmaCount != count)
	debugger->flamingDeath("DMA counts on GDROM (%d) and DMAC (%d) do not match\n", count, dmaCount);
    if(repeat != 1)
    debugger->flamingDeath("No support for repeat DMA (%d)\n", repeat);


    // signal that we're done with DMA
    intc->externalInt9a(PVR_DMA);
}

void SHDmac::startDma(Dword dest, Dword count, Dword repeat)
{
    debugger->print("DMA transfer of %d bytes from %08x to %08x\n",
    		    count, dmaSrcAddr, dest);

    if(dmaCount != count)
	debugger->flamingDeath("DMA counts on PVR (%d) and DMAC (%d) do not match\n", count, dmaCount);
    if(repeat != 1)
	debugger->flamingDeath("No support for repeat DMA (%d)\n", repeat);

    memcpy(mmu->videoMem+(dest&0x00ffffff), mmu->mem+(dmaSrcAddr&0x00ffffff), count);

    // signal that we're done with DMA
    intc->externalInt9a(PVR_DMA);
}

Dword SHDmac::hook(int event, Dword addr, Dword data)
{
    addr &= 0xff;

    if(event&MMU_WRITE)
    {
	switch(addr)
	{
	case CHCR2:
	    transSize = (data>>4)&7; 
	    if(transSize > 4) 
		debugger->flamingDeath("DMA::CHCR2 has invalid size\n");
	    break;
	case DAR2: dmaDestAddr = data; break;
	case SAR2: dmaSrcAddr = data; break;
	case TCR2: 
	    switch(transSize)
	    {
	    case 0:
	    case 2: dmaCount = data << 1; break;
	    case 1: dmaCount = data;      break;
	    case 3: dmaCount = data << 2; break;
	    case 4: dmaCount = data << 5; break;
	    }
	    break;

	case DMAOR: dmaor = data; break;
	    
	default:
	    debugger->print("Dmac: unknown address %08x written with data %08x\n", addr, data); 
	    break;
	}
	return 0;
    }

    switch(addr)
    {
    case DMAOR: return 0x8001; // return dmaor;
    case CHCR2: return 0x12c1;
    case TCR2: return 0;
	    
    default: 
	debugger->print("Dmac: unknown address %08x read\n", addr); 
	break;
    }

    return 0;
}
