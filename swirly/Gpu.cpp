#include <string.h>
#include "Gpu.h"
#include "Overlord.h"

#include <GL/gl.h>

#define RegDword(offs) (*((Dword*)(regs+offs)))
#define RegFloat(offs) (*((float*)(regs+offs)))

Gpu::Gpu(SHCpu *shcpu) : cpu(shcpu), screen(0), currBackBuffer(0), dwordsReceived(0)
{
	for(int i=0; i<GPU_MAXBACKBUFFERS; i++)
	{
		backBuffers[i] = 0;
		backBufferDCAddrs[i] = GPU_BACKBUFFERUNUSED;
	}
}

Gpu::~Gpu()
{
	for(int i=0; i<GPU_MAXBACKBUFFERS; i++)
	{
		if(backBuffers[i] != 0)
			SDL_FreeSurface(backBuffers[i]);
	}
	if(screen = 0)
		SDL_FreeSurface(screen);
}

void Gpu::recvStoreQueue(Dword *sq)
{
	cpu->debugger->print("Gpu::recvStoreQueue: received a store queue\n");
	for(int i=0; i<8; i++)
		handleTaWrite(0x10000000, sq[i]);
}

void Gpu::handleTaWrite(Dword addr, Dword data)
{
	Dword *recvBufDwords = (Dword*)recvBuf;
	float *recvFloat = (float*)recvBuf;
	recvBufDwords[dwordsReceived] = data;
	dwordsReceived++;
	bool withinList = false;

	cpu->debugger->print("Gpu::handleTaWrite: received a write to the TA, addr: %08x, data %08x\n", addr, data);
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
			cpu->debugger->print("TA: vertex (%.2f %.2f %.2f) RGBA: %.2f %.2f %.2f %.2f\n",
				recvFloat[1],
				recvFloat[2],
				recvFloat[3],
				recvFloat[5],
				recvFloat[6],
				recvFloat[7],
				recvFloat[4]
				);

			glColor4f(recvFloat[5], recvFloat[6], recvFloat[7], recvFloat[4]);
			glVertex3f(recvFloat[1], recvFloat[2], -1.0f * recvFloat[3]);
			break;
		case 0: // End of list
			cpu->debugger->print("TA: end of list\n");
			glEnd();
			withinList = false;
			break;
		case 1: // user clip
			cpu->debugger->print("TA: User clip is %f %f %f %f\n",
				recvBuf[4],
				recvBuf[5],
				recvBuf[6],
				recvBuf[7]);
			//cpu->debugger->print("hit a key");
			//getchar();
			break;
		case 4: // polygon / modifier volume
		//	cpu->debugger->print("TA: Polygon / modifier volume\n");
			break;
		default: // hmm...
			cpu->debugger->print("TA: *** unimplemented TA command\n");
		}
	}
}

void Gpu::updateFps()
{

}

void Gpu::drawFrame()
{
	if(taEnabled() || (screen == NULL) || (currBackBuffer == NULL))
		return;
	SDL_LockSurface(screen);
	memcpy(screen->pixels, currBackBuffer->pixels, currBackBuffer->pitch*currBackBuffer->h);
	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}

Dword Gpu::hook(int eventType, Dword addr, Dword data)
{
	Dword offs = addr & 0xfff;

	if(eventType & MMU_READ)
	{
		RegDword(GPU_REG_SYNC_STAT) ^= 0x1ff;
	}

	switch(eventType)
	{
	case MMU_WRITE_DWORD:
		RegDword(offs) = data;
		break;
	case MMU_READ_DWORD:
		return RegDword(offs);
	default: cpu->debugger->flamingDeath("Gpu::hook: can only handle dword accesses because I'm lazy");
	}

	if(eventType & MMU_WRITE)
		modeChange();
	return 0;
}

int Gpu::width()
{
	int tmp;
	tmp = Overlord::bits(RegDword(GPU_REG_DISPLAY_SIZE), 9, 0);
	tmp++;
	tmp *= 4;
	tmp /= (bpp() / 8);
	return tmp;
}

int Gpu::height()
{
	int tmp;
	tmp = Overlord::bits(RegDword(GPU_REG_DISPLAY_SIZE), 19, 10);
	tmp++;
	return tmp;
}

int Gpu::bpp()
{
	switch((RegDword(GPU_REG_FB_CFG1) & 0xf) >> 2)
	{
		case 0:
		case 1:
			return 16;
		case 3:
			return 32;
		default:
			cpu->debugger->flamingDeath("Gpu::bpp: Unknown bpp value in GPU_REG_FB_CFG1");
	}
}

void Gpu::masks(Dword *rm, Dword *gm, Dword *bm, Dword *am)
{
	switch((RegDword(GPU_REG_FB_CFG1) & 0xf) >> 2)
	{
	case 0:
		*bm = 0x1f;
		*gm = 0x1f << 5;
		*rm = 0x1f << 10;
		*am = 0;
		break;
	case 1: // RGB565
		*bm = 0x1f;
		*rm = 0x1f << 11;
		*gm = 0x3f << 5;
		*am = 0;
		break;
	case 3: // RGBA8888
		*rm = 0x00ff0000;
		*gm = 0x0000ff00;
		*bm = 0x000000ff;
		*am = 0xff000000;
		break;
	default:
		cpu->debugger->flamingDeath("Gpu::masks: Unknown values set in GPU_REG_FB_CFG1");
	}
}

