// Main header file for Swirly

#ifndef _SWIRLY_H_
#define _SWIRLY_H_

// #define VERSION_STRING "Swirly 1.0 - built on " __DATE__ " at " __TIME__ " - private version"
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

// i could have used hex here - would have been easier
#define D0 1
#define D1 2
#define D2 4
#define D3 8
#define D4 16
#define D5 32
#define D6 64
#define D7 128
#define D8 256
#define D9 512
#define D10 1024
#define D11 2048
#define D12 4096
#define D13 8192
#define D14 16384
#define D15 32768
#define D16 65536
#define D17 131072
#define D18 262144
#define D19 524288
#define D20 1048576
#define D21 2097152

#endif