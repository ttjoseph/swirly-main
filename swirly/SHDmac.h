// header for SHDmac, implementation of SH4 DMAC

#ifndef _SHDMAC_H_
#define _SHDMAC_H_

#include "swirly.h"

class SHDmac
{
public:
        void startGdromDma(Dword dest, Dword count, Dword repeat);
        void startDma(Dword dest, Dword count, Dword repeat);
	Dword hook(int event, Dword addr, Dword data);
	SHDmac();
	~SHDmac();
	

	int transSize;     // 0: 64 bit, 1: byte, 2: word, 3: long, 4: 32 byte
	Dword dmaSrcAddr;  // dma source address 
	Dword dmaDestAddr; // dma destination address 
	Dword dmaCount;    // count in size units

	Dword dmaor;
private:
};

#endif
