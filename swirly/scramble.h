#ifndef _SCRAMBLE_H_
#define _SCRAMBLE_H_
// prototypes for Marcus' scramble.c to make it usable from C++
extern "C"
{
void load_file(FILE *fh, unsigned char *ptr, unsigned long filesz);
void descramble(char *src, char *dst);
}
#endif
