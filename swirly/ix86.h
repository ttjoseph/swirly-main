/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __IX86_H__
#define __IX86_H__

/* general defines */

#define EAX 0
#define EBX 3
#define ECX 1
#define EDX 2
#define ESI 6
#define EDI 7
#define EBP 5
#define ESP 4

#define SIB 4
#define DISP32 5

#define write8(val)  *(unsigned char *)recPtr = val; recPtr++;
#define write16(val) *(unsigned short*)recPtr = val; recPtr+=2;
#define write32(val) *(unsigned long *)recPtr = val; recPtr+=4;

/* macros helpers */

#define SET8R(cc, to) { \
	write8(0x0F); write8(cc); write8((0xC0) | (to)); }

#define J8Rel(cc, to) { \
	write8(cc); write8(to); }

#define J32Rel(cc, to) { \
	write8(0x0F); write8(cc); write32(to); }

#define CMOV32RtoR(cc, to, from) { \
	write8(0x0F); write8(cc); write8((0xC0) | (from << 3) | (to)); }

#define CMOV32MtoR(cc, to, from) { \
	write8(0x0F); write8(cc); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* ix86 intructions */

// mov instructions

/* mov r32 to r32 */
#define MOV32RtoR(to, from) { \
	write8(0x89); write8((0xC0) | (from << 3) | (to)); }

/* mov r32 to m32 */
#define MOV32RtoM(to, from) { \
	write8(0x89); write8((0x00) | (from << 3) | (DISP32)); \
	write32(to); }

/* mov m32 to r32 */
#define MOV32MtoR(to, from) { \
	write8(0x8B); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* mov [r32] to r32 */
#define MOV32RmtoR(to, from) { \
	write8(0x8B); write8((0x00) | (to << 3) | (from)); }

/* mov disp8[r32] to r32 */
#define MOV32R8toR(to, from, disp8) { \
	write8(0x8B); write8((0x40) | (to << 3) | (from)); write8(disp8); }

/* mov r32 to disp8[r32] */
#define MOV32RtoR8(to, disp8, from) { \
	write8(0x89); write8((0x40) | (from << 3) | (to)); write8(disp8); }

/* mov imm32 to r32 */
#define MOV32ItoR(to, from) { \
	write8(0xB8 | to); write32(from); }

/* mov imm32 to m32 */
#define MOV32ItoM(to, from) { \
	write8(0xC7); write8((0x00) | (0 << 3) | (DISP32)); \
	write32(to); write32(from); }

/* cmovne r32 to r32*/
#define CMOVNE32RtoR(to, from) CMOV32RtoR(0x45, to, from)

/* cmovne m32 to r32*/
#define CMOVNE32MtoR(to, from) CMOV32MtoR(0x45, to, from)

/* cmove r32 to r32*/
#define CMOVE32RtoR(to, from) CMOV32RtoR(0x44, to, from)

/* cmove m32 to r32*/
#define CMOVE32MtoR(to, from) CMOV32MtoR(0x44, to, from)

/* cmovg r32 to r32*/
#define CMOVG32RtoR(to, from) CMOV32RtoR(0x4F, to, from)

/* cmovg m32 to r32*/
#define CMOVG32MtoR(to, from) CMOV32MtoR(0x4F, to, from)

/* cmovge r32 to r32*/
#define CMOVGE32RtoR(to, from) CMOV32RtoR(0x4D, to, from)

/* cmovge m32 to r32*/
#define CMOVGE32MtoR(to, from) CMOV32MtoR(0x4D, to, from)

/* cmovl r32 to r32*/
#define CMOVL32RtoR(to, from) CMOV32RtoR(0x4C, to, from)

/* cmovl m32 to r32*/
#define CMOVL32MtoR(to, from) CMOV32MtoR(0x4C, to, from)

/* cmovle r32 to r32*/
#define CMOVLE32RtoR(to, from) CMOV32RtoR(0x4E, to, from)

/* cmovle m32 to r32*/
#define CMOVLE32MtoR(to, from) CMOV32MtoR(0x4E, to, from)

// arithmic instructions

/* add imm32 to r32 */
#define ADD32ItoR(to, from) { \
	write8(0x81); write8((0xC0) | (0x0 << 3) | (to)); \
	write32(from); }

/* add imm32 to m32 */
#define ADD32ItoM(to, from) { \
	write8(0x81); write8((0x00) | (0x0 << 3) | (DISP32)); \
	write32(to); write32(from); }

