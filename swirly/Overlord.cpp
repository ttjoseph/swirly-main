#ifdef _WIN32
#include <conio.h>
#endif
#include "Overlord.h"
#include "scramble.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "SHMmu.h"

Overlord::Overlord() {}

Overlord::~Overlord() {}

// loads and descrambles a file which has been scrambled in 1ST_READ.BIN style
void Overlord::loadAndDescramble(FILE *fp, int start, int len, SHCpu *cpu, Dword addr)
{
	
	Byte *buf = new Byte[len];
	
#if 0
	Dword sz = 0;
	load_file(fp, buf, sz);
	copyFromHost(cpu, addr, buf, len);
	unlink(tempFilename);
#else
	char *tempFilename = "swirly-1stread.tmp";
	char *tempFilename2 = "swirly-1stread-2.tmp";
	
	// copy the 1ST_READ.BIN to a temp file
	fseek(fp, start, SEEK_SET);
	fread(buf, 1, len, fp);
	FILE *tmpfp = fopen(tempFilename, "wb");
	fwrite(buf, 1, len, tmpfp);
	fclose(tmpfp);
	descramble(tempFilename, tempFilename2);
	tmpfp = fopen(tempFilename2, "rb");
	fread(buf, 1, len, tmpfp);
	fclose(tmpfp);
	copyFromHost(cpu, addr, buf, len);
	unlink(tempFilename);
	unlink(tempFilename2);
#endif
	delete [] buf;
}

// loads stuff from an open file to SH4 memory
void Overlord::load(FILE *fp, int start, int len, SHCpu *cpu, Dword addr)
{
    long i=0;
    char c;
    fseek(fp, start, SEEK_SET);
    while(i < len)
    {
	fread((void*)&c, 1, 1, fp);
	mmu->writeByte(addr+i, c);
	i++;
    }
}

struct elfSectionHeader {
    Dword unused0; 
    Dword type;
    Dword flags; 
    Dword addr;    
    Dword offset;
    Dword size;    
    Dword unused1;
    Dword unused2;
    Dword unused3;   
    Dword unused4;
};

bool Overlord::loadElf(char *fname) 
{
	
    FILE *fp;
	
    fp = fopen(fname, "rb");
    if(fp == NULL) 
    {
	printf("Can't open %s!\n", fname);
	return false;
    }
	
    // read in the ELF file header -- we just need to check a few words
    Word header[26];
    fread(header, 2, 26, fp);
	
    // verify that we indeed do have a correct ELF file on our hands
    if((header[0] != 0x457f) || (header[1] != 0x464c) || (header[2] != 0x0101))
    {
	printf("Error: file %s does not appear to be an ELF file.\n", fname);
	return false;
    }
	
    // get section header offset
    Dword *headerD = (Dword *)header;
    fseek(fp, headerD[8], SEEK_SET);
	
    // read in all the section headers
    Word shnum = header[24];
    Byte *secData = new Byte[shnum*sizeof(struct elfSectionHeader)];
    fread(secData, 1, shnum*sizeof(struct elfSectionHeader), fp);
    struct elfSectionHeader *secs = (struct elfSectionHeader *) secData;
	
    // load elf sections to their respective places in sh4 memory
    for(int i=0; i<shnum; i++) 
    {
	if(secs[i].flags&2) 
        { 
	    
	    Byte *buf = new Byte[secs[i].size];
	    
	    if(secs[i].type==8) 
	    { 
		memset(buf, 0, secs[i].size); 
	    } 
	    else 
	    {
		fseek(fp, secs[i].offset, SEEK_SET);
		fread(buf, 1, secs[i].size, fp);
	    }
	    copyFromHost(cpu, secs[i].addr, buf, secs[i].size);
	    
	    delete [] buf;
	}
    }
	
    delete [] secData;
    fclose(fp);
    
    return true;
}

// loads a raw image at the desired virtual address
void Overlord::load(char *fname, Dword addr)
{
    FILE *fp;
    long fsize;
	
    fp = fopen(fname, "rb");
    if(fp == NULL)
    {
	printf("Can't open %s!\n", fname);
	return;
    }
	
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp); // figure out how big the file is
    fseek(fp, 0, SEEK_SET);
    
    load(fp, 0, fsize, cpu, addr);
    
    fclose(fp);
}

