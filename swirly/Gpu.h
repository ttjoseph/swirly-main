#ifndef _GPU_H_
#define _GPU_H_

#include "swirly.h"
#include "SHCpu.h"

#include <SDL/SDL.h>

#define GPU_REG_ID 0
#define GPU_REG_REVISION 4
#define GPU_REG_RESET 8
#define GPU_REG_STARTRENDER 14
#define GPU_REG_OBJECTBUF_ADDR 20
#define GPU_REG_TILEBUF_ADDR 0x2c
#define GPU_REG_BORDER_COLOR 0x40
#define GPU_REG_FB_CFG1 0x44
#define GPU_REG_FB_CFG2 0x48
#define GPU_REG_RENDER_MODULO 0x4c
#define GPU_REG_DISPLAY_ADDR_ODD 0x50
#define GPU_REG_DISPLAY_ADDR_EVEN 0x54
#define GPU_REG_DISPLAY_SIZE 0x5c
#define GPU_REG_RENDER_ADDR 0x60
#define GPU_REG_PCLIP_X 0x68
#define GPU_REG_PCLIP_Y 0x6c
#define GPU_REG_SHADOW 0x74
#define GPU_REG_OBJECT_CLIP 0x78
#define GPU_REG_TSP_CLIP 0x84
#define GPU_REG_BGPLANE_Z 0x88
#define GPU_REG_BGPLANE_CFG 0x8c
#define GPU_REG_VRAM_CFG1 0xa0
#define GPU_REG_VRAM_CFG2 0xa4
#define GPU_REG_VRAM_CFG3 0xa8
#define GPU_REG_FOGTABLE_COLOR 0xb0
#define GPU_REG_FOGVERTEX_COLOR 0xb4
#define GPU_REG_FOG_DENSITY 0xb8
#define GPU_REG_RGB_CLAMP_MAX 0xbc
#define GPU_REG_RGB_CLAMP_MIN 0xc0
#define GPU_REG_GUN_POS 0xc4
#define GPU_REG_SYNC_CFG 0xd0
#define GPU_REG_HBORDER 0xd4
#define GPU_REG_SYNC_LOAD 0xd8
#define GPU_REG_VBORDER 0xdc
#define GPU_REG_SYNC_WIDTH 0xe0
#define GPU_REG_TSP_CFG 0xe4
#define GPU_REG_VIDEO_CFG 0xe8
#define GPU_REG_HPOS 0xec
#define GPU_REG_VPOS 0xf0
#define GPU_REG_SCALER_CFG 0xf4
#define GPU_REG_PALETTE_CFG 0x108
#define GPU_REG_SYNC_STAT 0x10c

#define GPU_MAXBACKBUFFERS 8
#define GPU_BACKBUFFERUNUSED 0xffffffff
class Gpu
{
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
	
	int  actListType;
	Byte regs[0x1000];
	SDL_Surface *screen, *currBackBuffer, *backBuffers[GPU_MAXBACKBUFFERS];
	Dword backBufferDCAddrs[GPU_MAXBACKBUFFERS], recvBuf[32];
	int dwordsReceived, dwordsNeeded;
};

#endif
