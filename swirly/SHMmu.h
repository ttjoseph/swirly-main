// SHMmu.h: interface for the SHMmu class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _SHMMU_H_
#define _SHMMU_H_

#include "swirly.h"
#include "SHCpu.h"

#define MMU_DEFAULT_MEM_SIZE 16777216
#define MMU_CACHE_SIZE 8192
#define MMU_FLASH_SIZE 262144
#define MMU_VIDEOMEM_SIZE 0x800000
#define MMU_BOOTROM_SIZE (1048576 * 2)
#define MMU_SOUNDMEM_SIZE (1048576 * 2)

// values used internally for SHMmu::eventType
#define MMU_READ 0x0100
#define MMU_READ_BYTE 0x0110
#define MMU_READ_WORD	0x0111
#define MMU_READ_DWORD 0x0112
#define MMU_WRITE 0x0200
#define MMU_WRITE_BYTE 0x0213
#define MMU_WRITE_WORD 0x0214
#define MMU_WRITE_DWORD 0x0215
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
	void setSR(Dword d);
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
	SHMmu(class SHCpu *cpu);
	virtual ~SHMmu();

	class SHCpu *cpu;
	Byte *mem, *cache, *flash, *videoMem, *bootRom, *soundMem;

private:
  SHMmu() {}

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
