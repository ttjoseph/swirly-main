// SHMmu.cpp: SH4 MMU implementation

#include "swirly.h"
#include "SHMmu.h"
#include "SHCpu.h"
#include "Debugger.h"
#include "SHUbc.h"
#include "SHTmu.h"
#include "Maple.h"
#include "Gpu.h"
#include "Modem.h"
#include "SHBsc.h"
#include "SHDmac.h"
#include "SHSci.h"
#include "SHIntc.h"
#include "Spu.h"

int pageAccessRight[16] = { /* user */ 1,1,0,0,1,1,1,0, /*priv*/ 0,0,0,0,1,0,1,0 };

SHMmu::SHMmu()
{
	mem = new Byte[MMU_DEFAULT_MEM_SIZE];
	cache = new Byte[MMU_CACHE_SIZE];
	flash = new Byte[MMU_FLASH_SIZE];
	videoMem = new Byte[MMU_VIDEOMEM_SIZE];
	bootRom = new Byte[MMU_BOOTROM_SIZE];
	soundMem = new Byte[MMU_SOUNDMEM_SIZE];
	texMem = new Byte[MMU_TEXMEM_SIZE];

	for (int i = 0; i < 64; i++) 
		UTLB_Data1[i] = 0;
	for (int j = 0; j < 4; j++) 
		ITLB_Data1[j] = 0;
}

SHMmu::~SHMmu()
{
	delete [] mem;
	delete [] cache;
	delete [] flash;
	delete [] videoMem;
	delete [] bootRom;
	delete [] soundMem;
	delete [] texMem;
}

int SHMmu::readByteTLB(Dword accessAddr, Byte* data)
{
	if (translateVirtual(accessAddr, MMU_READ)) return true;
	*data =  (Byte) access(accessAddr, MMU_READ_BYTE);
	return false;
}

int SHMmu::readWordTLB(Dword accessAddr, Word* data)
{
	if (translateVirtual(accessAddr, MMU_READ)) return true;
	*data =  (Word) access(accessAddr, MMU_READ_WORD);
	return false;
}

int SHMmu::readDwordTLB(Dword accessAddr, Dword* data)
{
	if (translateVirtual(accessAddr, MMU_READ)) return true;
	*data =  access(accessAddr, MMU_READ_DWORD);
	return false;
}

int SHMmu::readQwordTLB(Dword accessAddr, Dword* data1, Dword* data2)
{
	if (translateVirtual(accessAddr, MMU_READ)) return true;
	*data1 =  access(accessAddr, MMU_READ_DWORD);
	*data2 =  access(accessAddr+4, MMU_READ_DWORD);
	return false;
}

Byte SHMmu::readByte(Dword accessAddr)
{
	return (Byte) access(accessAddr, MMU_READ_BYTE);
}

Word SHMmu::readWord(Dword accessAddr)
{
	return (Word) access(accessAddr, MMU_READ_WORD);
}

Dword SHMmu::readDword(Dword accessAddr)
{
	return access(accessAddr, MMU_READ_DWORD);
}

void SHMmu::writeByte(Dword accessAddr, Byte d)
{
	access(accessAddr, MMU_WRITE_BYTE, (Dword) d);
}

void SHMmu::writeWord(Dword accessAddr, Word d)
{
	access(accessAddr, MMU_WRITE_WORD, (Dword) d);
}

void SHMmu::writeDword(Dword accessAddr, Dword d)
{
	access(accessAddr, MMU_WRITE_DWORD, d);
}

int SHMmu::writeByteTLB(Dword accessAddr, Byte data)
{
	if (translateVirtual(accessAddr, MMU_WRITE)) return true;
	access(accessAddr, MMU_WRITE_BYTE, data);
	return false;
}

int SHMmu::writeWordTLB(Dword accessAddr, Word data)
{
	if (translateVirtual(accessAddr, MMU_WRITE)) return true;
	access(accessAddr, MMU_WRITE_WORD, data);
	return false;
}