int Gpu::pitch()
{
	Dword mod = Overlord::bits(RegDword(GPU_REG_DISPLAY_SIZE), 29, 20) - 1;
	return width() * (bpp() / 8) + (mod * 4);
}

Dword Gpu::baseAddr()
{
	return RegDword(GPU_REG_DISPLAY_ADDR_ODD) & 0x00fffffc;
}
// are we in a sane enough state to attempt a host graphics mode switch?
bool Gpu::sane()
{
	if((width() < 300) || (width() > 805))
		return false;

	if((height() < 200) || (height() > 610))
		return false;

	return true;
}

bool Gpu::taEnabled()
{
	return (RegDword(GPU_REG_VRAM_CFG3) == 0x15d1c951);
}

void Gpu::modeChange()
{
	if(!sane())
		return;

	// cpu->debugger->print("Gpu::modeChange: mode change to %dx%d, %dbpp\n", width(), height(), bpp());

	// Switch to 3D mode if necessary
	if(taEnabled())
	{
		if(screen != 0)
			SDL_FreeSurface(screen);

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		if((screen = SDL_SetVideoMode(width(), height(), bpp(), SDL_OPENGL | SDL_DOUBLEBUF)) == NULL)
			cpu->debugger->flamingDeath("Gpu::modeChange: Couldn't set OpenGL video mode to %dx%d, %dbpp!", width(), height(), bpp());

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		// XXX: what is going on here? why must we scale like this?
		// cpu->debugger->print("Gpu::modeChange: background plane is at %f\n", RegFloat(GPU_REG_BGPLANE_Z));
		glFrustum(0, width() / 10.0, 0, height() / 10.0, 1.0, RegFloat(GPU_REG_BGPLANE_Z));
		//gluOrtho2D(0, width+200, 0, height+100);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glClearColor(0.0, 0.0, 0.0, 0.0);
		glShadeModel(GL_SMOOTH);

		glViewport(0, 0, width(), height());
		SDL_WM_SetCaption(VERSION_STRING " - OpenGL", NULL);

		return;
	}

	// See if we have a back buffer already made that matches the current GPU settings
	for(int i=0; i<GPU_MAXBACKBUFFERS; i++)
	{
		if( // hooray for short-circuit evaluation of expressions
			(backBuffers[i] != 0) &&
			(backBufferDCAddrs[i] == baseAddr()) &&
			(backBuffers[i]->format->BitsPerPixel == bpp()) &&
			(backBuffers[i]->w == width()) &&
			(backBuffers[i]->h == height()) )
		{
			// cpu->debugger->print("Gpu::modeChange: reused back buffer for base addr %08x\n", baseAddr());
			currBackBuffer = backBuffers[i];
			break;
		}
	}

	// If we didn't have an already made back buffer, make one
	// and put it in the back buffers array
	if((currBackBuffer == 0) ||
		((currBackBuffer->w != width()) || (currBackBuffer->h != height()) || (currBackBuffer->format->BitsPerPixel != bpp())))
	{
		Dword rmask, gmask, bmask, amask;

		masks(&rmask, &gmask, &bmask, &amask);
		currBackBuffer = SDL_CreateRGBSurfaceFrom(
			(void*)(((Dword)cpu->mmu->videoMem)+baseAddr()),
			width(),
			height(),
			bpp(),
			pitch(),
			rmask,
			gmask,
			bmask,
			amask);

		int i;
		for(i=0; i<GPU_MAXBACKBUFFERS; i++)
		{
			if(backBuffers[i] == 0)
				break;
		}

		// TODO: make this kill one of the old back buffers to make room for the new one
		if(i >= GPU_MAXBACKBUFFERS)
			cpu->debugger->flamingDeath("Gpu::modeChange: too many back buffers allocated");

		backBuffers[i] = currBackBuffer;
		backBufferDCAddrs[i] = baseAddr();
	}

	if(screen != 0)
	{
		if((screen->w != width()) || (screen->h != height()) ||	(screen->format->BitsPerPixel != bpp()))
		{
			SDL_FreeSurface(screen);
			screen = 0;
		}
	}

	// If we don't have a primary screen made, make one
	if(screen == 0)
	{
		screen = SDL_SetVideoMode(width(), height(), bpp(), SDL_HWSURFACE);
		char caption[512];
		sprintf(caption, "%s - %dx%d %dbpp", VERSION_STRING, width(), height(), bpp());
		SDL_WM_SetCaption(caption, NULL);
	}

	if(currBackBuffer->format->BitsPerPixel != screen->format->BitsPerPixel)
		*((Dword*)0) = 0xdeadbeef;
}