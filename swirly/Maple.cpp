#include "Maple.h"
#include "SHMmu.h"
#include "Debugger.h"
#include "SHIntc.h"
#include "SHDmac.h"

union {
    struct {
	Dword extAddr;
	Dword size; // may double as sh4addr for SPU DMA
	Dword count;
	Dword dir;
	Dword mode;
	Dword ctrl1;
	Dword ctrl2;
	Dword unknown;
    } banks[4];
    
    Dword data[32];
} g2data;

Maple::Maple(Dword setting) 
{
    controller = new Controller(this, setting);

    asic9a = asic9b = asic9c = 0x00000000;

    // g2 data structure initialization
    for(int bank=0; bank<4; bank++)
    {
	g2data.banks[bank].ctrl1 = 0;
	g2data.banks[bank].ctrl2 = 0;
	g2data.banks[bank].mode = 0;
    }
}

Maple::~Maple() 
{
    delete controller;
}

void Maple::setAsicA(int bit)
{
    asicAckA |= (1<<bit);
}

Dword Maple::asicHook(int eventType, Dword addr, Dword data) 
{
    switch(eventType) 
    {
    case MMU_READ_DWORD:
	switch(addr & 0xff) 
        {
	case 0x10: return asic9a;
	case 0x20: return asicBa;
	case 0x30: return asicDa;

	case 0x38: return asicDc; break; // triggered by linux kernel

	case 0x00: return asicAckA; 
	case 0x04: return asicAckB; 
	case 0x08: return asicAckC; 

	default:
	    printf("Unknown read to asic address %08x\n", addr);
	    return 0;
	}
    case MMU_WRITE_DWORD:
	switch(addr & 0xff)
	{
	// asic irq 9
	case 0x10: asic9a = data; break;
	case 0x14: asic9b = data; break;
	case 0x18: asic9c = data; break;

	// asic irq B
	case 0x20: asicBa = data; break;
	case 0x24: asicBb = data; break;
	case 0x28: asicBc = data; break;

	// asic irq D
	case 0x30: asicDa = data; break;
	case 0x34: asicDb = data; break;
	case 0x38: asicDc = data; break;

        // asic acks
	case 0x00: asicAckA &= ~data; break;
	case 0x04: asicAckB &= ~data; break;
	case 0x08: asicAckC &= ~data; break;

	default: 
	    printf("Unknown write to asic address %08x (%08x)\n", addr, data); 
	    break;
	}
        break; 

    default: debugger->flamingDeath("Invalid access to Maple::asicHook");
    }
    return 0;
}

Dword Maple::unknownHook(int eventType, Dword addr, Dword data) 
{
    printf("unknown Hook invoked: %08x\n", addr);

    addr &= 0xfff; 

    //if(addr==0x018) debugger->promptOn=true;

    Dword tempData;

    static bool test = false;
    if(test) {
	tempData = 0x08;
	test = false;
    } else {
	tempData = 0x00;
	test = true;
    }
    
    return tempData;
}