int SHMmu::writeDwordTLB(Dword accessAddr, Dword data)
{
	if (translateVirtual(accessAddr, MMU_WRITE)) return true;
	access(accessAddr, MMU_WRITE_DWORD, data);
	return false;
}

int SHMmu::writeQwordTLB(Dword accessAddr, Dword data1, Dword data2)
{
	if (translateVirtual(accessAddr, MMU_WRITE)) return true;
	access(accessAddr, MMU_WRITE_DWORD, data1);
	access(accessAddr+4, MMU_WRITE_DWORD, data2);
	return false;
}

void SHMmu::writeDwordToExternal(Dword accessAddr, Dword d)
{
	access(accessAddr, MMU_WRITE_DWORD, d);
}

void SHMmu::writeDouble(Dword addr, Double d)
{
	writeDword(addr, *(((Dword*)&d)));
	writeDword(addr+4, *(((Dword*)&d)+4));
	// debugger->flamingDeath("writeDouble not implemented yet");
}

void SHMmu::writeFloat(Dword addr, Float d)
{
	float foo = d;
	Dword *foo2;
	foo2 = (Dword*)&foo;
	writeDword(addr, *foo2);
}

Double SHMmu::readDouble(Dword addr)
{
	//	debugger->flamingDeath("readDouble not implemented yet");
	Dword tmp[2];
	tmp[0] = readDword(addr);
	tmp[1] = readDword(addr+4);
	return *((Double*) tmp);
}

Float SHMmu::readFloat(Dword addr)
{
	// XXX: isn't there a better way to do this?
	float *foo;
	Dword foo2 = readDword(addr);
	foo = (float*) &foo2;
	return *foo;
}

int SHMmu::accessITLB(Dword &addr)
{
	// find an entry
	int theentry = searchItlb(addr);
	// we couldn't find an entry, so throw an exception
	if(theentry == MMU_TLB_MISS) {
		int utlbentry = searchUtlb(addr);
		if(utlbentry == MMU_TLB_MISS) { // sheesh, not even in the UTLB
			CCNREG(TEA) = addr;
			CCNREG(PTEH) = (CCNREG(PTEH) & 0x3ff) | (addr & ~0x3ff);
			intc->exception(E_INSTRUCTION_TLB_MISS, addr, 0);
			return true;
		}

		// we must now copy the UTLB entry into the ITLB
		// To determine which ITLB entry to use, we look at
		// MMUCR.LRUI
		// LRUI is in bits 31-26 of MMUCR
		int lrui = CCNREG(MMUCR) >> 26;
		if((lrui & 0x38) == 0x38) {
			theentry = 0; CCNREG(MMUCR) &= 0x1fffffff;
		} else if((lrui & 0x26) == 0x06) {
			theentry = 1; CCNREG(MMUCR) &= 0xe7ffffff; CCNREG(MMUCR) |= 0x80000000;
		} else if((lrui & 0x15) == 0x01) {
			theentry = 2; CCNREG(MMUCR) &= 0xfbffffff; CCNREG(MMUCR) |= 0x50000000;
		} else if((lrui & 0x0b) == 0x00) {
			theentry = 3; CCNREG(MMUCR) |= 0x2c000000;
		} else
			debugger->flamingDeath("MMUCR.LRUI is set to some weird setting!");
				
		ITLB_Addr[theentry] = UTLB_Addr[utlbentry];
		ITLB_Data1[theentry] = UTLB_Data1[utlbentry];
		ITLB_Data2[theentry] = UTLB_Data2[utlbentry];
		ITLB_Mask[theentry] = UTLB_Mask[utlbentry];
	} // if(can't find entry)

	if(!(cpu->SR & F_SR_MD)) { // SR.MD == 0?
		if(!(ITLB_Data1[theentry] & 0x40)) {
			intc->exception(E_INSTRUCTION_TLB_PROTECTION_VIOLATION, addr, 0);
			return true;
		}
	}
	
	addr = (ITLB_Data1[theentry] & ITLB_Mask[theentry]) | (addr & ~ITLB_Mask[theentry]);
	return false;
}

