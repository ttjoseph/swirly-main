// Gpu.h: interface for the Gpu class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _GPU_H_
#define _GPU_H_

#include "swirly.h"
#include "SHCpu.h"

#include <SDL/SDL.h>

#define MAXBACKBUFFERS 8
#define BACKBUFFERUNUSED 0xffffffff
class Gpu
{
public:
	void recvStoreQueue(Dword *sq);
	void handleTaWrite(Dword addr, Dword data);
	void updateFps();
	void drawFrame();
	Dword hook(int eventType, Dword addr, Dword data);
	Gpu(class SHCpu *shcpu);
	virtual ~Gpu();

	class SHCpu *cpu;


private:
	Gpu() {}

	void setupGL();
	void getFBSettings(Dword *w, Dword *h, Dword *vidbase, Dword *rm, Dword *gm, Dword *bm, Dword *am, Dword *bpp, Dword *mod, Dword *pitch);
	Dword accessReg(int operation, Dword addr, Dword data);
	void makeScreen();
	bool setHost2DVideoMode(int w, int h, int bpp);
	inline Dword switchEndian(Dword d);

	SDL_Surface *screen, *backBuffer, *backBuffers[MAXBACKBUFFERS];
	Dword backBufferDCAddrs[MAXBACKBUFFERS];
	int nextBackBuffer, currentBackBuffer;
	bool taEnabled; // true if TA is enabled

	// TA stuff
	int dwordsReceived, dwordsNeeded;
	float recvBuf[16];

};

#endif
