#ifndef _GPU_H_
#define _GPU_H_

#include "swirly.h"
#include "SHCpu.h"

#include <SDL/SDL.h>

/* colors */
#define GPU_RGB555       0
#define GPU_RGB565       1
#define GPU_RGB888       3
/*
  0: ARGB1555 (2 bytes/pixel)
  1: RGB565 (2 bytes/pixel)
  2: RGB888 (3 bytes/pixel)
  3: ARGB8888 (4 bytes/pixel)
*/
/* 49 4b 4a 4c 4f 50 59 51 1a 1b */

/* 0xa05f8000 registers */
#define GPU_REG_ID            0x00
#define GPU_REG_REVISION      0x01
#define GPU_RESET             0x02 /* 0xffffffff: reset all, 0: reset none */
#define GPU_SPANSORT_CFG      0x0c /* ??? -- 0x101 */
#define GPU_BORDER_COLOR      0x10
#define GPU_DISPLAY_ENABLE    0x11 /* 0xf: colors | double and vga | enable */
#define GPU_LINESTRIDE        0x13
#define GPU_DISPLAY_ADDR_ODD  0x14
#define GPU_DISPLAY_ADDR_EVEN 0x15
#define GPU_DISPLAY_SIZE      0x17

#define GPU_VRAM_CFG3         0x2a /* alwyas has 0x15d1c851 */

#define GPU_FOG_TABLE_COLOR   0x2c /* table fog color */
#define GPU_FOG_VERTEX_COLOR  0x2d /* vertex fog color */
#define GPU_FOG_DENSITY       0x2e /* fog density coefficient */

#define GPU_UNK               0x3a /* 0x100 on? pixel doubling |= 8 ... */

#define GPU_VBLANK            0x33
#define GPU_SYNC_CFG          0x34 /* interlaced PAL:  0x190
				      interlaced NTSC: 0x150
				      non-interlaced:  0x100 */
#define GPU_BORDER_WINDOW1    0x35
#define GPU_SCANLINE_CLOCKS   0x36
#define GPU_BORDER_WINDOW2    0x37
#define GPU_SYNC_STAT         0x43 /* vertical refresh */

#define PVR_TA_VERT_BUF_POS   0x4e

/* magic values */


/* 30 b0 b4 b7 68 6c d0 d4 dc d8 e0 60 64 48 4c 50 54 5c c8 74 7c 80 78 84 88 8c bc c0 e4 f4 */


#define GPU_MAXBACKBUFFERS 8
#define GPU_BACKBUFFERUNUSED 0xffffffff

class Gpu {

public:
	void recvStoreQueue(Dword *sq);
	void handleTaWrite(Dword addr, Dword data);
	void updateFps();
	void drawFrame();
	Dword hook(int eventType, Dword addr, Dword data);
	Gpu(SHCpu *shcpu);
	virtual ~Gpu();

	SHCpu *cpu;

 private:
	bool sane();
	bool taEnabled();
	void modeChange();
	int width();
	int height();
	int bpp();
	int pitch();
	Dword baseAddr();
	void masks(Dword *rm, Dword *gm, Dword *bm, Dword *am);
    void textureMap(Dword address, Dword data);    
    void genTwiddledTexture(Word *texture, Dword address, int w, int h);
	int  actListType;

    Dword regs[0x80];       // 128 powervr regs
    Float fog_table[0x100]; // 128 indices, 128 floating point entries for fog
    Dword opl_table[0x280]; 
    /*
      a05f8000 - a05f81ff: regs      0x200
      a05f8200 - a05f85ff: fog_table 0x400
      a05f8600 - a05f8fff: opl_table 0xa00
    */
	SDL_Surface *screen, *currBackBuffer, *backBuffers[GPU_MAXBACKBUFFERS];
	Dword backBufferDCAddrs[GPU_MAXBACKBUFFERS], recvBuf[32];
	int dwordsReceived, dwordsNeeded;
};

#endif
