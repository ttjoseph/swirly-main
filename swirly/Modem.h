#ifndef _SHMODEM_H_
#define _SHMODEM_H_

#include "swirly.h"
#include "SHCpu.h"

class Modem 
{

 public:
	Modem(class SHCpu *cpu);
	
	Dword hook(int eventType, Dword addr, Dword data);
	class SHCpu *cpu;
	
 private:
	
	Modem() {}
	
};

#endif
