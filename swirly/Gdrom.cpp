#include "Gdrom.h"
#include "Overlord.h"
#include "Debugger.h"
#include "SHMmu.h"
#include "SHCpu.h"
#include "SHDmac.h" // dma..... *NOT WORKING AT ALL*

Gdrom::Gdrom(int startsector, int sectorsize)
	: startSector_(startsector), sectorSize(sectorsize), cdImage(NULL) {}

Gdrom::~Gdrom() {}

void Gdrom::hook() 
{
    if(cpu->R[6] != 0) 
    {
	debugger->print("Gdrom::hook: r6 != 0 so doing nothing.\n");
	return;
    }

    switch(cpu->R[7]) 
    {
    case GDROM_EXECSERVER:
	// start executing commands
	debugger->print("Gdrom: syscall: GDROM_EXECSERVER\n");
	break;
    case GDROM_INITSYSTEM:
	// this is a nop for us
	debugger->print("Gdrom: syscall: GDROM_INITSYSTEM\n");
	break;
    case GDROM_GETDRVSTAT:
	// get type of disc
	// r4 = addr of param block
	// XXX: what is the structure of the param block?
	//debugger->print("Gdrom: syscall: GDROM_GETDRVSTAT\n");
	// we'll pretend we have an XA disc
	//printf("PC is %08x\n", cpu->PC);
	
	// CDDA     0x00
	// CDROM    0x10
	// CDROM_XA 0x20
	// CD_CDI   0x30
	// CD_GDROM 0x80

	mmu->writeDword(cpu->R[4]+4, 0x20);
	break;
    case GDROM_G1DMAEND:
	debugger->print("Gdrom: syscall: GDROM_G1DMAEND\n");
	break;
    case GDROM_REQDMATRANS:
	debugger->print("Gdrom: syscall: GDROM_REQDMATRANS\n");
	break;
    case GDROM_CHECKDMATRANS:
	debugger->print("Gdrom: syscall: GDROM_CHECKDMATRANS\n");
	break;
    case GDROM_READABORT:
	debugger->print("Gdrom: syscall: GDROM_READABORT\n");
	break;
    case GDROM_RESET:
	debugger->print("Gdrom: syscall: GDROM_RESET\n");
	break;
    case GDROM_CHANGEDATATYPE:
	// sets the format of the disc
	// we want to know the sector size
	// param[0]: 0=set, 1=get
	// param[1]: 8192, for some reason
	// param[2]: 2048 = mode1, 1024=mode2
	// param[3]: sector size
	// r4 = addr of param block
	debugger->print("Gdrom: syscall: GDROM_CHANGEDATATYPE\n");
	{
	    int operation = mmu->readDword(cpu->R[4]);
	    if(operation == 1)
		debugger->flamingDeath("GDROM_CHANGEDATATYPE get data type not implemented!");
	    sectorSize = mmu->readDword(cpu->R[4]+12);
	    cpu->R[0] = 0;
	}
	break;
    case GDROM_GETCMDSTAT:
	// checks whether a command has completed?
	// r4 = command id
	// r5 = addr of param block
	// return 1 if still pending
	// return 2 if done
	debugger->print("Gdrom: syscall: GDROM_GETCMDSTAT\n");
	cpu->R[0] = 2;
	break;
    case GDROM_REQCMD:
	// adds a command to the GDROM command queue?
	// r4=cmd, r5=addr of param block
	debugger->print("Gdrom: syscall: GDROM_REQCMD ");
	switch(cpu->R[4]) 
	{
	case GDROM_CMD_INIT:
	    debugger->print("GDROM_CMD_INIT");
	    break;
	case GDROM_CMD_GETTOC2:
	    // param[0] = session
	    // param[1] = pointer to buffer to receive TOC info
	    // TOC structure:
	    // Dword entry[99];
	    // Dword first, last, unknown;
	    // The data track should have a ctrl of 4
	    // ctrl is found in the high 4 bits of track num
	    // LBA is the lower 3 bytes.  To get real sector number
	    // from LBA, add 150.
	    debugger->print("Gdrom: syscall: GDROM_CMD_READTOC");
	    {
		//Dword session = mmu->readDword(cpu->R[5]);
		Dword tocBuffer = mmu->readDword(cpu->R[5]+4);
		debugger->print(" tocBuffer is at %08x", tocBuffer);
		
		Toc toc;
		toc.first = 1 << 16;
		toc.last = 2 << 16;
		toc.entry[0] = 0;
		toc.entry[1] = 0x40000000 + startSector_;
		
		// copy it into SH memory
		Overlord::copyFromHost(cpu, tocBuffer, (void*)&toc, sizeof(Toc));
	    }
	    break;
	case GDROM_CMD_PIOREAD:
	    // param[0] = start sector
	    // param[1] = number of sectors
	    // param[2] = pointer to buffer
	    // param[3] = ?
	    debugger->print("GDROM_CMD_READSECTORS");
	    {
		Dword start = mmu->readDword(cpu->R[5]);
		Dword count = mmu->readDword(cpu->R[5]+4);
		Dword buffer = mmu->readDword(cpu->R[5]+8);
		debugger->print(" start %u count %u buffer %08x sectorSize %d", start, count, buffer, sectorSize);
		count *= sectorSize;
		start -= startSector_;
		start *= sectorSize;
		debugger->print(" really starting at %u bytes", start);
		if(cdImage != NULL)
		    Overlord::load(cdImage, start, count, cpu, buffer);
		else
		    debugger->print("cdImage is null");
	    }
	    break;
	case GDROM_CMD_DMAREAD:

	    printf("\nGdrom is attempting to do DMA. We are NOT ready for this.\n");
	    printf("command block: %08x %08x %08x %08x %08x\n",
		   mmu->readDword(cpu->R[5]),    // size
		   mmu->readDword(cpu->R[5]+4),  // count
		   mmu->readDword(cpu->R[5]+8),  // address to do DMA to
		   mmu->readDword(cpu->R[5]+12), // ??? 
		   mmu->readDword(cpu->R[5]+16)); // address to get DMA from?

	    dmac->startGdromDma(mmu->readDword(cpu->R[5]+8),   // dest
				mmu->readDword(cpu->R[5]),     // size
				mmu->readDword(cpu->R[5]+4));  // repeat

	    break;
	default:
	    debugger->print("Unknown command %d\n", cpu->R[4]);
	    debugger->print("PC is %08x, command block is %08x\n",
				 cpu->PC, cpu->R[5]);
	    printf("command block: %08x %08x %08x %08x %08x\n",
		   mmu->readDword(cpu->R[5]),   
		   mmu->readDword(cpu->R[5]+4), 
		   mmu->readDword(cpu->R[5]+8), 
		   mmu->readDword(cpu->R[5]+12),
		   mmu->readDword(cpu->R[5]+16)); 
	    break;
	}
	debugger->print("\n");
	break;
    }
}

// selects the CDROM image to use
void Gdrom::load(char *fname) 
{
    if(cdImage != NULL)
	fclose(cdImage);
    cdImage = fopen(fname, "rb");
}