// XXX: avoid recursive call of fetchInstruction
Word SHMmu::fetchInstruction(Dword addr)
{
	if (addr < 0x80000000) {	// P0, U0
		// address translation on
		if (cpu->ccnRegs[MMUCR] & 1) {
			if (accessITLB(addr)) return fetchInstruction(cpu->PC); // new attempt
			return access(addr, MMU_READ_WORD);
		}
		// address translation off
		addr &= 0x1fffffff;
		return access(addr, MMU_READ_WORD);
	}

	// no user mode after this point
	if (!(cpu->SR & F_SR_MD)) {
		intc->exception(E_INSTRUCTION_ADDRESS_ERROR, addr, 0);
		return fetchInstruction(cpu->PC);
	}

	if (addr < 0xc0000000) {	// P1, P2
		// no address translation
		addr &= 0x1fffffff;
		return access(addr, MMU_READ_WORD);
	}
	
	if (addr < 0xe0000000) {	// P3
		// address translation on
		if (cpu->ccnRegs[MMUCR] & 1) {
			if (accessITLB(addr)) return fetchInstruction(cpu->PC);
			return access(addr, MMU_READ_WORD);
		}
		// address translation off
		addr &= 0x1fffffff;
		return access(addr, MMU_READ_WORD);
	}

	intc->exception(E_INSTRUCTION_ADDRESS_ERROR, addr, 0);
	return fetchInstruction(cpu->PC);
}


int SHMmu::accessUTLB(Dword &addr, int type)
{
	// find an entry
	int theentry = searchUtlb(addr);
	// we couldn't find an entry, so throw an exception
	if(theentry == MMU_TLB_MISS) {
		CCNREG(TEA) = addr;
		CCNREG(PTEH) = (CCNREG(PTEH) & 0x3ff) | (addr & ~0x3ff);

		if(type & MMU_WRITE)
			intc->exception(E_DATA_TLB_MISS_WRITE, addr, 0);
		else
			intc->exception(E_DATA_TLB_MISS_READ, addr, 0);
		return true;
	}

	// Now we know that VPNs match and V=1(the entry is valid)
	// We must check for Data TLB protection violation exception
	// To do this we use the PR bits, in UTLB_Data1, bits 6-5
//	printf("SHMmu::translateVirtual: address %08x RPN %08x\n", addr, UTLB_Data1[theentry]);

	int protection = (UTLB_Data1[theentry] >> 5) & 3;
	if (type & MMU_WRITE) protection |= 0x4;
	if (cpu->SR & F_SR_MD) protection |= 0x8;

	if (pageAccessRight[protection]) {
		if(type & MMU_WRITE)
			intc->exception(E_DATA_TLB_PROTECTION_VIOLATION_WRITE, addr, 0);
		else		
	        intc->exception(E_DATA_TLB_PROTECTION_VIOLATION_READ, addr, 0);
		return true;
	}

	if (((UTLB_Data1[theentry] & 0x4) == 0) && (type & MMU_WRITE)) {
		intc->exception(E_INITAL_PAGE_WRITE, addr, 0);
		return true;
	}

	// now we figure the page size and then return the correct
	// physical address
	// SZ is in bits 7 and 4 of UTLB_Data1[theentry]
	addr = (UTLB_Data1[theentry] & UTLB_Mask[theentry]) | (addr & ~UTLB_Mask[theentry]);

	// change addresses 0x1c000000-0x1fffffff to 0xfc000000-0xffffffff
	if (addr > 0x1c000000) addr |= 0xe0000000; 
	return false;
}

