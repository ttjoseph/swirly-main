// Gpu.cpp: implementation of the DC graphics subsystem.

#ifdef _WIN32
#include <windows.h>
#endif
#include <signal.h>
#include "Gpu.h"
#include "SHMmu.h"

#include <GL/gl.h>
#include <GL/glu.h>

Gpu::Gpu(class SHCpu *shcpu) : cpu(shcpu)
{
	screen = NULL;
	backBuffer = NULL;
	taEnabled = false;
	nextBackBuffer = 0;
	dwordsReceived = 0;
	dwordsNeeded = 0;
	
	for(int i=0; i<8; i++)
	{
		backBuffers[i] = NULL;
		backBufferDCAddrs[i] = 0;
	}
	signal(SIGSEGV, SIG_DFL); // try and avoid the SDL parachute
}

Gpu::~Gpu()
{
	if(screen != NULL)
		SDL_FreeSurface(screen);
	if(backBuffer != NULL)
		SDL_FreeSurface(backBuffer);
}

Dword Gpu::hook(int eventType, Dword addr, Dword data)
{
	switch(eventType)
	{
	case MMU_READ_BYTE:
	case MMU_READ_WORD:
	case MMU_WRITE_BYTE:
	case MMU_WRITE_WORD:
		cpu->debugger->flamingDeath("GPU non dword access!\n");
		return 0;

	case MMU_READ_DWORD:
//		printf("GPU read %08X  data: %08x  PC: %08X\n", addr, data, cpu->PC);
		return accessReg(REG_READ, addr, data);

	case MMU_WRITE_DWORD:
//		printf("GPU write %08X  data: %08x  PC: %08x\n", addr, data, cpu->PC);
		accessReg(REG_WRITE, addr, data);
		return 0;
	}
	cpu->debugger->flamingDeath("Unknown event received in Gpu::hook");
	return 0;
}

Dword Gpu::accessReg(int operation, Dword addr, Dword data)
{
	static Dword gpuRegs[0xffff];
	static bool currentlyRetracing = true;
	Dword *realaddr;

	if((addr & 0xf0000000) == 0xe0000000) // write to TA through store queue
	{
		handleTaWrite(addr, data);
		return 0;
	}
	
	// first figure out where in the array we want to look
	if((addr & 0xffff0000) == 0xa05f0000)
		realaddr = gpuRegs;
	else
		return 0xdeadbeef; // return a junk value

	int regoffs = addr & 0xffff;

	switch(operation)
	{
	case REG_READ:
		switch(addr)
		{
		case 0xa05f810c: // another vertical retrace (VBL) reg
			currentlyRetracing = !currentlyRetracing; // XXX: this is a hack!
			if(currentlyRetracing)
				return 0x1ff;
			else
				return 0;
		case 0xa05f8000: // Machine version
			return 0x17fd11db; // we're a Set5 or real consumer machine
		default:
			return realaddr[regoffs];
		}

	case REG_WRITE:
		switch(addr)
		{
		case 0xa05f8014: // Flush TA pipeline - begin render
			// we defer flushing the OpenGL pipeline until we're about to swap buffers
//			printf("Gpu: Flush TA pipeline\n");
			break;

		case 0xa05f80a8: // magic TA register
			// we are now in 3D land...
			if(data == 0x15d1c951) // what on earth is this?
			{
				printf("Gpu::accessReg: TA enabled.\n");
				setupGL();
			}
		}
		realaddr[regoffs] = data;
		
		// don't create a new screen unless there's a write to a certain
		// arbitrary range of GPU registers
		if(((regoffs >= 0x8044) & (regoffs <= 0x805c)) || (regoffs == 0x80ec))
			makeScreen(); // XXX: there's got to be a better approach
		return 0;
	}

	cpu->debugger->flamingDeath("Got invalid operation in Gpu::accessReg");
	return 0;
}

// renders the current frame, as assembled in the DC framebuffer
// does not refresh the OpenGL frame...setupGL does that.
void Gpu::drawFrame()
{
	if(screen == NULL)
		return;
	if(!taEnabled)
	{
		if(backBuffer == NULL)
			return;
		SDL_LockSurface(screen);
		memcpy(screen->pixels, backBuffer->pixels, backBuffer->pitch*backBuffer->h);
		SDL_UnlockSurface(screen);
		SDL_Flip(screen);
	}
}