// Loads an ISO mangled image at the desired virtual address
// This correctly loads ISO-type files that have 8 junk bytes
// before every 2048-byte block.  This won't load incomplete
// blocks.  So I'm lazy.
void Overlord::loadIso(char *fname, Dword addr)
{
    FILE *fp;
    // FILE *out;
    long fsize;
    int numblocks;
    char buf[2048];
    const int blockSize = 2048;
    
    fp = fopen(fname, "rb");
    if(fp == NULL)
    {
	printf("can't open %s! ", fname);
	return;
    }
    
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp); // figure out how big the file is
    fseek(fp, 0, SEEK_SET);
	
    numblocks = fsize / (2048 + 8);
	
    for(int i=0; i<numblocks; i++)
    {
	fread(buf, 1, 8, fp); // who cares about those 8 junk bytes?
	fread(buf, 1, blockSize, fp); // we'll keep this though
	
	// horribly inefficient--oh well
	for(int j=0; j<blockSize; j++)
	    mmu->writeByte(addr+(blockSize*i)+j, buf[j]);
    }
    
    fclose(fp);
}

// loads a raw image anywhere in our (not the SH's necessarily) memory
void Overlord::loadToHost(char *fname, Dword addr)
{
    FILE *fp;
    long fsize;
    char buf[2048];
    const int blockSize = 2048;
    
    fp = fopen(fname, "rb");
    if(fp == NULL)
    {
	printf("can't open %s! ", fname);
	return;
    }
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp); // figure out how big the file is
    fseek(fp, 0, SEEK_SET);
    
    long i=0;
    while(i < (fsize / blockSize))
    {
	fread((void*)buf, 1, blockSize, fp);
	memcpy((void*)(addr+blockSize*i), (void*)buf, blockSize);
	i++;
    }
    
    fclose(fp);
}

// like memcpy, except it copies from host address space to SH address space
void Overlord::copyFromHost(SHCpu *cpu, Dword dest, void *src, Dword len) 
{
    
    Dword *sd = (Dword *)src;
    for(Dword i=0; i<(len>>2); i++) 
	mmu->writeDword(dest+(i<<2), sd[i]);
	
    // write last few bytes if any
    if(len&3) 
    {
	Byte *sb = (Byte *)src;
	for(Dword i=1; i<=(len&3); i++) 
	    mmu->writeByte(dest+len-i, sb[len-i]);
    }
}

// loads an SREC format file
bool Overlord::loadSrec(char *fname) 
{
    
    FILE *fp = fopen(fname, "rb");
    if(fp == NULL) 
    {
	printf("Couldn't open %s!\n", fname);
	return false;
    }
    
    Word type;
    char count[4];
    int numBytes;
	
    while(!feof(fp)) 
    {
	type = 0;
	while((type&0xFF) != 'S')
	    fread(&type, 2, 1, fp);
	
	fread(&count, 1, 2, fp);
	count[2] = 0; // null terminate
	sscanf(count, "%02x", &numBytes);
	
	type >>= 8;
	switch(type) 
	{
	case '0': 
	    numBytes -= 3;
	    fseek(fp, numBytes*2+4, SEEK_CUR); 
	    break;
	case '3': // 4-byte address
	    {
	    numBytes -= 4;
	    
	    char addrStr[8];
	    Dword addr;
	    fread(addrStr, 1, 8, fp);
	    sscanf(addrStr, "%8x", &addr);
	    
	    char *buf = new char[numBytes*2];
	    fread(buf, 1, numBytes*2, fp);
	    
	    numBytes--; // throw out checksum
	    convertSrecBytes(buf, buf, numBytes);
	    copyFromHost(cpu, addr, buf, numBytes);
	    
	    delete [] buf;
	    break;
	    }
	case '7': // execution start address (not needed)
#if 0 
	    char addrStr[8];
	    Dword addr;
	    fread(addrStr, 1, 8, fp);
	    sscanf(addrStr, "%8x", &addr);
	    printf("SREC load address is %08x\n", addr);
#endif 
	    
	    fclose(fp);
	    return false; 
	default:
	    printf("Overlord::loadSrec: Unknown SREC block type S%c\n", type);
	    fclose(fp);
	    return false;
	}
    }
    
    fclose(fp);
    return true;
}

// converts SREC-type byte strings into real bytes (src == dst allowed)
void Overlord::convertSrecBytes(char *src, char *dest, int numBytes) 
{
    char x, y;
    for(int i=0; i<numBytes; i++) 
    {
	
	x = *(src+(i*2))   - '0';
	y = *(src+(i*2)+1) - '0';
	
	if(x>9) x-=7; 
	if(y>9) y-=7;
	
	dest[i] = (x<<4) | y;
    }
	
    dest[numBytes] = 0;
}