// translates an SH virtual address using UTLB
int SHMmu::translateVirtual(Dword &addr, int type)
{
	debugger->checkMemBp(addr);
	// XXX: make this actually work

	if (addr < 0x80000000) {	// P0, U0
		// address translation on
		if (cpu->ccnRegs[MMUCR] & 1) return accessUTLB(addr, type);
		// address translation off
		addr &= 0x1fffffff;
		return false;
	}

	// Special Store Queue treatment, at the moment just return the vpn addr
	// XXX: add SQ MMU entry search, ...
	if ((addr >= 0xe0000000) && (addr < 0xe4000000)) {
		return false;
	}

	// no user mode after this point
	if (!(cpu->SR & F_SR_MD)) {
		if(type & MMU_WRITE)
			intc->exception(E_DATA_ADDRESS_ERROR_WRITE, addr, 0);
		else		
			intc->exception(E_DATA_ADDRESS_ERROR_READ, addr, 0);
		return true;
	}

	if (addr < 0xc0000000) {	// P1, P2
		// address translation off
		addr &= 0x1fffffff;
		return false;
	}

	if (addr < 0xe0000000) {	// P3
		// address translation on
		if (cpu->ccnRegs[MMUCR] & 1) return accessUTLB(addr, type);
		// address translation off
		addr &= 0x1fffffff;
		return false;
	}

	// P4 area, return addr
	return false;
}

int SHMmu::checkAsids(Dword tlbentry)
{
	// see if ASIDs should match and if so, check if they do
	// (SH==0) and (MMUCR.SV==0 or SR.MD==0)?
	if(!(tlbentry & 2) &&
	   (((*((Dword*)(cpu->ccnRegs+MMUCR)) & 0x100) == 0) ||
	    ((cpu->SR & 0x40000000) == 0)))
	{
		// ASIDs must match
		if((*((Dword*)(cpu->ccnRegs+PTEH))&0xff) != (tlbentry & 0xff))
			return 0;
		else
			return 1;
	}
	else // they don't have to match, so say everything's ok
		return 1;
}

int SHMmu::searchUtlb(Dword addr)
{
	int i, theentry = MMU_TLB_MISS; // nonsense value to signal an error

	updateMmucrUrc();
	for(i=0;i<64;i++)
	{
		// V must be 1 (entry must be valid)
		if(!(UTLB_Data1[i] & 0x100)) continue;
		if((UTLB_Addr[i] & UTLB_Mask[i]) == (addr & UTLB_Mask[i]))
		{
			if(checkAsids(UTLB_Addr[i]))
			{
				theentry=i;
				break;
			}
		}
	} // UTLB search
	return theentry;
    
}

void SHMmu::updateMmucrUrc()
{
	Byte urb, urc;
	
	urb = (CCNREG(MMUCR) >> 18) & 0x3f;
	urc = (CCNREG(MMUCR) >> 10) & 0x3f;
	if((urb > 0) && (urb == urc))
		urc = 0;
	else
		urc++;

	CCNREG(MMUCR) &= ~(0x3f << 10);
	CCNREG(MMUCR) |= ((urc & 0x3f) << 10);	
}

int SHMmu::searchItlb(Dword addr)
{
	int i, theentry = MMU_TLB_MISS; // nonsense value to signal an error
	for(i=0;i<4;i++)
	{
		// V must be 1 (entry must be valid)
		if(!(ITLB_Data1[i] & 0x100)) continue;
		if((ITLB_Addr[i] & ITLB_Mask[i]) == (addr & ITLB_Mask[i]))
		{
			if(checkAsids(ITLB_Addr[i]))
			{
				theentry=i;
				break;
			}
		}
	} // ITLB search
	return theentry;
}

void SHMmu::ldtlb()
{
	int addr = bits(CCNREG(MMUCR), 15, 10);
	printf("SHMmu::ldtlb: at %02x with PTEH: %08x PTEL %08x PTEA %02x\n", addr, CCNREG(PTEH), CCNREG(PTEL),CCNREG(PTEA));
	UTLB_Addr[addr] = CCNREG(PTEH);
	UTLB_Data1[addr] = CCNREG(PTEL) & 0x1fffffff;
	UTLB_Data2[addr] = CCNREG(PTEA);

	switch(UTLB_Data1[addr] & 0x90) {
		case 0x00: // 1K pages
			UTLB_Mask[addr] = 0xfffffc00; break;
		case 0x10: // 4K pages
	        UTLB_Mask[addr] = 0xfffff000; break;
		case 0x80: // 64K pages
	        UTLB_Mask[addr] = 0xffff0000; break;
		case 0x90: // 1M pages
	        UTLB_Mask[addr] = 0xfff00000; break;
	}
}

