// SHMmu.cpp: SH4 MMU implementation

#include "swirly.h"
#include "SHMmu.h"
#include "SHCpu.h"

SHMmu::SHMmu(class SHCpu *cpu)
{
	this->cpu = cpu;
	mem = new Byte[MMU_DEFAULT_MEM_SIZE];
	cache = new Byte[MMU_CACHE_SIZE];
	flash = new Byte[MMU_FLASH_SIZE];
	videoMem = new Byte[MMU_VIDEOMEM_SIZE];
	bootRom = new Byte[MMU_BOOTROM_SIZE];
	soundMem = new Byte[MMU_SOUNDMEM_SIZE];
}

SHMmu::~SHMmu()
{
	delete mem;
	delete cache;
	delete flash;
	delete videoMem;
	delete bootRom;
	delete soundMem;
}

Byte SHMmu::readByte(Dword addr)
{
	eventType = MMU_READ_BYTE;
	accessAddr = addr;
	access(translateVirtual(addr), MMU_READ_BYTE);
	return (Byte) tempData;
}

Word SHMmu::readWord(Dword addr)
{
	eventType = MMU_READ_WORD;
	accessAddr = addr;
	access(translateVirtual(addr), MMU_READ_WORD);
	return (Word) tempData;

}

Dword SHMmu::readDword(Dword addr)
{
	eventType = MMU_READ_DWORD;
	accessAddr = addr;
	access(translateVirtual(addr), MMU_READ_DWORD);
	return tempData;
}


void SHMmu::writeByte(Dword addr, Byte d)
{
	eventType = MMU_WRITE_BYTE;
	tempData = (Dword) d;
	accessAddr = addr;
	access(translateVirtual(addr), MMU_WRITE_BYTE);
}

void SHMmu::writeWord(Dword addr, Word d)
{
	eventType = MMU_WRITE_WORD;
	tempData = (Dword) d;
	accessAddr = addr;
	access(translateVirtual(addr), MMU_WRITE_WORD);
}

void SHMmu::writeDword(Dword addr, Dword d)
{
	eventType = MMU_WRITE_DWORD;
	tempData = d;
	accessAddr = addr;
	access(translateVirtual(addr), MMU_WRITE_DWORD);
}

void SHMmu::writeDwordToExternal(Dword addr, Dword d)
{
	eventType = MMU_WRITE_DWORD;
	tempData = d;
	accessAddr = addr;
	access(addr, MMU_WRITE_DWORD);
}

void SHMmu::writeDouble(Dword addr, Double d)
{
	cpu->debugger->flamingDeath("writeDouble not implemented yet");
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
	cpu->debugger->flamingDeath("readDouble not implemented yet");
	return 0;
}

Float SHMmu::readFloat(Dword addr)
{
	// XXX: isn't there a better way to do this?
	float *foo;
	Dword foo2 = readDword(addr);

	foo = (float*) &foo2;
	return *foo;
}

