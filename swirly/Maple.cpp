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


Dword Maple::asicHook(int eventType, Dword addr, Dword data) 
{
	switch(eventType) 
      	{
	case MMU_READ_DWORD:
		switch(addr) 
                {
		case 0xa05f6930: 
			return asic9a;
		case 0xa05f6900: 
			
			// hack to get the ASIC working. Works OK in most situations.
			// this is just here until we can figure out how to get rid of 0x8...

			if(asicAckA==0x80) return 0x84;
			if(asicAckA==0x200) return 0x204; 
			
			return (asicAckA | 0x8); // hmm....
		case 0xa05f6920:
			// XXX: ... 
			return 0;
		default:
			printf("Unknown read to asic address %08x\n", addr);
			return 0;
		}
	case MMU_WRITE_DWORD:
		switch(addr)
		{
		case 0xa05f6920: 
			// XXX: ... 
			return 0;
		case 0xa05f6930: 
			asic9a = data; 
			return 0;
		case 0xa05f6900: 
			asicAckA &= ~data;
			return 0;
		default: printf("Unknown write to asic address %08x\n", addr);
			return 0;
		}

	default: cpu->debugger->flamingDeath("Invalid access to Maple::asicHook");
	}
	return 0;
}

Dword Maple::g2Hook(int eventType, Dword addr, Dword data) 
{
	addr &= 0xfff;

	if(eventType&MMU_WRITE) {

		if(addr == 0x884) 
                {
			return 0; // XXX: ??? 
		}
		else if(addr == 0x888) 
                {
			return 0; // XXX: ??? 
		}

		printf("Unknown write to g2 bus address %03x\n", addr);
		return 0;
	} else {
		if(addr == 0x88c) {
			// XXX: hack
			g2Fifo = g2Fifo ? 0 : 0x21;
			return g2Fifo; 
		}
		printf("Unknown read to g2 bus address %03x\n", addr);
		return 0;
	}

	return 0;
}

Dword Maple::mapleHook(int eventType, Dword addr, Dword data) 
{
	static Controller::DeviceInfo *dev;

	switch(eventType) 
      	{
	case MMU_READ_DWORD:
		switch(addr&0xff)
		{
		case MAPLE_DMA_ADDR: return 0; // hack: pretend DMA is done
		case MAPLE_STATE_ADDR: return 0; // XXX: what to do here ??? 
		default: printf("Unknown read to maple address %08x\n", addr); 
		}
		break;
			
	case MMU_WRITE_DWORD:
		switch(addr&0xff)
		{
		case MAPLE_DMA_ADDR: 
			dmaAddr = data; 
			return 0;
		case MAPLE_ENABLE_ADDR:
			enabled = data & 1;
			return 0;
		case MAPLE_STATE_ADDR: 
			if(enabled && (data & 1))
                        { 
				
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
		case MAPLE_SPEED_ADDR: 
			return 0; // silently ignore speed
		case MAPLE_RESET1_ADDR: 
			if(data != 0x6155404f)
				printf("Maple: weird value for maple reset 1: %08x\n", data);
			// the dreamcast boot rom gives us 0x61557f00. hmm.... 
			return 0;
		case MAPLE_RESET2_ADDR: 
			if(data != 0x00000000) 
				printf("Maple: weird value for maple reset 2: %08x\n", data);
			return 0;

		default: printf("Unknown write to maple address %08x\n", addr); 
		}
		break;

	default: cpu->debugger->flamingDeath("Invalid access to Maple::mapleHook");
	}
	return 0;
}
