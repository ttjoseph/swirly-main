// Overlord.cpp: implementation of the Overlord class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <conio.h>
#endif
#include "Overlord.h"
#include "scramble.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

Overlord::Overlord(SHCpu *cpu)
{
	this->cpu = cpu;
	cpu->overlord = this;
}

Overlord::~Overlord()
{

}

void Overlord::handleEvents()
{
	int button;
	SDL_Event e;
	SDL_PollEvent(&e);
	switch(e.type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch(e.key.keysym.sym)
		{
		case SDLK_LEFT: button = BUTTON_LEFT; break;
		case SDLK_RIGHT: button = BUTTON_RIGHT; break;
		case SDLK_UP: button = BUTTON_UP; break;
		case SDLK_DOWN: button = BUTTON_DOWN; break;
		case SDLK_RETURN: button = BUTTON_START; break;
		default: button = 0; break;
		}
		cpu->maple->buttonState[button] = (e.type == SDL_KEYDOWN);
	}
}

// loads and descrambles a file which has been scrambled in the 1ST_READ.BIN style
void Overlord::loadAndDescramble(FILE *fp, int start, int len, SHCpu *cpu, Dword addr)
{
	char *tempFilename = "swirly-1stread.tmp";
	char *tempFilename2 = "swirly-1stread-2.tmp";
	
	unsigned char *buf = new unsigned char[len];
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
	copyFromHost(cpu, addr, (void*) buf, len);

	unlink(tempFilename);
	unlink(tempFilename2);
	delete buf;
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
		cpu->mmu->writeByte(addr+i, c);
		i++;
	}
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
			cpu->mmu->writeByte(addr+(blockSize*i)+j, buf[j]);
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

// someday we'll use this for something
char* Overlord::getNameForNumber(int n, NumberName list[])
{
	int i = 0;
	while(list[i].number != 0)
	{
		if(list[i].number == n)
			return list[i].name;
	}
	return "[Unknown]";
}

// This function gets a string while pseudo-concurrently processing SDL events.
// Sadly, it uses a spinlock to check for SDL events, which means it hogs the
// CPU.  I wish I knew of a better way to do this while still maintaining cross-
// platform compatibility.
void Overlord::getString(char *buf, int buflen)
{
	SDL_Event e;
	fflush(stdout);
	for(;;)
	{
		if(kbhit() != 0)
		{
			fgets(buf, buflen, stdin);
			return;
			// Not using the following code because Win32 likes to buffer
			// input streams and take an entire string even on a getchar/fgetc
			// which defeats the purpose of using fgetc/getchar in the first
			// place.  Even putting a setvbuf() call at program startup
			// to diable buffering on stdin won't fix it.
		/*	int c = fgetc(stdin); 
			switch(c)
			{
			case '\r':
				break;
			case '\n':
				buf[i] = '\0';
				return;
			default:
				if(i<(buflen-1))
					buf[i++] = c;
			}
		*/
		}
		else if(SDL_PollEvent(&e) != 0)
		{
			switch(e.type)
			{
			case SDL_ACTIVEEVENT:
				cpu->gpu->drawFrame();
				break;
			}
		}
 	}
}

// OS-specific routine - returns nonzero if a key has been hit but the char
// hasn't been read.
int Overlord::kbhit()
{
#ifndef _WIN32
	return 1;
#else
	return _kbhit();
#endif
}

Dword Overlord::switchEndian(Dword d)
{
	return
	((d & 0xff) << 24) |
	((d & 0xff00) << 8) |
	((d & 0xff0000) >> 8) |
	(d >> 24);
}

// extracts bits out of a dword
Dword Overlord::bits(Dword val, int hi, int lo)
{
	Dword ans = val << (31 - hi);
	ans = ans >> (31 - hi + lo);
	return ans;
}

// like memcpy, except it copies from host address space to SH address space
void Overlord::copyFromHost(SHCpu *cpu, Dword dest, void *src, Dword len)
{
	Byte *s = (Byte *) src;
	for(Dword i=0; i<len; i++)
		cpu->mmu->writeByte(dest+i, s[i]);
}

// loads an SREC format file
// FIXME: this sucks
bool Overlord::loadSrec(char *fname)
{
	FILE *fp = fopen(fname, "rb");
	if(fp == NULL)
	{
		printf("Couldn't open %s!\n", fname);
		return false;
	}
	char type[4], count[4];
	int numBytes;

	while(!feof(fp))
	{
		type[0] = 0;
		while(type[0] != 'S')
			fread(&type, 1, 2, fp);


		fread(&count, 1, 2, fp);
		type[2] = count[2] = 0; // null terminate
		//convertSrecBytes(count, (char*)&numBytes, 1);
		sscanf(count, "%02x", &numBytes);

		switch(type[1])
		{
		case '0':
			{
				//printf("..%d///", numBytes);
				numBytes -= 3;
				char *desc = new char[numBytes*2];
				fread(desc, 1, 4, fp); // throw out 4 bytes of address
				fread(desc, 1, numBytes*2, fp);
				convertSrecBytes(desc, desc, numBytes);
				// printf("SREC: %s\n", desc);
				delete desc;
				break;
			}

		case '3': // 4-byte address
			{
				numBytes -= 4;
				char addrStr[8];
				char *buf = new char[numBytes*2];
				Dword addr;
				fread(addrStr, 1, 8, fp);
				sscanf(addrStr, "%8x", &addr);
				fread(buf, 1, numBytes*2, fp);
				// we'll throw out the checksum
				numBytes--;
				convertSrecBytes(buf, buf, numBytes);
				copyFromHost(cpu, addr, buf, numBytes);

				break;
			}
		case '7': // execution start address
			return false; // break; // XXX: we don't care
		default:
			printf("Overlord::loadSrec: Unknown SREC block type S%c\n", type[1]);
			return false;

		}
	}
	return true;
}

// converts SREC-type byte strings into real bytes
// okay to specify same address as src and dest
void Overlord::convertSrecBytes(char *src, char *dest, int numBytes)
{
	Byte *tmp = new Byte[numBytes];
	int d, i;
	for(i=0; i<numBytes; i++)
	{
//		printf("%2s", src+(i*2));
		sscanf(src+(i*2), "%2x", &d);
		tmp[i] = (Byte)(d&0xff);
	}
	memcpy(dest, tmp, numBytes);
	dest[i] = 0;
	delete tmp;
}