Word SHMmu::fetchInstruction(Dword addr)
{
	unsigned int theentry;
	Dword lrui, utlbentry;

	eventType = MMU_READ_WORD;
	
	switch(addr >> 29) // check which area we want
	{
		case 0: // P0, U0
		case 6: // P3
			if(*((Dword*)(cpu->ccnRegs+MMUCR))&1) // MMUCR.AT = 1?
			{
				theentry = searchItlb(addr);
				// we couldn't find an entry, so search the UTLB
				if(theentry == MMU_TLB_MISS)
				{
					utlbentry = searchUtlb(addr);
					if(utlbentry == MMU_TLB_MISS) // sheesh, not even in the UTLB
					{
						cpu->exception(E_INSTRUCTION_TLB_MISS, addr, 0);
						return 0;
					}
					// we must now copy the UTLB entry into the ITLB
					// To determine which ITLB entry to use, we look at
					// MMUCR.LRUI
					// LRUI is in bits 31-26 of MMUCR
					lrui = CCNREG(MMUCR) >> 26;
					if((lrui & 0x38) == 0x38)
						theentry = 0;
					else if((lrui & 0x26) == 0x06)
						theentry = 1;
					else if((lrui & 0x15) == 0x01)
						theentry = 2;
					else if((lrui & 0x0b) == 0x00)
						theentry = 3;
					else
						cpu->debugger->flamingDeath("MMUCR.LRUI is set to some weird setting!");

					ITLB_Addr[theentry] = UTLB_Addr[utlbentry];
					ITLB_Data1[theentry] = UTLB_Data1[utlbentry];
					ITLB_Data2[theentry] = UTLB_Data2[utlbentry];
				} // if(can't find entry)

				// now we've found an entry!
				// let's update LRUI now.
				switch(theentry)
				{
				case 0:
					CCNREG(MMUCR) &= 0x1fffffff;
					break;
				case 1:
					CCNREG(MMUCR) &= 0xe7ffffff;
					CCNREG(MMUCR) |= 0x80000000;
					break;
				case 2:
					CCNREG(MMUCR) &= 0xfbffffff;
					CCNREG(MMUCR) |= 0x50000000;
					break;
				case 3:
					CCNREG(MMUCR) |= 0x2c000000;
					break;
				default:
					cpu->debugger->flamingDeath("Trying to look at an ITLB entry that can't exist.");
				}

				if(cpu->SR & F_SR_MD) // SR.MD == 1?
				{
					// if we're privileged we can do anything
					// now we figure the page size and then return the correct
					// physical address
					// SZ is in bits 7 and 4 of ITLB_Data1[theentry]
					switch(ITLB_Data1[theentry] & 9)
					{
						case 0: // 1K pages - use PPN[28:10] and offset[9:0]
							return (GETWORD(((ITLB_Data1[theentry] & 0x1ffffc00) | (addr & 0x3ff))));
						case 1: // 4K pages
							return (GETWORD(((ITLB_Data1[theentry] & 0x1ffff000) | (addr & 0xfff))));
						case 8: // 64K pages
							return (GETWORD(((ITLB_Data1[theentry] & 0x1fff8000) | (addr & 0xffff))));
						case 9: // 1M pages
							return (GETWORD(((ITLB_Data1[theentry] & 0x1ff00000) | (addr & 0xfffff))));
					}
				}
				else
				{
					// must check PR - bit 6 in ITLB_Data1
					if((ITLB_Data1[theentry] >> 6) & 1)
					{
						// now we figure the page size and then return the correct
						// physical address
						// SZ is in bits 7 and 4 of ITLB_Data1[theentry]
						switch(ITLB_Data1[theentry] & 9)

						{
							case 0: // 1K pages - use PPN[28:10] and offset[9:0]
								return (GETWORD(((ITLB_Data1[theentry] & 0x1ffffc00) | (addr & 0x3ff))));
							case 1: // 4K pages
								return (GETWORD(((ITLB_Data1[theentry] & 0x1ffff000) | (addr & 0xfff))));
							case 8: // 64K pages
								return (GETWORD(((ITLB_Data1[theentry] & 0x1fff8000) | (addr & 0xffff))));
							case 9: // 1M pages
								return (GETWORD(((ITLB_Data1[theentry] & 0x1ff00000) | (addr & 0xfffff))));
						}
					}
					else
					{
						cpu->exception(E_INSTRUCTION_TLB_PROTECTION_VIOLATION, addr, 0);
						return 0;
					}
				} // if SR.MD == 1
			}
			else // if MMUCR.AT == 1
			{
				// no address translation, thanks
				eventType = MMU_READ_WORD;
				access(addr, MMU_READ_WORD);
				return (Word) tempData;
			} // if MMUCR.AT == 1
			break; // P0, U0, P3

	case 5: // P2
	case 4: // P1
		if(cpu->SR & F_SR_MD) // no address translation
		{
			eventType = MMU_READ_WORD;
			access(addr, MMU_READ_WORD);
			return (Word) tempData;
		}
		else
		{
			cpu->exception(E_INSTRUCTION_ADDRESS_ERROR, addr, 0);
			return 0;
		}
		break;
	} // switch(addr >> 29)
	cpu->debugger->flamingDeath("Reached a point in SHMmu::fetchInstruction that we should never reach");
	return 0; // shouldn't ever reach here
}

