#ifndef _OVERLORD_H_
#define _OVERLORD_H_

#include "swirly.h"
#include <stdio.h>

/*
    // *** examples on how to load dreamcast files from within swirly ***
    
    // to load the boot ROM:
    cpu->overlord->load("../fakebootrom/fbr.bin", 0xa0000000);
    
    // to load flash memory (nonessential):
    cpu->overlord->loadToHost("../images/flash", (Dword) cpu->mmu->flash);
    
    // to load a flat binary:
    cpu->overlord->load("../images/dreampac_unscrambled.bin",0x8c010000);
    
    // to load a SREC file:
    cpu->overlord->loadSrec("../images/stars.srec");
    
    // to load an ISO: 
    cpu->gdrom->startSector(0);
    cpu->gdrom->load("something.iso");
*/

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
    
    Overlord();
    virtual ~Overlord();
	
 private:
    void convertSrecBytes(char *src, char *dest, int numBytes);
};

#endif
