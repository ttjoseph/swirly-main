#ifndef _SWIRLY_H_
#define _SWIRLY_H_

#define VERSION_STRING "Swirly beta"

extern class SHCpu *cpu;
extern class SHMmu *mmu;
extern class SHTmu *tmu;
extern class SHBsc *bsc;
extern class SHUbc *ubc;
extern class SHSci *sci;
extern class SHDmac *dmac;
extern class SHIntc *intc;

extern class Overlord *overlord;
extern class Debugger *debugger;
extern class Modem *modem;
extern class Gdrom *gdrom;
extern class Maple *maple;
extern class Gpu *gpu;
extern class Spu *spu;

typedef unsigned char Byte;  // 8 bits
typedef signed char Sbyte;   // 8 bits
typedef unsigned short Word; // 16 bits
typedef signed short Sword;  // 16 bits - chop chop
typedef unsigned int Dword;  // 32 bits
typedef signed int Sdword;   // 32 bits

typedef double Double; // just in case - should be 64 bits
typedef float Float;  // also just in case - should be 32 bits

// useful funcs
static inline Dword switchEndian(Dword d) {
    return ((d&0xff)<<24) | ((d&0xff00)<<8) | ((d&0xff0000)>>8) | (d>>24);
}

static inline Dword bits(Dword val, int hi, int lo) {
    Dword ans = val << (31 - hi);
    ans = ans >> (31 - hi + lo);
    return ans;
}

#endif