// translates an SH virtual address to an external address
Dword SHMmu::translateVirtual(Dword addr)
{
	cpu->debugger->checkMemBp(addr);

	unsigned int theentry;
	// TODO: make this actually work
	switch(addr >> 29)
	{
		case 0: // P0, U0
		case 3:
		case 6: // P3
			// if AT bit is on (address translation on)...
			if(cpu->ccnRegs[MMUCR] & 1)
			{
				// find an entry
				theentry = searchUtlb(addr);
				// we couldn't find an entry, so throw an exception
				if(theentry == MMU_TLB_MISS)
				{
					if(eventType & MMU_READ)
						cpu->exception(E_DATA_TLB_MISS_READ, addr, 0);
					else if(eventType & MMU_WRITE)
						cpu->exception(E_DATA_TLB_MISS_WRITE, addr, 0);
					else
						cpu->debugger->flamingDeath("Something's fishy here - unknown MMU operation");

					return 0;
				} // if(can't find entry)
				// Now we know that VPNs match and V=1(the entry is valid)
				// We must check for Data TLB protection violation exception
				// To do this we use the PR bits, in UTLB_Data1, bits 6-5
				if(cpu->SR & F_SR_MD)
				{
					switch((UTLB_Data1[theentry] >> 5) & 3) // look at PR bits
					{
					case 0:
					case 2:
						if(eventType & MMU_WRITE)
						{
							cpu->exception(E_DATA_TLB_PROTECTION_VIOLATION_WRITE, addr, 0);
							return 0;
						}
						else // yes! success!!
						{
							// now we figure the page size and then return the correct
							// physical address
							// SZ is in bits 7 and 4 of UTLB_Data1[theentry]
							switch(UTLB_Data1[theentry] & 9)
							{
								case 0: // 1K pages - use PPN[28:10] and offset[9:0]
									return ((UTLB_Data1[theentry] & 0x1ffffc00) | (addr & 0x3ff));
								case 1: // 4K pages
									return ((UTLB_Data1[theentry] & 0x1ffff000) | (addr & 0xfff));
								case 8: // 64K pages
									return ((UTLB_Data1[theentry] & 0x1fff8000) | (addr & 0xffff));
								case 9: // 1M pages
									return ((UTLB_Data1[theentry] & 0x1ff00000) | (addr & 0xfffff));
							}
							// shouldn't ever reach here
							cpu->debugger->flamingDeath("SHMmu::translateVirtual: the unthinkable has happened on line %d.", __LINE__);
						} // success!
						break; // PR bit stuff
					case 1:
					case 3:
					 	// check dirty bit to see if we need an inital page write exception
					 	// that's bit 9 in UTLB_Addr
						if((eventType & MMU_WRITE) && ((UTLB_Addr[theentry] & 0x200) == 0))
					 	{
				 	 		cpu->exception(E_INITAL_PAGE_WRITE, addr, 0);
				 	 		return 0;
					 	}
					 	// yay! success!
						// now we figure the page size and then return the correct
						// physical address
						// SZ is in bits 7 and 4 of UTLB_Data1[theentry]
						switch(UTLB_Data1[theentry] & 9)
						{
							case 0: // 1K pages - use PPN[28:10] and offset[9:0]
								return ((UTLB_Data1[theentry] & 0x1ffffc00) | (addr & 0x3ff));
							case 1: // 4K pages
								return ((UTLB_Data1[theentry] & 0x1ffff000) | (addr & 0xfff));
							case 8: // 64K pages
								return ((UTLB_Data1[theentry] & 0x1fff8000) | (addr & 0xffff));
							case 9: // 1M pages
								return ((UTLB_Data1[theentry] & 0x1ff00000) | (addr & 0xfffff));
						}
						break;
					}	// switch(PR bits)
				} // if(SR.MD)
				else
				{
					switch((UTLB_Data1[theentry] >> 5) & 3) // look at PR bits
					{
						case 0:
						case 1:
							if(eventType & MMU_WRITE)
				 		 		cpu->exception(E_DATA_TLB_PROTECTION_VIOLATION_WRITE, addr, 0);
						 	else
						 		cpu->exception(E_DATA_TLB_PROTECTION_VIOLATION_READ, addr, 0);
							return 0;
							break;
						case 2:
						 	// can't write with PR set to 2
							if(eventType & MMU_WRITE)
						 	{
				 		 		cpu->exception(E_DATA_TLB_PROTECTION_VIOLATION_WRITE, addr, 0);
				 		 		return 0;
				 		 	}
				 		 	// FALL THROUGH
				 		case 3:
						 	// yay! success!
							// now we figure the page size and then return the correct
							// physical address
							// SZ is in bits 7 and 4 of UTLB_Data1[theentry]
							switch(UTLB_Data1[theentry] & 9)
							{
								case 0: // 1K pages - use PPN[28:10] and offset[9:0]
									return ((UTLB_Data1[theentry] & 0x1ffffc00) | (addr & 0x3ff));
								case 1: // 4K pages
									return ((UTLB_Data1[theentry] & 0x1ffff000) | (addr & 0xfff));
								case 8: // 64K pages
									return ((UTLB_Data1[theentry] & 0x1fff8000) | (addr & 0xffff));
								case 9: // 1M pages
									return ((UTLB_Data1[theentry] & 0x1ff00000) | (addr & 0xfffff));
							}
							break;
					} // switch(PR bits)
				} // else (SR.MD)
			} // if(MMUCR.AT)
			else // no address translation, just zero 3 high bits
				return addr & 0x1fffffff;
			break; // PO, UO, P3

		case 4: // P1
		case 5: // P2
		if(cpu->SR & F_SR_MD) // gotta be privileged to access P1
		{
			return addr & 0x1fffffff;
		}
		else // user mode can't touch this area
		{
			if(eventType & MMU_READ)
				cpu->exception(E_DATA_ADDRESS_ERROR_READ, addr, 0);
			else if(eventType & MMU_WRITE)
				cpu->exception(E_DATA_ADDRESS_ERROR_WRITE, addr, 0);
			else
				cpu->debugger->flamingDeath("Oh gosh no.  Unknown MMU operation.");
			return 0;
		}

		// XXX: we shouldn't ever reach here -- P4 should be caught before this
		case 7: // P4 area
			return addr; // whee
			//cpu->debugger->flamingDeath("translateVirtual: Attempting to handle a P4 access"
				//" that should be handled by doDriverHook()");
	}

	cpu->debugger->flamingDeath("SHMmu::translateVirtual: didn't translate");
	return addr & 0x1fffffff; // zero 3 high bits

}

