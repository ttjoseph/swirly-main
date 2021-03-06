#ifndef _SPU_H_
#define _SPU_H_

// This is supposed to be an implementation of the sound processor,
// which is basically an ARM7 core AFAIK.  At the moment there is
// no ARM emulation.

#include "swirly.h"

#define TWENTY_YEARS ((20 * 365LU + 5) * 86400)

class Spu  
{
public:
	Dword hook(int eventType, Dword addr, Dword data);
	Spu();
	virtual ~Spu();

private:
};

#endif