/* add imm8 to disp8[r32] */
#define ADD32I8toR8(to, disp, from) { \
	write8(0x83); write8((0x40) | (0 << 3) | (to)); write8(disp); write8(from); }

/* add r32 to r32 */
#define ADD32RtoR(to, from) { \
	write8(0x01); write8((0xC0) | (from << 3) | (to)); }

/* add m32 to r32 */
#define ADD32MtoR(to, from) { \
	write8(0x03); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* sub imm32 to r32 */
#define SUB32ItoR(to, from) { \
	write8(0x81); write8((0xC0) | (0x5 << 3) | (to)); \
	write32(from); }

/* sub imm8 to disp8[r32] */
#define SUB32I8toR8(to, disp, from) { \
	write8(0x83); write8((0x40) | (5 << 3) | (to)); write8(disp); write8(from); }

/* sub r32 to r32 */
#define SUB32RtoR(to, from) { \
	write8(0x29); write8((0xC0) | (from << 3) | (to)); }

/* sub m32 to r32 */
#define SUB32MtoR(to, from) { \
	write8(0x2B); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* sbb r32 to r32 */
#define SBB32RtoR(to, from) { \
	write8(0x19); write8((0xC0) | (from << 3) | (to)); }

/* mul eax by r32 to edx:eax */
#define MUL32R(from) { \
	write8(0xF7); write8((0xC0) | (0x4 << 3) | (from)); }

/* imul eax by r32 to edx:eax */
#define IMUL32R(from) { \
	write8(0xF7); write8((0xC0) | (0x5 << 3) | (from)); }

/* mul eax by r32 to edx:eax */
#define MUL32M(from) { \
	write8(0xF7); write8((0x00) | (0x4 << 3) | (DISP32)); \
	write32(from); }

/* imul eax by r32 to edx:eax */
#define IMUL32M(from) { \
	write8(0xF7); write8((0x00) | (0x5 << 3) | (DISP32)); \
	write32(from); }

/* div eax by r32 to edx:eax */
#define DIV32R(from) { \
	write8(0xF7); write8((0xC0) | (0x6 << 3) | (from)); }

/* idiv eax by r32 to edx:eax */
#define IDIV32R(from) { \
	write8(0xF7); write8((0xC0) | (0x7 << 3) | (from)); }

/* div eax by m32 to edx:eax */
#define DIV32M(from) { \
	write8(0xF7); write8((0x00) | (0x6 << 3) | (DISP32)); \
	write32(from); }

/* idiv eax by m32 to edx:eax */
#define IDIV32M(from) { \
	write8(0xF7); write8((0x00) | (0x7 << 3) | (DISP32)); \
	write32(from); }

// shifting instructions

/* shl imm8 to r32 */
#define SHL32ItoR(to, from) { \
	write8(0xC1); write8((0xC0) | (0x4 << 3) | (to)); \
	write8(from); }

/* shl cl to r32 */
#define SHL32CLtoR(to) { \
	write8(0xD3); write8((0xC0) | (0x4 << 3) | (to)); }

/* shr imm8 to r32 */
#define SHR32ItoR(to, from) { \
	write8(0xC1); write8((0xC0) | (0x5 << 3) | (to)); \
	write8(from); }

/* shr cl to r32 */
#define SHR32CLtoR(to) { \
	write8(0xD3); write8((0xC0) | (0x5 << 3) | (to)); }

/* sal imm8 to r32 */
#define SAL32ItoR SHL32ItoR

/* sal cl to r32 */
#define SAL32CLtoR SHL32CLtoR

/* sar imm8 to r32 */
#define SAR32ItoR(to, from) { \
	write8(0xC1); write8((0xC0) | (0x7 << 3) | (to)); \
	write8(from); }

/* sar cl to r32 */
#define SAR32CLtoR(to) { \
	write8(0xD3); write8((0xC0) | (0x7 << 3) | (to)); }


// logical instructions

/* or imm32 to r32 */
#define OR32ItoR(to, from) { \
	write8(0x81); write8((0xC0) | (0x1 << 3) | (to)); \
	write32(from); }

/* or r32 to r32 */
#define OR32RtoR(to, from) { \
	write8(0x09); write8((0xC0) | (from << 3) | (to)); }

