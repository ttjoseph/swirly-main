#ifndef _SHMMU_H_
#define _SHMMU_H_

#include "swirly.h"

// boot rom  -- 2 mb   repeated 
// flash rom -- 256 kb repeated
// video rom -- 8 mb   repeated
// sys ram   -- 16 mb  repeated
// tex mem   -- 8 mb   repeated
// sound mem -- 2 mb   repeated

#define MMU_CACHE_SIZE       (8*1024)
#define MMU_FLASH_SIZE       (256*1024)
#define MMU_BOOTROM_SIZE     (2*1024*1024)
#define MMU_SOUNDMEM_SIZE    (2*1024*1024)
#define MMU_TEXMEM_SIZE      (8*1024*1024)
#define MMU_VIDEOMEM_SIZE    (16*1024*1024) // XXX: this is wrong: we need to implement interleaved video 
#define MMU_DEFAULT_MEM_SIZE (16*1024*1024)

// values used internally for SHMmu::eventType
#define MMU_READ 0x0100
#define MMU_WRITE 0x0200
#define MMU_BYTE 1
#define MMU_WORD 2
#define MMU_DWORD 4
#define MMU_READ_BYTE (MMU_READ | MMU_BYTE)
#define MMU_READ_WORD	(MMU_READ | MMU_WORD)
#define MMU_READ_DWORD (MMU_READ | MMU_DWORD)
#define MMU_WRITE_BYTE (MMU_WRITE | MMU_BYTE)
#define MMU_WRITE_WORD (MMU_WRITE | MMU_WORD)
#define MMU_WRITE_DWORD (MMU_WRITE | MMU_DWORD)
// junk value to signal a TLB miss internally
#define MMU_TLB_MISS 0xffffff00

// addresses of memory-mapped registers (high byte zeroed)
#define PTEH 0
#define PTEL 4
#define TTB 8
#define TEA 0x0c
#define MMUCR 0x10
#define TRA 0x20
#define EXPEVT 0x24
#define INTEVT 0x28
#define PTEA 0x34

#define CCR 0x1c
#define QACR0 0x38
#define QACR1 0x3c

// helper macros
#define CCNREG(a) (*((Dword*)(cpu->ccnRegs+(a))))
#define GETWORD(a) access(a, MMU_READ_WORD)
#define GETDWORD(a) access(a, MMU_READ_DWORD)

class SHMmu
{
public:
	Dword access(Dword externalAddr, int accessType);
	void storeQueueSend(Dword target);
	void ldtlb();

	Float readFloat(Dword addr);
	Double readDouble(Dword addr);
	void writeFloat(Dword addr, Float d);
	void writeDouble(Dword addr, Double d);
        Word fetchInstruction(Dword addr);
	void writeDword(Dword addr, Dword d);
	void writeDwordToExternal(Dword addr, Dword d);
	void writeWord(Dword addr, Word d);
	void writeByte(Dword addr, Byte d);
	Dword readDword(Dword addr);
	Word readWord(Dword addr);
	Byte readByte(Dword addr);
	SHMmu();
	virtual ~SHMmu();

	Byte *mem, *cache, *flash, *videoMem, *bootRom, *soundMem, *texMem;
	class xMMUException {};

private:

	int searchItlb(Dword addr);
	void updateMmucrUrc();
	int searchUtlb(Dword addr);
	int checkAsids(Dword tlbentry);
	Dword translateVirtual(Dword addr);
	Dword accessP4();

	int eventType; // used internally to decide what exception to raise
	Dword tempData, accessAddr;

	// TLB
	Dword UTLB_Addr[64];
	Dword UTLB_Data1[64];
	Dword UTLB_Data2[64];
	Dword ITLB_Addr[4];
	Dword ITLB_Data1[4];
	Dword ITLB_Data2[4];

	// Store queues
	Dword SQ0[8], SQ1[8];
};

#endif
