#ifndef _SHUBC_H_
#define _SHUBC_H_

#include "swirly.h"

class SHUbc 
{

 public:
	
	Dword hook(int event, Dword addr, Dword data);
	SHUbc();
	virtual ~SHUbc();

	Dword bara, barb, bdrb, bdmrb;
	Word bbra, bbrb, brcr;
	Byte bamra, bamrb;


 private:

};

#endif
