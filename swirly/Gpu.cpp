#include <string.h>
#include "Gpu.h"
#include "Overlord.h"

#include <GL/gl.h>
#include <GL/glu.h>

#define RegDword(offs) (*((Dword*)(regs+offs)))
#define RegFloat(offs) (*((float*)(regs+offs)))

char *listtype[] = { "opaque polygon", 
		     "opaque modifier", 
		     "transparent polygon",
		     "transparent modifier", 
		     "punchthru polygon"};

char *colortype[] = { "32bit ARGB packed colour", 
		      "32bit * 4 floating point colour",
		      "intensity", 
		      "intensity from previous face"};

char *newcoltype[] = { "RGB555",
		       "RGB565",
		       "ARGB4444",
		       "ARGB1555",
		       "RGB888",
		       "ARGB888"};

GLenum depthType[] = { GL_NEVER, GL_GEQUAL, GL_EQUAL, GL_GREATER, GL_LEQUAL, GL_NOTEQUAL, GL_LESS, GL_ALWAYS};

Gpu::Gpu(SHCpu *shcpu) : 
    cpu(shcpu), screen(0), currBackBuffer(0), dwordsReceived(0) {
	
    for(int i=0; i<GPU_MAXBACKBUFFERS; i++) {
		backBuffers[i] = 0;
		backBufferDCAddrs[i] = GPU_BACKBUFFERUNUSED;
	}

    /* set default values in registers */
    RegDword(GPU_REG_ID) = 0x17fd11db;
    RegDword(GPU_REG_REVISION) = 0x11;
}

Gpu::~Gpu() {
    for(int i=0; i<GPU_MAXBACKBUFFERS; i++) {
		if(backBuffers[i] != 0)
			SDL_FreeSurface(backBuffers[i]);
	}
    if(screen==NULL) SDL_FreeSurface(screen);
}

void Gpu::recvStoreQueue(Dword *sq) {

    //cpu->debugger->print("Gpu::recvStoreQueue: received a store queue\n");
	for(int i=0; i<8; i++)
		handleTaWrite(0x10000000, sq[i]);
}

void Gpu::genTwiddledTexture(Word *texture, Dword address, int w, int h) {

// taken from KOS which was in turn taken from libdream
// basically shifts each bit by its position in the word... 
    
#define TWIDTAB(x) ( (x&1)|((x&2)<<1)|((x&4)<<2)|((x&8)<<3)|((x&16)<<4)| \
        ((x&32)<<5)|((x&64)<<6)|((x&128)<<7)|((x&256)<<8)|((x&512)<<9) )
#define TWIDOUT(x, y) ( TWIDTAB((y)) | (TWIDTAB((x)) << 1) )

    int depth = 2;
    
    int min = w > h ? h : w;
    int mask = min-1;
    int i;
    for(int y=0; y<h; y++) {
	i = y; // XXX: we may need to invert here
	for(int x=0; x<w; x++) {
	    texture[y*w+x] = cpu->mmu->readWord(address+depth*(TWIDOUT((x&mask), (i&mask)) + (x/min+i/min)*min*min));
	}
    }
}

void Gpu::textureMap(Dword address, Dword data) {

    // pvr reg e4 -- modulo width for textures
    // todo caching -- use objects? 
    // todo: check for texture format type

#define MAX_TEXTURES 8 // for now ... really an arbitrary limit

    static int texSizeList[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
    static GLuint tex[MAX_TEXTURES];
    static Dword texList[MAX_TEXTURES] = { 0 };
    static int currTexNum = 0;

    int w = texSizeList[data&0x38>>3];
    int h = texSizeList[data&0x7];

    if(tex[0]==0) glGenTextures(MAX_TEXTURES, tex);

    bool loaded = false;
    int index;
    for(index=0;index<MAX_TEXTURES; index++)
	if(texList[index]==address) {
	    glBindTexture(GL_TEXTURE_2D, tex[index]);
	    loaded = true;
	    break;
	}

    if(!loaded) {

	// generate the texture. 
	texList[currTexNum] = address;
	glBindTexture(GL_TEXTURE_2D, tex[currTexNum++]);
    
	printf("%d texture size: %d x %d, address %08x\n", 
	       currTexNum, w, h, address);

	// bleh. only one format supported right now.
	Word *texture = new Word[h*w];
	genTwiddledTexture(texture, address, w, h);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, 
		     GL_UNSIGNED_SHORT_5_6_5, texture);
    }

    
    bool s = (data>>16)&1;
    bool t = (data>>15)&1;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s ? GL_CLAMP:GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, s ? GL_CLAMP:GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);    
}


