#include "Maple.h"

Maple::Maple(class SHCpu *cpu, Dword setting) 
{
	this->cpu = cpu;
	controller = new Controller(this, setting);
}

Maple::~Maple() 
{
	delete controller;
}

Dword Maple::hook(int eventType, Dword addr, Dword data) 
{
	//cpu->debugger->print("Maple::access: access to external addr %08X, PC=%08X\n", addr, cpu->PC);
	
	static bool scanlineInterrupt = false;
	static Controller::DeviceInfo *dev;
	
	switch(eventType) 
        {
	case MMU_READ_DWORD:
		switch(addr) 
                {
		case 0xa05f6930: 
			return asic9a; // ASIC_IRQ9_A
		case 0xa05f6900: 
			
		        // XXX: this is VERY ugly. currently undergoing a rewrite 

			// hack: if we're done rendering BUT haven't sent the terminating
			// command for opaque/transparent polys do so now
			if(asicAckA==0x80) 
                        {
#define OPAQUE_POLY 0
#if OPAQUE_POLY
				// hack if you KNOW the demo has only opaque polys (eg kosh and wump)
				scanlineInterrupt = true;
#endif
				return 0x80;
			}
			
			if(asicAckA==0x200) 
                        {
				scanlineInterrupt = true;
				asicAckA |= 0x4;
				return 0x200; 
			}
			
			if(scanlineInterrupt) 
                        {
				scanlineInterrupt = false;
				return 0x8;
			}
			
			return (asicAckA | 0x8); // ASIC_ACK_A
		case 0xa05f688c: // XXX: g2_fifo
			
			// to signal resume g2 dma return 0x20;
			// to signal ready return 0;
			// some programs need 1 back though... so we do 0x21
			
			g2Fifo = g2Fifo ? 0 : 0x21;
			return g2Fifo; 
			
		case 0xa05f6c18: // Maple DMA
			//cpu->debugger->print("Maple: check DMA status, PC=%08X\n", cpu->PC);
			return 0; // hack, pretend DMA is done
			
		default: return 0;
		}
		
	case MMU_WRITE_DWORD:
		switch(addr) 
                {
		case 0xa05f6920: // asic irq b
			
			return 0;
		case 0xa05f6930: 
			asic9a = data; // ASIC_IRQ9_A
			return 0;
		case 0xa05f6900: 
			asicAckA &= ~data; // ASIC_ACK_A
			return 0;
		case 0xa05f6c04: // DMA address set
			// we are given a physical address here, NOT a virtual address
			dmaAddr = data;
			return 0;
		case 0xa05f6c14: // Maple enable
			if(data & 1)
				cpu->debugger->print("Maple: enabled\n");
			else
				cpu->debugger->print("Maple: disabled\n");
			return 0;
		case 0xa05f6c18: // DMA start
			if(data & 1) 
                        { // if bit 0 is set, begin transfer
				
				int cmd, recipient, sender, addtlwords, last, port, length;
				Dword transferDesc1, transferDesc2, frameHeader, dmaPtr, resultAddr;
				
				dmaPtr = dmaAddr;
				// XXX: finish me!
				do {
					transferDesc1 = cpu->mmu->access(dmaPtr, MMU_READ_DWORD);
					transferDesc2 = cpu->mmu->access(dmaPtr+4, MMU_READ_DWORD);
					frameHeader = cpu->mmu->access(dmaPtr+8, MMU_READ_DWORD);
					frameHeader = switchEndian(frameHeader);
					last = bits(transferDesc1, 31, 31);
					port = bits(transferDesc1, 17, 16);
					length = bits(transferDesc1, 7, 0);
					cmd = bits(frameHeader, 31, 24);
					recipient = bits(frameHeader, 23, 16);
					sender = bits(frameHeader, 15, 8);
					addtlwords = bits(frameHeader, 7, 0);
					resultAddr = bits(transferDesc2, 28, 5) << 5;
					
#if 0
					cpu->debugger->print("Maple: DMA transfer from %08X: ", dmaPtr);
					cpu->debugger->print("last %d, port %d, length %d ", last, port, length);
					cpu->debugger->print("Result addr: %08X\n", transferDesc2);
					cpu->debugger->print("Maple: command %08X: ", frameHeader);
					cpu->debugger->print("cmd: %d ", cmd);
					cpu->debugger->print("recipient: %d ", recipient);
					cpu->debugger->print("sender: %d ", sender);
					cpu->debugger->print("addtl words: %d\n", addtlwords);
#endif
					
					switch(cmd) 
					{
					case MAPLE_REQ_DEVICE_INFO:
						
						dev = controller->returnInfo(recipient);
						
						if(dev->suppFuncs==0 || (recipient&0x1f)!=0) 
                                                {
							
							FrameHeader fh;
							fh.command = MAPLE_NORESP_ERR;
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
						} 
						else 
						{
							
							FrameHeader fh;
							fh.command = MAPLE_DEVICE_INFO_RESP;
							fh.recipient = sender;
							fh.sender = recipient;
							fh.numWordsFollowing = 28;
							
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
							for(int i=0; i<28; i++)
								cpu->mmu->writeDwordToExternal(resultAddr+i*4+4, ((Dword*)dev)[i]);
						}
						
						break;
						
					case MAPLE_GET_CONDITION:
						//cpu->debugger->print("Maple: get condition of %02x (on port %d)\n", recipient, bits(recipient, 7, 6));
						//printf("%08x params[0]=%08x\n", dmaAddr, cpu->mmu->access(dmaAddr+4, MMU_READ_DWORD));
						
						dev = controller->returnInfo(recipient);
						
						if(dev->suppFuncs==0) 
                                                {
							
							FrameHeader fh;
							fh.command = MAPLE_NORESP_ERR;
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
						} 
						else 
						{
							
							FrameHeader fh;
							fh.command = MAPLE_DATA_TRANSFER_RESP;
							fh.recipient = sender;
							fh.sender = recipient;
							fh.numWordsFollowing = controller->returnSize(recipient);
							
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
							cpu->mmu->writeDwordToExternal(resultAddr+4, dev->suppFuncs);
							
							Dword *data = controller->returnData(recipient);
							
							for(int i=0; i<(fh.numWordsFollowing-1); i++)
								cpu->mmu->writeDwordToExternal(resultAddr+8+i*4, data[i]);
						}
						break;
					}
					
					dmaPtr += 8 + (length + 1)*4;					
				} while (last != 1);
				asicAckA |= 0x1000;	// Maple DMA Transfer finished
			}
			return 0;
		case 0xa05f6c80: // timeout / speed
			cpu->debugger->print("Maple: timeout set to %d\n", data >> 16);
			return 0;
		case 0xa05f6c8c: // maple reset 1
			if(data != 0x6155404f)
				printf("Maple: weird value for maple reset 1: %08x\n", data);
			return 0;
		case 0xa05f6c10: // maple reset 2
			if(data != 0x00000000) 
				printf("Maple: weird value for maple reset 2: %08x\n", data);
			return 0;
		default:
			printf("Maple: data written to unknown address %08x\n", addr);
			return 0;
		}
	}
	
	cpu->debugger->flamingDeath("Wrong parameters passed to Maple::hook");
	return 0;
}
