#ifndef _SHMODEM_H_
#define _SHMODEM_H_

#include "swirly.h"

class Modem 
{

 public:
	Modem();
	
	Dword hook(int eventType, Dword addr, Dword data);
	
 private:
	
};

#endif