void Gpu::handleTaWrite(Dword addr, Dword data) {

    static bool withinList = false;
	Dword *recvBufDwords = (Dword*)recvBuf;
	float *recvFloat = (float*)recvBuf;

	recvBufDwords[dwordsReceived] = data;
	dwordsReceived++;

    //cpu->debugger->print("Gpu::handleTaWrite: received a write to the TA, addr: %08x, data %08x\n", addr, data);
    if(dwordsReceived == 1) {
		// see which command this is and decide how many dwords we need to receive
	switch(data >> 29) {
	case 0: // FALL THRU
		default:
			dwordsNeeded = 8;
		}
	}
    else if(dwordsReceived == dwordsNeeded) {
		dwordsReceived = 0;
	switch(recvBufDwords[0] >> 29) {
		case 7: // Vertex
	    if(withinList == false) {

		Dword base = 0xa5000000 | RegDword(0x4e);

		if((cpu->mmu->readDword(base)!=0x90800000) || 
		   (cpu->mmu->readDword(base+4) != 0x20800440)) {
		    printf("handleTaWrite: weird flags are %08x %08x\n", 
			   cpu->mmu->readDword(base), 
			   cpu->mmu->readDword(base+4));
		}

#if 0
		// we should do something with these. 
		// three points are given, usually corresponding to the
		// corners of the screen
		printf("%f %f %f\n", cpu->mmu->readFloat(base+12), 
		       cpu->mmu->readFloat(base+16), 
		       cpu->mmu->readFloat(base+20));
#endif

		Dword background = cpu->mmu->readDword(base+24);

		glClearColor((float)((background>>16)&0xff)/255, (float)((background>>8)&0xff)/255, (float)(background&0xff)/255, 1.0);

				glBegin(GL_TRIANGLE_STRIP);
				withinList = true;
			}
	    // vertex:  recvFloat 1 to 3
	    // texture: recvFloat 4 to 5
	    // color:   recvFloat 6

	    // if enabled??? 


			// XXX: support only for packed Color at the moment
	    //glColor4f(recvFloat[5], recvFloat[6], recvFloat[7], recvFloat[4]);
	    //printf("Red %f Green %f Blue %f Alpha %f\n", (float)((recvBuf[6]>>16)&0xff), (float)((recvBuf[6]>>8)&0xff),(float)(recvBuf[6]&0xff),(float)((recvBuf[6]>>24)&0xff));
			glColor4f((float)((recvBuf[6]>>16)&0xff), (float)((recvBuf[6]>>8)&0xff),(float)(recvBuf[6]&0xff),(float)((recvBuf[6]>>24)&0xff));


	    //printf("glVertex3f(%f, %f, %f);\n", recvFloat[1], height()-recvFloat[2], recvFloat[3]);

	    //printf("glTexCoord2f(%f, %f)\n", recvFloat[4], recvFloat[5]);
	    glTexCoord2f(recvFloat[4], recvFloat[5]);


	    //glVertex3f(recvFloat[1], 480.0 - recvFloat[2], recvFloat[3]);
	    //XXX shouldnt this be: ??? 
	    glVertex3f(recvFloat[1], height()-recvFloat[2], recvFloat[3]);
	    //glVertex3f(recvFloat[1], recvFloat[2], recvFloat[3]);

			if(recvBufDwords[0] & 0x10000000) {

				glEnd();
				withinList = false;
			}
			break;
		case 0: // End of list

			cpu->debugger->print("TA: end of list\n");
			switch (actListType) {
				case 0:	// opaque polygon
					cpu->maple->asicAckA |= (1 << 0x7); 
					break;
				case 1:	// opaque modifier
					cpu->maple->asicAckA |= (1 << 0x8); 
					break;
				case 2:	// transparent polygon
					cpu->maple->asicAckA |= (1 << 0x9); 
				    SDL_GL_SwapBuffers( );
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			        glLoadIdentity();
					break;
				case 3:	// transparent modifier
					cpu->maple->asicAckA |= (1 << 0xa); 
					break;
				case 4:	// punchthru polygon
					cpu->maple->asicAckA |= (1 << 0x15); 
					break;
			}
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
			actListType = (recvBuf[0]>>24)&0x7;
		
	        glDepthFunc(depthType[(recvBuf[1]>>29)&0x7]);

	    // textures.
	    if(recvBuf[0]&8) {
		glEnable(GL_TEXTURE_2D);
		Dword taddr = (recvBuf[3]<<3)&0x00fffff8;
		taddr |= 0xa4000000; // XXX: fixme
		textureMap(taddr, recvBuf[2]);
	    } else {
		glDisable(GL_TEXTURE_2D);
	    }

	    //cpu->debugger->print("TA: Polygon / modifier volume: Listtype: %s Colortype: %s (%s)\n", listtype[actListType],colortype[(recvBuf[0]>>4)&0x3],(recvBuf[0]&0x8)?"texture":"no texture");
	    //cpu->debugger->print("TA: depthType: %d\n", (recvBuf[1]>>29)&0x7);
			break;
		default: // hmm...
	    //cpu->debugger->print("TA: *** unimplemented TA command\n");
	    cpu->debugger->flamingDeath("TA: *** unimplemented TA command\n");
		}
	}
    if(screen==NULL) SDL_FreeSurface(screen);
}

