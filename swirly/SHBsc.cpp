#include "SHBsc.h"
#include "SHMmu.h"
#include "Debugger.h"

SHBsc::SHBsc()
{
	regs[PCTRA] = regs[PCTRB] = 0;
}

SHBsc::~SHBsc()
{

}

Dword SHBsc::hook(int event, Dword addr, Dword data)
{
	addr &= 0xff;
	switch(event)
	{
	case MMU_READ_DWORD:
	case MMU_READ_WORD:
		update();
		return regs[addr];
	case MMU_WRITE_WORD:
		regs[addr] = (Word) data;
		update();
		return 0;
	case MMU_WRITE_DWORD:
		regs[addr] = data;
		update();
		return 0;
	default:
	    //debugger->print("Unknown SHBsc address %08x\n", addr);
		return 0;
	}
}

void SHBsc::update()
{
	// update PORT8 and PORT9 of PDTRA, which the DC video cable is hooked to
	// 0: VGA
	// 2: RGB
	// 3: composite
    static bool test = false; // HACK HACK to get dreamcast boot rom working

    if(test) {
	regs[PDTRA] = 0 << 8;
	test = false;
    } else {
	regs[PDTRA] = 0x3;
	test = true;
    }
}