Dword Maple::g2Hook(int eventType, Dword addr, Dword data) 
{
    addr &= 0xff;
    
    // g2 dma control structures
    if(addr < 0x80)
    {
	if(eventType&MMU_READ) return g2data.data[addr>>2];

	int bank = addr >> 5;

	switch((addr>>2)&0x7)
	{
	case 0: g2data.banks[bank].extAddr = data; break; 
	case 1: g2data.banks[bank].size = data;    break; 
	case 2: g2data.banks[bank].count = data;   

	    printf("PVR DMA possible initialization (count %d)\n", data);
	    if(g2data.banks[bank].mode == 0) // not SPU DMA
		{
		    if((g2data.banks[bank].count==1) &&
		       (g2data.banks[bank].size) &&
		       (g2data.banks[bank].extAddr))
			{
			    dmac->startDma(g2data.banks[bank].extAddr, 
					   g2data.banks[bank].size, 
					   g2data.banks[bank].count);
			    g2data.banks[bank].count = 0;
			}
		}

	    break;
	case 3: g2data.banks[bank].dir = data;     break;
	case 4: g2data.banks[bank].mode = data;    break;
	case 5: g2data.banks[bank].ctrl1 = data;   break;
	case 6: g2data.banks[bank].ctrl2 = data;   break;
	case 7: g2data.banks[bank].unknown = data; break;
	}

	// XXX: check for DMA activity -- how to activate DMA, etc
	// XXX: this is a hack.


	return 0;
    }

    if(eventType&MMU_READ && addr==0x8c)
    {
	// XXX: hack... hopefully we can get rid of this eventually....
	g2Fifo = g2Fifo ? 0 : 0x21;
	return g2Fifo; 
    }

    // log accesses from 0x80 to 0xbc since we don't know what they do... 
    // 0x84: pvr lmmode0 ???
    // 0x88: pvr lmmode1 ???
    // 0x8c: g2 fifo
    // 0x90: wait state (usually 0x1b)
    // 0x9c: ??? some programs read this...
    // 0xbc: magic value (0x46597f00, 0x4659404f)

    if(eventType&MMU_READ)
	printf("Unknown read to g2 bus address %.2x\n", addr);
    if(eventType&MMU_WRITE)
	printf("Unknown write to g2 bus address %.2x (%08x)\n", addr, data);

    // XXX: g2 dma status structures

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
		    transferDesc1 = mmu->access(dmaPtr, MMU_READ_DWORD);
		    transferDesc2 = mmu->access(dmaPtr+4, MMU_READ_DWORD);
		    frameHeader = mmu->access(dmaPtr+8, MMU_READ_DWORD);
		    frameHeader = switchEndian(frameHeader);
		    last =       bits(transferDesc1, 31, 31);
		    port =       bits(transferDesc1, 17, 16);
		    length =     bits(transferDesc1, 7, 0);
		    cmd =        bits(frameHeader, 31, 24);
		    recipient =  bits(frameHeader, 23, 16);
		    sender =     bits(frameHeader, 15, 8);
		    addtlwords = bits(frameHeader, 7, 0);
		    resultAddr = bits(transferDesc2, 28, 5) << 5;
		    
#if 0
		    debugger->print("Maple: DMA transfer from %08X: ", dmaPtr);
		    debugger->print("last %d, port %d, length %d ", last, port, length);
		    debugger->print("Result addr: %08X\n", transferDesc2);
		    debugger->print("Maple: command %08X: ", frameHeader);
		    debugger->print("cmd: %d ", cmd);
		    debugger->print("recipient: %d ", recipient);
		    debugger->print("sender: %d ", sender);
		    debugger->print("addtl words: %d\n", addtlwords);
#endif
					
		    switch(cmd) 
		    {
		    case MAPLE_REQ_DEVICE_INFO:
			
			dev = controller->returnInfo(recipient);
			
			if(dev->suppFuncs==0 || (recipient&0x1f)!=0) 
			{
			    
			    FrameHeader fh;
			    fh.command = MAPLE_NORESP_ERR;
			    mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
			} 
			else 
			{
			    
			    FrameHeader fh;
			    fh.command = MAPLE_DEVICE_INFO_RESP;
			    fh.recipient = sender;
			    fh.sender = recipient;
			    fh.numWordsFollowing = 28;
			    
			    mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
			    for(int i=0; i<28; i++)
				mmu->writeDwordToExternal(resultAddr+i*4+4, ((Dword*)dev)[i]);
			}
			
			break;
			
		    case MAPLE_GET_CONDITION:
			//debugger->print("Maple: get condition of %02x (on port %d)\n", recipient, bits(recipient, 7, 6));
			//printf("%08x params[0]=%08x\n", dmaAddr, mmu->access(dmaAddr+4, MMU_READ_DWORD));
						
			dev = controller->returnInfo(recipient);
			
			if(dev->suppFuncs==0) 
			{
			    
			    FrameHeader fh;
			    fh.command = MAPLE_NORESP_ERR;
			    mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
			} 
			else 
			{
			    
			    FrameHeader fh;
			    fh.command = MAPLE_DATA_TRANSFER_RESP;
			    fh.recipient = sender;
			    fh.sender = recipient;
			    fh.numWordsFollowing = controller->returnSize(recipient);
			    
			    mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
			    mmu->writeDwordToExternal(resultAddr+4, dev->suppFuncs);
			    
			    Dword *data = controller->returnData(recipient);
			    
			    for(int i=0; i<(fh.numWordsFollowing-1); i++) {
				mmu->writeDwordToExternal(resultAddr+8+i*4, data[i]);
			    }
			}
			break;
		    default:
			printf("MAPLE_STATE: unknown command %0x\n", cmd);
		    }
		    
		    dmaPtr += 8 + (length + 1)*4;					
		} while (last != 1);

		//intc->externalInt9a(MAPLE_DMA); // Maple DMA transfer done
		setAsicA(MAPLE_DMA);

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
	
    default: debugger->flamingDeath("Invalid access to Maple::mapleHook");
    }
    return 0;
}