void Gpu::updateFps() {

}

void Gpu::drawFrame()
{
	if(taEnabled() || (screen == NULL) || (currBackBuffer == NULL))
		return;

	SDL_LockSurface(screen);
    memcpy(screen->pixels, currBackBuffer->pixels, 
	                   currBackBuffer->pitch*currBackBuffer->h);
	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}

Dword Gpu::hook(int eventType, Dword addr, Dword data) {

    Dword offs = (addr>>2) & 0xfff;

    switch(eventType) {
    case MMU_READ_DWORD:
	cpu->debugger-printf("GPU::access: WARNING! read to reg %.3x\n", offs);

	RegDword(GPU_SYNC_STAT) ^= 0x1ff;
	if(offs<0x80)       return regs[offs];
	else if(offs<0x180) return (*((Dword*)(fog_table+offs)));
	else                return opl_table[offs];
	break;
	case MMU_WRITE_DWORD:
	//cpu->debugger->print("GPU::access: write to addr %04X, PC=%08X\n",
	//		     offs, cpu->PC);

	if(offs==0x14) cpu->maple->asicAckA |= 0x40; // render done

	if(offs<0x80)       regs[offs] = data;
	else if(offs<0x180) fog_table[offs-0x80] = (float)data;
	else                opl_table[offs-0x180] = data;
		break;
    default:
	cpu->debugger->flamingDeath("GPU::access: non-dword access!");
	}

    // when exactly should we do this?
    //if(offs==GPU_DISPLAY_SIZE || offs==GPU_DISPLAY_ENABLE) modeChange();
		modeChange();
	return 0;
}

int Gpu::width()
{
	int tmp;
	tmp = Overlord::bits(RegDword(GPU_DISPLAY_SIZE), 9, 0);
	tmp++; // only works for VGA
	tmp *= 4;
	tmp /= (bpp() / 8);
	return tmp;
}

int Gpu::height()
{
	int tmp;
	tmp = Overlord::bits(RegDword(GPU_DISPLAY_SIZE), 19, 10);
	tmp++; // only works for VGA
	return tmp;
}

int Gpu::bpp() {

    switch((RegDword(GPU_DISPLAY_ENABLE) & 0xf) >> 2) {
    case GPU_RGB555:
    case GPU_RGB565: return 16;
    case GPU_RGB888: return 32;
		default:
	cpu->debugger->flamingDeath("Gpu::bpp: Unknown bpp value in GPU_DISPLAY_ENABLE");
	}
}

void Gpu::masks(Dword *rm, Dword *gm, Dword *bm, Dword *am)
{
	switch((RegDword(GPU_DISPLAY_ENABLE) & 0xf) >> 2)
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
		cpu->debugger->flamingDeath("Gpu::masks: Unknown values set in GPU_DISPLAY_ENABLE");
	}
}