void Gpu::makeScreen()
{
	if(taEnabled)
	{
		setupGL();
		return;
	}

	Dword vidbase, rmask, gmask, amask, bmask, bpp, width, height, modulo, pitch;
	getFBSettings(&width, &height, &vidbase, &rmask, &gmask, &bmask, &amask,
		&bpp, &modulo, &pitch);

	// XXX: this is an arbitrary check
	// don't waste time creating a screen if the width is insanely low or high--
	// the program is probably not done initializing the GPU yet
	if((width < 32) || (width > 800))
		return;

	// if we're interlaced then we need to double the height value
	// and screw with different video base addresses...
	// XXX: for now we won't support interlaced mode.

	int bbNum = 0;
	while((bbNum < MAXBACKBUFFERS) && (backBufferDCAddrs[bbNum] != 0))
	{
		// XXX: do more extensive checking to make sure we are still in the same
		// video mode, etc.
		if((backBufferDCAddrs[bbNum] == vidbase) && 
			(backBuffers[bbNum]->format->BitsPerPixel == bpp))
		{
			printf("Gpu::makeScreen: Reused backbuffer %d.\n", bbNum);
			backBuffer = backBuffers[bbNum];
			currentBackBuffer = bbNum;
			return;
		}
		bbNum++;
	}
	if(bbNum >= MAXBACKBUFFERS)
		cpu->debugger->flamingDeath("Gpu::makeScreen: too many back buffers allocated!");

	printf("**** Gpu::makeScreen: RESETTING VIDEO MODE! ****\n");
	printf("New back buffer video base: %08X\n", vidbase);

	backBuffers[bbNum] = SDL_CreateRGBSurfaceFrom(
		(void*)(((Dword)cpu->mmu->videoMem)+vidbase),
		width,
		height,
		bpp,
		pitch,
		rmask,
		gmask,
		bmask,
		amask);
	backBuffer = backBuffers[bbNum];
	backBufferDCAddrs[bbNum] = vidbase;
	currentBackBuffer = bbNum;
	
	if(screen != NULL)
		SDL_FreeSurface(screen);

	screen = SDL_SetVideoMode(width, height, bpp, SDL_HWSURFACE);

	char caption[512];
	sprintf(caption, "%s - %dx%d %dbpp %s", VERSION_STRING, width, height, bpp, 
		taEnabled ? "OpenGL" : "");
	SDL_WM_SetCaption(caption, NULL);
	if(screen == NULL)
		return; // we used to return an error here...but no more
	drawFrame();
}

void Gpu::recvStoreQueue(Dword *sq)
{
#ifdef DANGEROUS_OPTIMIZE
	// this assumes that the SH program is sending things to TA in
	// aligned 8-dword increments
	memcpy(recvBuf, sq, 7*sizeof(Dword));
	dwordsReceived = 7;
	dwordsNeeded = 8;
	handleTaWrite(0x10000000, sq[7]);
#else
	for(int i=0; i<8; i++)
		handleTaWrite(0x10000000, sq[i]);
#endif
}

void Gpu::handleTaWrite(Dword addr, Dword data)
{
	Dword *recvBufDwords = (Dword*)recvBuf;
	recvBufDwords[dwordsReceived] = data;
	dwordsReceived++;
	bool withinList = false;

	if(dwordsReceived == 1)
	{
		// see which command this is and decide how many dwords we need to receive
		switch(data >> 29)
		{
		case 0:
			// FALL THRU
		default:
			dwordsNeeded = 8;
		}
	}
	else if(dwordsReceived == dwordsNeeded)
	{
		dwordsReceived = 0;
		switch(recvBufDwords[0] >> 29)
		{
		case 7: // Vertex
			if(withinList == false)
			{
				glBegin(GL_TRIANGLES);
				withinList = true;
			}
			withinList = true;
			/*printf("TA: vertex (%.2f %.2f %.2f) RGBA: %.2f %.2f %.2f %.2f\n", 
				recvBuf[1], 
				recvBuf[2], 
				recvBuf[3],
				recvBuf[5], 
				recvBuf[6], 
				recvBuf[7],
				recvBuf[4]
				);*/
		
			glColor4f(recvBuf[5], recvBuf[6], recvBuf[7], recvBuf[4]);
			glVertex3f(recvBuf[1], recvBuf[2], -1.0f * recvBuf[3]);
			break;
		case 0: // End of list
	//		printf("TA: end of list\n");
			glEnd();
			withinList = false;
			break;
		case 1: // user clip
			printf("TA: User clip is %f %f %f %f\n", 
				recvBuf[4], 
				recvBuf[5], 
				recvBuf[6], 
				recvBuf[7]);
			//printf("hit a key");
			//getchar();
			break;
		case 4: // polygon / modifier volume
		//	printf("TA: Polygon / modifier volume\n");
			break;
		default: // hmm...
			printf("TA: *** unimplemented TA command\n");
		}
	}
}