// sends contents of currently selected store queue
void SHMmu::storeQueueSend(Dword target) {

	Dword *sq, addr, qacr;
	
	if(((target >> 5) & 1) == 0) {
		sq = SQ0;
		qacr = CCNREG(QACR0);
	} else {
		sq = SQ1;
		qacr = CCNREG(QACR1);
	}
	
	addr = (target & 0x03ffffe0) | ((qacr & 0x1c) << 24);
	
	if(addr == 0x10000000) { // tile accelerator
		for(int i=0; i<8; i++)
			gpu->handleTaWrite(0x10000000, sq[i]);
		return;
	}
	
	for(int i=0; i<8; i++) {
		access(addr, MMU_WRITE_DWORD, sq[i]);
		addr+=4;
	}
}

// accesses P4 / area7
// we assume that the 3 high bits in accessAddr are correct
Dword SHMmu::accessP4(Dword accessAddr, int eventType, Dword tempData)
{
	Dword realAddr= 0, addr = accessAddr & 0xffffff;
	
	// XXX: Store queues don't work when AT is on
	// Handle store queue access
	// Accesses to this area should always be writes...
	if((accessAddr & 0xfc000000) == 0xe0000000)
	{
		// set the store queue data
		if(((accessAddr >> 5) & 1) == 0)
			SQ0[(accessAddr >> 2) & 7] = tempData;
		else
			SQ1[(accessAddr >> 2) & 7] = tempData;
		return tempData;
	}
    
	switch(addr >> 16)
	{
	case 0x00: realAddr = (Dword) cpu->ccnRegs + (accessAddr & 0xff); break;
	case 0x20: return(tempData = ubc->hook(eventType, accessAddr, tempData));
	case 0xC0: debugger->print("SHMmu:accessP4: WDT %08x (%08x)\n", accessAddr, tempData); return 0;
	case 0x80: return(tempData = bsc->hook(eventType, accessAddr, tempData));
	case 0xA0: return(tempData = dmac->hook(eventType, accessAddr, tempData));
	case 0xD0: return(tempData = intc->hook(eventType, accessAddr, tempData));
	case 0xD8: return(tempData = tmu->hook(eventType, accessAddr, tempData));
	case 0xE0:
	case 0xE8: return(tempData = sci->hook(eventType, accessAddr, tempData));
	case 0x90:
	case 0x94: return(tempData = bsc->hook(eventType, accessAddr, tempData)); 
	case 0xC8: debugger->print("SHMmu:accessP4: RTC %08x\n", accessAddr); return 0;
	default: debugger->flamingDeath("SHMmu:accessP4: Tried to access %08x, which I can't handle", accessAddr);
	}
    
	switch(eventType)
	{
	case MMU_READ_DWORD: tempData = *((Dword*)(realAddr)); break;
	case MMU_READ_BYTE:  tempData = *((Byte *)(realAddr)); break;
	case MMU_READ_WORD:  tempData = *((Word *)(realAddr)); break;
	case MMU_WRITE_DWORD: *((Dword*)(realAddr)) = tempData; break;
	case MMU_WRITE_BYTE:  *((Byte *)(realAddr)) = (Byte) tempData; break;
	case MMU_WRITE_WORD:  *((Word *)(realAddr)) = (Word) tempData; break;
	}
	return tempData;
}