int SHMmu::checkAsids(Dword tlbentry)
{
	// see if ASIDs should match and if so, check if they do
	// (SH==0) and (MMUCR.SV==0 or SR.MD==0)?
	if(!(tlbentry & 2) &&
	(
		((*((Dword*)(cpu->ccnRegs+MMUCR)) & 0x100) == 0) ||
		((cpu->SR & 0x40000000) == 0)
	))
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
		if((UTLB_Addr[i] >> 10) == (addr >> 10))
		{
		  // V must be 1 (entry must be valid)
		  if((UTLB_Addr[i] & 0x100) && checkAsids(UTLB_Addr[i]))
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

	urb = (CCNREG(MMUCR) >> 18)	& 0x3f;
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
		if((ITLB_Addr[i] >> 10) == (addr >> 10))
		{
		  // V must be 1 (entry must be valid)
		  if((ITLB_Addr[i] & 0x100) && checkAsids(ITLB_Addr[i]))
		  {
				theentry=i;
				break;
		  }
		}
	} // UTLB search
	return theentry;

}

void SHMmu::ldtlb()
{
	// XXX: write me!
	cpu->debugger->flamingDeath("SHMmu::ldtlb not implemented yet.");
}

// sends contents of currently selected store queue
void SHMmu::storeQueueSend(Dword target)
{
	Dword *sq, addr, qacr;

	if(((target >> 5) & 1) == 0)
	{
		sq = SQ0;
		qacr = CCNREG(QACR0);
	}
	else
	{
		sq = SQ1;
		qacr = CCNREG(QACR1);
	}

	addr = (target & 0x03ffffe0) | ((qacr & 0x38) << 24);

	// XXX: Special case the tile accelerator
	if(addr == 0x10000000)
	{
		cpu->gpu->recvStoreQueue(sq);
		return;
	}

	eventType = MMU_WRITE_DWORD;

	for(int i=0; i<8; i++)
	{
		tempData = sq[i];
		access(addr, MMU_WRITE_DWORD);
	}
}

