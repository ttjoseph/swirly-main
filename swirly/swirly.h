#ifndef _SWIRLY_H_
#define _SWIRLY_H_
// Main header file for Swirly

#define VERSION_STRING "Swirly 1.0"

typedef unsigned char Byte;  // 8 bits
typedef signed char Sbyte;   // 8 bits
typedef unsigned short Word; // 16 bits
typedef signed short Sword;  // 16 bits - chop chop
typedef unsigned int Dword;  // 32 bits
typedef signed int Sdword;   // 32 bits

typedef double Double; // just in case - should be 64 bits
typedef float Float;  // also just in case - should be 32 bits

// used for accessReg() function for peripherals
#define REG_READ 1000
#define REG_WRITE 1001

// used for Swirly hooks
#define HOOK_GDROM 1
#define HOOK_LOAD1STREAD 2

// darn lack of binary notation in C++
#define D0  0x00000001
#define D1  0x00000002
#define D2  0x00000004
#define D3  0x00000008
#define D4  0x00000010
#define D5  0x00000020
#define D6  0x00000040
#define D7  0x00000080
#define D8  0x00000100
#define D9  0x00000200
#define D10 0x00000400
#define D11 0x00000800
#define D12 0x00001000
#define D13 0x00002000
#define D14 0x00004000
#define D15 0x00008000
#define D16 0x00010000
#define D17 0x00020000
#define D18 0x00040000
#define D19 0x00080000
#define D20 0x00100000
#define D21 0x00200000
#define D22 0x00400000
#define D23 0x00800000
#define D24 0x01000000
#define D25 0x02000000
#define D26 0x04000000
#define D27 0x08000000
#define D28 0x10000000
#define D29 0x20000000
#define D30 0x40000000
#define D31 0x80000000

#endif