inline Dword Gpu::switchEndian(Dword d)
{
	return
		((d & 0xff) << 24) |
		((d & 0xff00) << 8) |
		((d & 0xff0000) >> 8) |
		(d >> 24);
}

void Gpu::getFBSettings(Dword *w, Dword *h, Dword *vidbase, Dword *rm, Dword *gm, Dword *bm, Dword *am, Dword *bpp, Dword *mod, Dword *pitch)
{
	*bpp = accessReg(REG_READ, 0xa05f8044, 0);
	if((*bpp & 1) == 0)
		return;
	*bpp = (*bpp & 0xf) >> 2;

	*vidbase = accessReg(REG_READ, 0xa05f8050, 0) & 0x00fffffc;

	switch(*bpp)
	{
	case 0: // RGB555
		*bm = 0x1f;
		*gm = 0x1f << 5;
		*rm = 0x1f << 10;
		*am = 0;
		*bpp = 16; 
		break;
	case 1: // RGB565
		*bm = 0x1f;
		*rm = 0x1f << 11;
		*gm = 0x3f << 5;
		*am = 0;
		*bpp = 16; 
		break;
	case 3: // RGBA8888
		*rm = 0x00ff0000;
		*gm = 0x0000ff00;
		*bm = 0x000000ff;
		*am = 0xff000000;
		*bpp = 32;
		break;
	default:
		return;
	}

	Dword tmp = accessReg(REG_READ, 0xa05f805c, 0);
	if(taEnabled == false)
	{
		*h = Overlord::bits(tmp, 19, 10) + 1;
		*w = Overlord::bits(tmp, 9, 0) + 1; // number of dwords per line
		*w <<= 2; // number of bytes per line
		*w /= (*bpp / 8);
	}
	else
	{
		*w = accessReg(REG_READ, 0xa05f8068, 0);
		*h = accessReg(REG_READ, 0xa05f806c, 0);
		*h = (*h >> 16) + 1;
		*w = (*w >> 16) + 1;
	}

	*mod = Overlord::bits(tmp, 29, 20) - 1;
	*pitch = *w * (*bpp / 8) + (*mod * 4);
}

// sets up GL properly for the beginning of a frame
// XXX: doesn't check for changes in resolution
void Gpu::setupGL()
{
	static Dword oldVidbase = 0xffffffff;
	Dword vidbase, rmask, gmask, amask, bmask, bpp, width, height, modulo, pitch;
	this->getFBSettings(&width, &height, &vidbase, &rmask, &gmask, &bmask, &amask,
		&bpp, &modulo, &pitch);

	// completely get rid of framebuffer-only mode.
	if(!taEnabled)
	{
		taEnabled = true;
		if(screen != NULL)
			SDL_FreeSurface(screen);
		for(int i=0; i<MAXBACKBUFFERS; i++)
		{
			if(this->backBuffers[i] != NULL)
				SDL_FreeSurface(backBuffers[i]);
		}
		backBuffer = NULL;

		// ...and jump into OpenGL!
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		if((screen = SDL_SetVideoMode(width, height, bpp, SDL_OPENGL | SDL_DOUBLEBUF)) == NULL)
			cpu->debugger->flamingDeath("Couldn't set OpenGL video mode!");

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		// XXX: what is going on here? why must we scale like this?
		glFrustum(0, width / 10.0, 0, height / 10.0, 1.0, 50.0);
		//gluOrtho2D(0, width+200, 0, height+100);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glClearColor(0.0, 0.0, 0.0, 0.0);
		glShadeModel(GL_SMOOTH);

		glViewport(0, 0, width, height);
		SDL_WM_SetCaption(VERSION_STRING " - OpenGL", NULL);
	} 
	else
	{
		if(oldVidbase != vidbase)
		{
			glFlush();
			SDL_GL_SwapBuffers();
			glClear(GL_COLOR_BUFFER_BIT);
			oldVidbase = vidbase;
			updateFps();
		}
	}

}

void Gpu::updateFps()
{
	static int frameCount = 0, timePassed = 1000;
	static char foo[256];

	if((frameCount % 120) == 0)
	{
		timePassed = SDL_GetTicks() - timePassed;
		float fps = (float) frameCount / (float) (timePassed / 1000.0f);
		sprintf(foo, VERSION_STRING " - OpenGL - %.0f fps", fps);
		SDL_WM_SetCaption(foo, NULL);
		timePassed = SDL_GetTicks();

		frameCount = 0;
	}

	frameCount++;
}