// accesses P4 / area7
// we assume that the 3 high bits in accessAddr are correct
Dword SHMmu::accessP4()
{
	Dword realAddr= 0, addr = accessAddr & 0xffffff;

	// XXX: Store queues don't work when AT is on
	// Handle store queue access
	// Accesses to this area should always be writes...
	if((accessAddr & 0xFC000000) == 0xE0000000)
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
	case 0x80: return(tempData = cpu->bsc->hook(eventType, accessAddr, tempData));
	case 0xA0: return(tempData = cpu->dmac->hook(eventType, accessAddr, tempData));
	case 0xD0: return(tempData = cpu->intc->hook(eventType, accessAddr, tempData));
	case 0xD8: return(tempData = cpu->tmu->hook(eventType, accessAddr, tempData));
	case 0xE0:
	case 0xE8: return(tempData = cpu->sci->hook(eventType, accessAddr, tempData));
	default: tempData = 0;
	}

	switch(eventType)
	{
	case MMU_READ_DWORD: tempData = *((Dword*)(realAddr)); break;
	case MMU_WRITE_DWORD: *((Dword*)(realAddr)) = tempData; break;
	case MMU_READ_BYTE:	tempData = *((Byte*)(realAddr)); break;
	case MMU_READ_WORD: tempData = *((Word*)(realAddr)); break;
	case MMU_WRITE_BYTE: *((Byte*)(realAddr)) = (Byte) tempData; break;
	case MMU_WRITE_WORD: *((Word*)(realAddr)) = (Word) tempData; break;
	}
	return tempData;
}

// kitchen-sink function to perform a memory access given an external address
// accepts and returns data in this->tempData
Dword SHMmu::access(Dword externalAddr, int accessType)
{
	Dword realAddr = 0;

	// TODO: check for exceptions

	// map to external memory

	// bits 31-29: specifies section of address space
	// bits 28-26: area number within section
	// bits 25-21: within area0:
	//				0: boot ROM
	//				1: flash
	//				2: TA
	//				3: SPU
	//				4: SPU RAM
	if((externalAddr >> 29) != 7) // if not in P4
	{
		// handle different areas
		switch((externalAddr >> 26) & 7)
		{
		case 0: // boot ROM, flash, regs of things
			switch((externalAddr >> 21) & 0x1f)
			{
			case 0: realAddr = (Dword) bootRom + (externalAddr & 0x1fffff); break;
			case 1: realAddr = (Dword) flash + (externalAddr & 0x3ffff); break;
			case 2:
				switch(externalAddr & 0xf000)
				{
				case 0x8000: return(tempData = cpu->gpu->hook(eventType, accessAddr, tempData));
				case 0x6000: return(tempData = cpu->maple->hook(eventType, accessAddr, tempData));
				default:
					cpu->debugger->print("SHMmu::access: can't handle access to external addr %08X, PC=%08X\n", externalAddr, cpu->PC);
					return 0xdeadbeef;
				}
				break;
			case 3:
				return(tempData = cpu->spu->hook(eventType, externalAddr, tempData));
			case 4:
				if((externalAddr & 0xffffff) == 0x0080ffc0) // XXX: this isn't very clean - checks for snd reg
					return(tempData = cpu->spu->hook(eventType, externalAddr, tempData));
				realAddr = (Dword) soundMem + (externalAddr & 0xfffff); break;
			}
			break;
		case 1: realAddr = (Dword) videoMem + (externalAddr & 0xffffff); break;
		case 3: realAddr = (Dword) mem + (externalAddr & 0xffffff); break;
		case 4: cpu->gpu->handleTaWrite(externalAddr, tempData); return 0; break;
		case 7: // cache used as memory
			if(accessAddr & 0x2000) // cache RAM area 2
				realAddr = (externalAddr & 0xfff) | 0x1000;
			else // RAM area 1
				realAddr = externalAddr & 0xfff;
			realAddr += (Dword) cache;
			break;
		}
	} else // do P4 stuff
		return accessP4();

	switch(accessType)
	{
	case MMU_READ_BYTE:	tempData = *((Byte*)(realAddr)); break;
	case MMU_READ_WORD: tempData = *((Word*)(realAddr)); break;
	case MMU_READ_DWORD: tempData = *((Dword*)(realAddr)); break;

	case MMU_WRITE_BYTE: *((Byte*)(realAddr)) = (Byte) tempData; break;
	case MMU_WRITE_WORD: *((Word*)(realAddr)) = (Word) tempData; break;
	case MMU_WRITE_DWORD: *((Dword*)(realAddr)) = tempData; break;
	}
	return tempData;
}

