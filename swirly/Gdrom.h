// Gdrom.h: interface for the Gdrom class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _GDROM_H_
#define _GDROM_H_

#include <stdio.h>
#include "swirly.h"
#include "SHCpu.h"

enum
{
	GDROM_REQCMD = 0,
	GDROM_GETCMDSTAT = 1,
	GDROM_EXECSERVER = 2,
	GDROM_INITSYSTEM = 3,
	GDROM_GETDRVSTAT = 4,
	GDROM_G1DMAEND = 5,
	GDROM_REQDMATRANS = 6,
	GDROM_CHECKDMATRANS = 7,
	GDROM_READABORT = 8,
	GDROM_RESET = 9,
	GDROM_CHANGEDATATYPE = 10
};

enum
{
	GDROM_CMD_INIT = 24,
	GDROM_CMD_READTOC = 19,
	GDROM_CMD_READSECTORS = 16
};	
	
class Gdrom  
{
public:
	void load(char *fname);
	void hook();
	Gdrom(SHCpu *shcpu, int startsector, int sectorsize = 0);
	virtual ~Gdrom();
	int startSector, sectorSize;
	FILE *cdImage;

private:
	Gdrom() {};
	class SHCpu *cpu;

	// startSector is the physical sector number where the CD image logically starts

	struct Toc
	{
		Dword entry[99];
		Dword first, last;
		Dword whoKnows;
	};

};

#endif