int Gpu::pitch() {
    Dword mod = Overlord::bits(RegDword(GPU_DISPLAY_SIZE), 29, 20) - 1;
	return width() * (bpp() / 8) + (mod * 4);
}

Dword Gpu::baseAddr() {
    return RegDword(GPU_DISPLAY_ADDR_ODD) & 0x00fffffc;
}

// are we in a sane enough state to attempt a host graphics mode switch?
bool Gpu::sane() {

    if((width() < 320) || (width() > 805))   return false;
    if((height() < 240) || (height() > 610)) return false;
	return true;
}

bool Gpu::taEnabled() {
    return (RegDword(GPU_VRAM_CFG3) == 0x15d1c951);
}

void Gpu::modeChange() {

    if(!sane()) return;

	cpu->debugger->print("Gpu::modeChange: mode change to %dx%d, %dbpp\n", width(), height(), bpp());

    if(width()%32) cpu->debugger->flamingDeath("Gpu::sane: width is not a multiple of 32");

	// Switch to 3D mode if necessary
    if(taEnabled()) {
	if(screen != 0) SDL_FreeSurface(screen);

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
	//glFrustum(0, width() / 10.0, 0, height() / 10.0, 1.0, RegFloat(0x22GPU_REG_BGPLANE_Z));
		gluOrtho2D(0, width(), 0, height());
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glClearColor(0.0, 0.0, 0.0, 0.0);
		glShadeModel(GL_SMOOTH);

        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);

		glViewport(0, 0, width(), height());
		SDL_WM_SetCaption(VERSION_STRING " - OpenGL", NULL);

		return;
	}

	// See if we have a back buffer already made that matches the current GPU settings
    for(int i=0; i<GPU_MAXBACKBUFFERS; i++) {
		if( // hooray for short-circuit evaluation of expressions
			(backBuffers[i] != 0) &&
			(backBufferDCAddrs[i] == baseAddr()) &&
			(backBuffers[i]->format->BitsPerPixel == bpp()) &&
			(backBuffers[i]->w == width()) &&
	   (backBuffers[i]->h == height()) ) {
			// cpu->debugger->print("Gpu::modeChange: reused back buffer for base addr %08x\n", baseAddr());
			currBackBuffer = backBuffers[i];
			break;
		}
	}

	// If we didn't have an already made back buffer, make one
	// and put it in the back buffers array
	if((currBackBuffer == 0) ||
       ((currBackBuffer->w != width()) || (currBackBuffer->h != height()) || (currBackBuffer->format->BitsPerPixel != bpp()))) {
		Dword rmask, gmask, bmask, amask;

		masks(&rmask, &gmask, &bmask, &amask);
		currBackBuffer = SDL_CreateRGBSurfaceFrom(
			(void*)(((Dword)cpu->mmu->videoMem)+baseAddr()),
			  width(), height(), bpp(),  pitch(),
			  rmask, gmask, bmask, amask);

		int i;
	for(i=0; i<GPU_MAXBACKBUFFERS; i++) {
			if(backBuffers[i] == 0)
				break;
		}

		// TODO: make this kill one of the old back buffers to make room for the new one
		if(i >= GPU_MAXBACKBUFFERS)
			cpu->debugger->flamingDeath("Gpu::modeChange: too many back buffers allocated");

		backBuffers[i] = currBackBuffer;
		backBufferDCAddrs[i] = baseAddr();
	}

    if(screen != 0) {
	if((screen->w != width()) || (screen->h != height()) ||	(screen->format->BitsPerPixel != bpp())) {
			SDL_FreeSurface(screen);
			screen = 0;
		}
	}

	// If we don't have a primary screen made, make one
    if(screen == 0) {
		screen = SDL_SetVideoMode(width(), height(), bpp(), SDL_HWSURFACE);
		char caption[512];
		sprintf(caption, "%s - %dx%d %dbpp", VERSION_STRING, width(), height(), bpp());
		SDL_WM_SetCaption(caption, NULL);
	}

	if(currBackBuffer->format->BitsPerPixel != screen->format->BitsPerPixel)
		*((Dword*)0) = 0xdeadbeef;
}
