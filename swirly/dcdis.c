/* dcdis.c - very simple Dreamcast(tm) disassembler
 *
 * Copyright (C) 1999 Lars Olsson
 *
 * This file is part of dcdis
 *
 * dcdis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "dcdis.h"
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

#ifdef DO_SYMBOL
extern SYMTAB;
#endif

int standard_disp = 0;

u_dcshort
char2short(unsigned char *buf)
{

   u_dcshort buf2 = 0;
   int i;

   for (i = (N_O_BITS/8)-1; i >= 0; i--)
   {
      buf2 = buf2<<8;
      buf2 |= buf[i];
   }
   return (buf2);

}

u_dcint
char2int(unsigned char *buf)
{

   u_dcint buf2 = 0;
   int i;

   for (i = 3; i>= 0; i--)
   {
      buf2 = buf2<<8;
      buf2 |= buf[i];
   }
   return (buf2);
}

char
isAlpha(char c)
{

   if (c >= ' ' && c < 127)
   {
      return(c);
   }
   return ('.');
}

/*
void
usage()
{

   fprintf(stdout, "dcdis 0.3.3a (13-Aug-2000)\n");
   fprintf(stdout, "usage: dcdis [options] filename\n");
   fprintf(stdout, "Options:\n");
   fprintf(stdout, " -b<address>   binary file, text start\n");
   fprintf(stdout, " -o<filename>  file to write output to (default: stdout)\n");
#ifdef DO_SYMBOL
   fprintf(stdout, " -s<filename>  Katana MAP file\n");
#endif
   fprintf(stdout, " -d            standard displacement\n");
   fprintf(stdout, " filename      file to disassemble\n");
   exit(0);

}

int
main(argc, argv)
   int argc;
   char **argv;
{

   u_dcshort my_opcode;
   unsigned char buf[(N_O_BITS/8)];
   char *fname1 = NULL;
   FILE *s1 = NULL, *out;
#ifdef DO_SYMBOL
   FILE *sym;
   char *my_sym;
#endif
   int i, j;
   struct stat stat_buf;
   char *file;

   unsigned char binary_file = 0;
   u_dcint my_pc;
   u_dcint start_address = START_ADDRESS;

   out = stdout;
   for (i = 1; i < argc; i++)
   {
      if (argv[i][0] == '-')
      {
         switch (argv[i][1])
         {
            case 'b':
               binary_file = 1;
#ifdef HAVE_SSCANF
               if (sscanf(&argv[i][2], "%x", &start_address) == 0)
               {
                  usage();
               }
#else
	       fprintf(stderr, "Can't interpret text address on this system...using default 0x%x\n", START_ADDRESS);
#endif
               break;
            case 'o':
               if ((out = fopen(&argv[i][2], "w")) == NULL)
               {
                  perror("dcdis: open file");
                  exit(0);
               }
               break;
#ifdef DO_SYMBOL
	    case 's':
	       SYMTAB = 1;
	       if ((sym = fopen(&argv[i][2], "r")) == NULL)
	       {
		  perror("dcdis: symbol table file");
		  exit (0);
	       }

	       if (symtab_read(sym) == -1)
	       {
		  perror("dcdis: symbol table file");
		  exit (0);
	       }
	       break;
#endif

            case 'd':
               standard_disp = 1;
               break;

	    default:
               usage();
               break;
         }
      }
      else
      {
         fname1 = argv[i];
         if ((s1 = fopen(fname1, "r")) == NULL)
         {
            perror("dcdis: open file");
            exit(0);
         }

	 if (stat(argv[i], &stat_buf) != 0)
	 {
	    perror("dcdis: stat()");
	    exit(-1);
	 }

	 if (!(file = (char *)calloc(1, stat_buf.st_size)))
	 {
	    fprintf(stderr, "dcdis: File is too large!");
	    exit(-1);
	 }
      }
   }

   if (s1 == NULL)
   {
      usage();
   }

   fprintf(stdout, "Dreamcast(tm) Disassembler V0.3.3a\n");
   fprintf(stdout, "Code by Maiwe in 1999, 2000\n");

   my_pc = start_address;

   if (fread(file, stat_buf.st_size, 1, s1) == 0)
   {
      perror("dcdis: fread()");
      exit (-1);
   }

   fprintf(out, "\n");
   for (i = 0; i < stat_buf.st_size; i += N_O_BITS/8)
   {
#ifdef DO_SYMBOL
      if ((my_sym = (char *)symtab_lookup(my_pc)) != NULL)
      {
	 fprintf(out, "%s:\n", my_sym);
      }
#endif
      fprintf(out, "H'%08x: ", my_pc);

      my_opcode = char2short(&file[i]);
      fprintf(out, "H'%04x  ", my_opcode);

      for (j = 0; j < (N_O_BITS/8); j++)
      {
         fprintf(out, "%c", isAlpha(file[i+j]));
      }

      fprintf(out, "  %s\n", decode(my_opcode, my_pc, file, stat_buf.st_size, start_address));
      my_pc += N_O_BITS/8;
   }

}
*/