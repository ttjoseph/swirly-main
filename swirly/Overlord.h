// Overlord.h: interface for the Overlord class.
//
// This class is a place for me to put new features that I haven't
// really decided the proper place for yet, and also to act
// as what its name suggests.
//////////////////////////////////////////////////////////////////////

#ifndef _OVERLORD_H_
#define _OVERLORD_H_

#include "swirly.h"
#include "SHCpu.h"
#include "SHMmu.h"
#include "Gpu.h"

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_mutex.h>

class Overlord  
{
public:
	static void convertSrecBytes(char *src, char *dest, int numBytes);
	static void loadAndDescramble(FILE *fp, int start, int len, SHCpu *cpu, Dword addr);
	static void copyFromHost(SHCpu *cpu, Dword dest, void* src, Dword len);
	static Dword bits(Dword val, int hi, int lo);
	static void load(FILE *fp, int start, int len, SHCpu *cpu, Dword addr);
	static Dword switchEndian(Dword d);
	void getString(char *buf, int buflen);
	void handleEvents();
	
	struct NumberName
	{
		int number;
		char *name;
	};

	char* getNameForNumber(int n, NumberName list[]);
	void loadToHost(char *fname, Dword addr);
	void loadIso(char *fname, Dword addr);
	void load(char *fname, Dword addr);
	bool loadSrec(char *fname);
	Overlord(SHCpu *cpu);
	virtual ~Overlord();

	class SHCpu *cpu;
private:
	Overlord() {}
	int kbhit();
};

#endif
