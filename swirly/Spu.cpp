#include "Spu.h"
#include "SHCpu.h"
#include <stdio.h>
#include <time.h>

Spu::Spu() {}

Spu::~Spu() {}

Dword Spu::hook(int eventType, Dword addr, Dword data)
{
	static bool flip = false;

	// RTC
	if (addr == 0x710000) {
		int tt = time(NULL) + TWENTY_YEARS;
		return ((tt>>16)&0xffff);
	} else if (addr == 0x710004) {
		int tt = time(NULL) + TWENTY_YEARS;
		return (tt & 0xffff);
	}

	flip = !flip;
	if(flip)
		return 0x3;
	else
		return 0xffffffff;
		
}
