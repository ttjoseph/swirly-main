#ifndef _SHINTC_H_
#define _SHINTC_H_

#include "swirly.h"

enum int_source {TMU0, TMU1, TMU2, RTC, WDT, REF, SCI1, GPIO, DMAC, SCIF, UDI};

// exception codes
#define E_USER_BREAK_BEFORE_INSTRUCTION_EXECUTION 0x1e0
#define E_INSTRUCTION_ADDRESS_ERROR               0x0e0
#define E_INSTRUCTION_TLB_MISS                    0x040
#define E_INSTRUCTION_TLB_PROTECTION_VIOLATION    0x0a0
#define E_GENERAL_ILLEGAL_INSTRUCTION             0x180
#define E_SLOT_ILLEGAL_INSTRUCTION                0x1a0
#define E_GENERAL_FPU_DISABLE                     0x800
#define E_SLOT_FPU_DISABLE                        0x820
#define E_DATA_ADDRESS_ERROR_READ                 0x0e0
#define E_DATA_ADDRESS_ERROR_WRITE                0x100
#define E_DATA_TLB_MISS_READ                      0x040
#define E_DATA_TLB_MISS_WRITE                     0x060
#define E_DATA_TLB_PROTECTION_VIOLATION_READ      0x0a0
#define E_DATA_TLB_PROTECTION_VIOLATION_WRITE     0x0c0
#define E_FPU					  0x120
#define E_TRAP                                    0x160
#define E_INITAL_PAGE_WRITE			  0x080

// for asic9a only
#define RENDER_DONE 0x02
#define SCANINT1    0x03
#define SCANINT2    0x04
#define VBL         0x05
#define OPAQUE      0x07
#define OPAQUEMOD   0x08
#define TRANS       0x09
#define TRANSMOD    0x0a
#define MAPLE_DMA   0x0c
#define MAPLE_ERR   0x0d
#define GDROM_DMA   0x0e
#define SPU_DMA     0x0f
#define PVR_DMA     0x13
#define PUNCHTHRU   0x15

// for asic9b only
#define PRIM_NOMEM  0x02
#define MATR_NOMEM  0x03

// for asic9c only
#define GDROM_CMD   0x00
#define SPU_IRQ     0x01
#define EXP_8BIT    0x02
#define EXP_PCI     0x03

class SHIntc
{
public:
	void reset();
	Dword hook(int event, Dword addr, Dword data);
	SHIntc();

	// interrupts
	void checkInterrupt();
	void iterationsOverflow();
	void addInterrupt(Dword at, Dword type);

	void internalInt(int_source is, int ivt);
	void externalInt(int irqnr, int ivt);

	void externalInt9a(int newAsic);
	void printInterrupts();
	
	// exceptions
	void exception(Dword type, Dword addr, Dword data, char *datadesc = 0);
	char* getExceptionName(int exception);

	int exceptionsPending;

private:
	Word ICR, IPRA, IPRB, IPRC;
};

#endif