// kitchen-sink function to perform a memory access given an external address
// accepts and returns data in this->tempData
Dword SHMmu::access(Dword externalAddr, int accessType, Dword tempData)
{
    Dword realAddr = 0;

    // XXX: check for exceptions
	
	// check for P4
    if(externalAddr >= 0xe0000000) return accessP4(externalAddr, accessType, tempData);

	// handle different areas
	switch((externalAddr >> 26) & 7) {
	case 0: // boot ROM, flash, regs of things
	    switch((externalAddr >> 21) & 0x1f) {
	    case 0: realAddr = (Dword) bootRom + (externalAddr & 0x1fffff); break;
	    case 1: realAddr = (Dword) flash + (externalAddr & 0x3ffff); break;
	    case 2:
		switch(externalAddr & 0xf000) {
		case 0x8000: return(tempData = gpu->hook(accessType, externalAddr, tempData));
		case 0x7000: 
		    switch(externalAddr&0xf00) 
		    {
		    case 0x000: return (tempData = maple->unknownHook(accessType, externalAddr, tempData));
		    default: break;
		    }
		    // 4e4 is cdrom register -- has size of whatever is going to be read
		case 0x6000: 
		    switch(externalAddr&0xf00)
		    { 
		    case 0x400: return (tempData = modem->hook(accessType, externalAddr, tempData));
		    case 0x800: return (tempData = maple->g2Hook(accessType, externalAddr, tempData));
		    case 0x900: return (tempData = maple->asicHook(accessType, externalAddr, tempData));
		    case 0xc00: return (tempData = maple->mapleHook(accessType, externalAddr, tempData));
		    default: break; // fall through
		    }
		default:
		    
		    if(accessType&MMU_READ)
			debugger->print("SHMmu::access: can't handle read to external addr %08X, PC=%08X\n", externalAddr, cpu->PC);
		    if(accessType&MMU_WRITE)
			debugger->print("SHMmu::access: can't handle write to external addr %08X, PC=%08X\n", externalAddr, cpu->PC);
		    debugger->flamingDeath("SHMMu:unhandled access to external address");
		    return 0xdeadbeef;
		}
		break;
	    case 3:
		return(tempData = spu->hook(accessType, externalAddr, tempData));
	    case 4:
		// 0x10000000 to 0x13000000

		// XXX: not sure about this... 
		switch(externalAddr>>24)
		{
		case 0: // SPU RAM
		    //if((externalAddr & 0xffffff) == 0x0080ffc0) // XXX: this isn't very clean - checks for snd reg
		    //return(tempData = spu->hook(accessType, externalAddr, tempData));
		    
		    realAddr = (Dword) soundMem + (externalAddr & 0x1fffff); 
		    break;
		case 1: // texture RAM

		    printf("access tex ram at %08x\n", externalAddr);

		    realAddr = (Dword) texMem + (externalAddr & 0x00ffffff);
		    break;
		default: 
		    printf("SHMmu::access: unknown addr at %08x\n", externalAddr);
		}

		break;
	    case 8: return 0;
	    }
	    break;
	case 1: 
	    realAddr = (Dword) videoMem + (externalAddr & 0xffffff); 
	    break;
	case 3: 
	    realAddr = (Dword) mem + (externalAddr & 0xffffff); 
	    break;
	case 4: 
	    gpu->handleTaWrite(externalAddr, tempData); 
	    return 0; 
	    break;
	case 7: // cache used as memory
	    if(externalAddr & 0x2000) // cache RAM area 2
		realAddr = (externalAddr & 0xfff) | 0x1000;
	    else // RAM area 1
		realAddr = externalAddr & 0xfff;
	    realAddr += (Dword) cache;
	    break;
	}
    
    switch(accessType)
    {
    case MMU_READ_BYTE:	 tempData = *((Byte*)(realAddr)); break;
    case MMU_READ_WORD:  tempData = *((Word*)(realAddr)); break;
    case MMU_READ_DWORD: tempData = *((Dword*)(realAddr)); break;
	
    case MMU_WRITE_BYTE:  *((Byte*)(realAddr)) = (Byte) tempData; break;
    case MMU_WRITE_WORD:  *((Word*)(realAddr)) = (Word) tempData; break;
    case MMU_WRITE_DWORD: *((Dword*)(realAddr)) = tempData; break;
    }
    
    return tempData;
}
