#ifndef _OVERLORD_H_
#define _OVERLORD_H_

#include "swirly.h"
#include "SHCpu.h"
#include "SHMmu.h"
#include "Gpu.h"

class Overlord 
{
	
 public:
	
	static void loadAndDescramble(FILE *fp, int start, int len, SHCpu *cpu, Dword addr);
	static void copyFromHost(SHCpu *cpu, Dword dest, void* src, Dword len);
	static void load(FILE *fp, int start, int len, SHCpu *cpu, Dword addr);
	
	void loadToHost(char *fname, Dword addr);
	
	void loadIso(char *fname, Dword addr);
	void load(char *fname, Dword addr);
	bool loadSrec(char *fname);
	bool loadElf(char *fname);
	
	Overlord(SHCpu *cpu);
	virtual ~Overlord();
	
	class SHCpu *cpu;
	
 private:
	void convertSrecBytes(char *src, char *dest, int numBytes);
	Overlord() {}
};

#endif
