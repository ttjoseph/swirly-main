#include "Maple.h"
#include "Overlord.h"
#include <stdio.h>

Maple::Maple(class SHCpu *cpu)
{
	this->cpu = cpu;
}

Maple::~Maple()
{
		
}

Dword Maple::hook(int eventType, Dword addr, Dword data)
{
	// printf("Maple access at %08X, PC=%08X\n", addr, cpu->PC);
	switch(eventType)
	{
	case MMU_READ_DWORD:
		switch(addr)
		{
			// XXX: is this Right?
		case 0xa05f6900: // TA's VBL reg
			return 0x88; // hack

		case 0xa05f688c: // XXX: some SPU-related reg?
			return 1; // hack
		
		case 0xa05f6c18: // Maple DMA
			printf("Maple: check DMA status, PC=%08X\n", cpu->PC);
			return 0; // hack, pretend DMA is done
		
		default:
			return 0;
		}
	case MMU_WRITE_DWORD:
		switch(addr)
		{
			case 0xa05f6c04: // DMA address set
				// we are given a physical address here, NOT a virtual address
				dmaAddr = data;
				return 0;

			case 0xa05f6c18: // DMA start
				if(data & 1) // if bit 0 is set, begin transfer
				{
					printf("Maple: DMA transfer from %08X: ", dmaAddr);
					// XXX: finish me!
					// now we need to get the byte there
					Dword transferDesc1 = cpu->mmu->access(dmaAddr, MMU_READ_DWORD);
					Dword transferDesc2 = cpu->mmu->access(dmaAddr+4, MMU_READ_DWORD);
					Dword frameHeader = cpu->mmu->access(dmaAddr+8, MMU_READ_DWORD);
					frameHeader = Overlord::switchEndian(frameHeader);
					printf("LAST %d, PORT %d, LENGTH %d ",
							Overlord::bits(transferDesc1, 31, 31),
							Overlord::bits(transferDesc1, 17, 16),
							Overlord::bits(transferDesc1, 7, 0));
							
					printf("Result addr: %08X\n", transferDesc2);
					
					printf("Maple: command %08X: ", frameHeader);
					printf("cmd: %d ", Overlord::bits(frameHeader, 31, 24));
					printf("recipient: %d ", Overlord::bits(frameHeader, 23, 16));
					printf("sender: %d ", Overlord::bits(frameHeader, 15, 8));
					printf("addtl words: %d\n", Overlord::bits(frameHeader, 7, 0));
				}			
		}

	}
	return 0xbeefbeef;

}
