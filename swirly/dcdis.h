/* dcdis definitions
 *
 * Copyright (C) 1999 Lars Olsson
 *
 * This file is part of dcdis.
 *
 * dcdis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef _DCDIS_H
#define _DCDIS_H

#define N_O_BITS 16	/* SH-4 instructions are 16-bit fixed width */
#define START_ADDRESS	0x8c010000
#define SIZEOF_SHORT 2
#define SIZEOF_INT 4

#  if (SIZEOF_SHORT == 2)
typedef short dcshort;
typedef unsigned short u_dcshort;
#  else
#    if (SIZEOF_INT == 2)
typedef int dcshort;
typedef unsigned int u_dcshort;
#    endif
#  endif

#  if (SIZEOF_INT == 4)
typedef int dcint;
typedef unsigned int u_dcint;
#  else
#    if (SIZEOF_LONG == 4)
typedef long dcint;
typedef unsigned long u_dcint;
#    endif
#  endif

/* Only include symbol stuff if needed functions are available */
// #if defined (HAVE_STRSTR) && defined(HAVE_MEMCPY)
// #define DO_SYMBOL 1
// #endif

#ifdef __cplusplus
extern "C" {
#endif
char* decode(u_dcshort opcode, u_dcint cur_PC, char *file, int size, int start_address);
u_dcshort char2short(unsigned char *buf);
u_dcint char2int(unsigned char *buf);
#ifdef __cplusplus
}
#endif

#endif