/* or r32 to disp8[r32] */
#define OR32RtoR8(to, disp8, from) { \
	write8(0x09); write8((0x40) | (from << 3) | (to)); write8(disp8); }

/* or m32 to r32 */
#define OR32MtoR(to, from) { \
	write8(0x0B); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* xor imm32 to r32 */
#define XOR32ItoR(to, from) { \
	write8(0x81); write8((0xC0) | (0x6 << 3) | (to)); \
	write32(from); }

/* xor r32 to r32 */
#define XOR32RtoR(to, from) { \
	write8(0x31); write8((0xC0) | (from << 3) | (to)); }

/* xor r32 to disp8[r32] */
#define XOR32RtoR8(to, disp8, from) { \
	write8(0x31); write8((0x40) | (from << 3) | (to)); write8(disp8); }

/* xor m32 to r32 */
#define XOR32MtoR(to, from) { \
	write8(0x33); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* and imm32 to r32 */
#define AND32ItoR(to, from) { \
	write8(0x81); write8((0xC0) | (0x4 << 3) | (to)); \
	write32(from); }

/* and r32 to r32 */
#define AND32RtoR(to, from) { \
	write8(0x21); write8((0xC0) | (from << 3) | (to)); }

/* and r32 to disp8[r32] */
#define AND32RtoR8(to, disp8, from) { \
	write8(0x21); write8((0x40) | (from << 3) | (to)); write8(disp8); }

/* and m32 to r32 */
#define AND32MtoR(to, from) { \
	write8(0x23); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* not r32 */
#define NOT32R(from) { \
	write8(0xF7); write8((0xC0) | (0x2 << 3) | (from)); }

/* neg r32 */
#define NEG32R(from) { \
	write8(0xF7); write8((0xC0) | (0x3 << 3) | (from)); }

// jump instructions

/* jmp rel32 */
#define JMP32(to) { \
	write8(0xE9); write32(to); }

/* jmp r32 */
#define JMP32R(to) { \
	write8(0xFF); write8((0xC0) | (0x4 << 3) | (to)); }

/* je rel8 */
#define JE8(to) { J8Rel(0x74, to); }

/* je rel32 */
#define JE32(to) { J32Rel(0x84, to); }

/* jz rel8 */
#define JZ8(to) { J8Rel(0x74, to); }

/* jne rel8 */
#define JNE8(to) { J8Rel(0x75, to); }

/* jnz rel8 */
#define JNZ8(to) { J8Rel(0x75, to); }

/* call rel32 */
#define CALL32(to) { \
	write8(0xE8); write32(to); }

/* call r32 */
#define CALL32R(to) { \
	write8(0xFF); write8((0xC0) | (0x2 << 3) | (to)); }

// misc instructions

/* cmp imm32 to r32 */
#define CMP32ItoR(to, from) { \
	write8(0x81); write8((0xC0) | (0x7 << 3) | (to)); \
	write32(from); }

/* cmp imm32 to m32 */
#define CMP32ItoM(to, from) { \
	write8(0x81); write8((0x00) | (0x7 << 3) | (DISP32)); \
	write32(to); write32(from); }

/* cmp r32 to r32 */
#define CMP32RtoR(to, from) { \
	write8(0x39); write8((0xC0) | (from << 3) | (to)); }

/* cmp m32 to r32 */
#define CMP32MtoR(to, from) { \
	write8(0x3B); write8((0x00) | (to << 3) | (DISP32)); \
	write32(from); }

/* setl r8 */
#define SETL8R(to) SET8R(0x9C, to);

/* cbw */
#define CBW() { \
	write8(0x66); write8(0x98); }

/* cwd */
#define CWD() { \
	write8(0x98); }

/* cdq */
#define CDQ() { \
	write8(0x99); }

/* push r32 */
#define PUSH32R(from) { \
	write8(0x50 | from); }

/* push m32 */
#define PUSH32M(from) { \
	write8(0xFF); write8((0x00) | (0x6 << 3) | (DISP32)); \
	write32(from); }

/* push imm32 */
#define PUSH32I(from) { \
	write8(0x68); write32(from); }

/* pop r32 */
#define POP32R(from) { \
	write8(0x58 | from); }

/* pushad */
#define PUSHA32() { \
	write8(0x60); }

/* popad */
#define POPA32() { \
	write8(0x61); }

/* ret */
#define RET() { \
	write8(0xC3); }

#endif /* __IX86_H__ */
