#include <sys/time.h> // FPS
#include <string.h>

#include "Gpu.h"

// static arrays to simplify parameters for pvr/opengl commands

char *listType[] = { "opaque polygon", 
		     "opaque modifier", 
		     "transparent polygon",
		     "transparent modifier", 
		     "punchthru polygon"};

char *colortype[] = { "32bit ARGB packed colour", 
		      "32bit * 4 floating point colour",
		      "intensity", 
		      "intensity from previous face"};

char *textureFormat[] = { "ARGB1555",
			  "RGB565",
			  "ARGB4444",
			  "YUV422",
			  "BUMP",
			  "4BPP_PALETTE",
			  "8BPP_PALETTE" };

#if 1
GLenum depthType[] = { GL_NEVER, GL_GEQUAL, GL_EQUAL, GL_GREATER,
                       GL_LEQUAL, GL_NOTEQUAL, GL_LESS, GL_ALWAYS};
#else
GLenum depthType[] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL,
                       GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
#endif

GLenum blendFunc[] = { GL_ZERO, GL_ONE, GL_DST_COLOR, 
		       GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
		       GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
		       GL_ONE_MINUS_DST_ALPHA };

GLint texParam[] = { GL_REPEAT, GL_CLAMP };

GLfloat texEnv[] = { GL_REPLACE, GL_MODULATE, GL_DECAL, GL_MODULATE };
                    // XXX: GL_MODULATEALPHA (C = Cs * Ct, A = As * At) 

int texSizeList[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
	
int actListData[] = { 1<<0x7, 1<<0x8, 1<<0x9, 1<<0xa, 1<<0x15 };

Gpu::Gpu(SHCpu *shcpu) : 
    cpu(shcpu), screen(0), currBackBuffer(0), dwordsReceived(0) 
{

	for(int i=0; i<GPU_MAXBACKBUFFERS; i++) 
        {
		backBuffers[i] = 0;
		backBufferDCAddrs[i] = GPU_BACKBUFFERUNUSED;
	}

	fogTable = (Float *)taRegs+0x100;
	oplTable = taRegs+0x280;

	// default list type values
	lastOPBConfig = 0x00000000;
	lastType = 0;
	
	// set default values in registers
	taRegs[GPU_REG_ID] = 0x17fd11db;
	taRegs[GPU_REG_REVISION] = 0x11;
}

Gpu::~Gpu() 
{
	for(int i=0; i<GPU_MAXBACKBUFFERS; i++) 
	{
		if(backBuffers[i] != 0)
			SDL_FreeSurface(backBuffers[i]);
	}
	if(screen==NULL) SDL_FreeSurface(screen);
}

void Gpu::twiddleTexture(Word *texture, Dword address, int w, int h) 
{

	// code adapted from KOS which in turn was adapted from libdream
	
#define TWIDDLE(x) (x&0x01|((x&0x02)<<1)|((x&0x04)<<2)|((x&0x08)<<3)|((x&0x10)<<4) | \
             ((x&0x20)<<5)|((x&0x40)<<6)|((x&0x80)<<7)|((x&0x100)<<8)|((x&0x200)<<9))

	Dword texAddr;
	int min = w > h ? h : w;
	int mask = min-1;
	int i, ytwid, square = 0;

	bool invert = false; // we don't invert textures right now

	if(w != h) square = min*min;

	for(int y=0; y<h; y++) 
        {
		i = invert ? (h-1)-y : y; 
		ytwid = TWIDDLE((y&mask));
		for(int x=0; x<w; x++) 
                {
			texAddr = (TWIDDLE((x&mask)) << 1) | ytwid;
			if(x>min) texAddr += (x/min)*square;
			if(i>min) texAddr += (i/min)*square;
			
			texAddr <<= 1;

			texture[y*w+x] = *(Word *)(cpu->mmu->videoMem+address+texAddr);
		}
	}
}

void Gpu::twiddleTexture(Byte *texture, Dword address, int w, int h) 
{

	Dword texAddr;
	int min = w > h ? h : w;
	int mask = min-1;
	int ytwid, square = 0;

	if(w != h) square = min*min;

	for(int y=0; y<h; y++) 
        {
		ytwid = TWIDDLE((y&mask));
		for(int x=0; x<w; x++) 
                {
			texAddr = (TWIDDLE((x&mask)) << 1) | ytwid;
			if(x>min) texAddr += (x/min)*square;
			if(y>min) texAddr += (y/min)*square;
			
			texAddr <<= 1;

			texture[y*w+x] = *(Byte *)(cpu->mmu->videoMem+address+texAddr);
		}
	}
}

void Gpu::decompressVQ(Word *texture, Dword address, int w, int h) 
{
	
#define BUGGY_VQ 0
#if BUGGY_VQ
	Word *codebook = (Word *)(cpu->mmu->videoMem+address); // 256*4 word entries

        Byte *buf = new Byte[w*h/4];

	// XXX: we need a correct twiddling algo
	twiddleTexture(buf, address+2048, w/2, h/2); 

#if 0
	printf("twiddled texture follows:\n");
	Word *wb = (Word *)buf;
	for(int i=0; i<w*h/8; i++) {
		if((i%8)==0) printf("\n");
		printf("%04x ", wb[i]);
	}
#endif

	int x = 0;
	for(int i=0; i<w*h/4; i++) {
		Byte index = buf[i]<<2; 

		texture[x++] = codebook[index+0];
		texture[x++] = codebook[index+2]; 
		texture[x++] = codebook[index+1]; 
		texture[x++] = codebook[index+3]; 
	}
	
	delete [] buf;

        Word *tex = new Word[w*h];

	// shuffle around data -- flip and rotate the texture
        for(int i=0; i<w; i++) {
                for(int j=0; j<h; j++)
                        tex[i*w+j] = texture[(i)*w+(h-j-1)];
        }

        for(int i=0; i<w; i++) {
                for(int j=0; j<h; j++) {
                        texture[j*h+i] = tex[(i)*w+(h-j-1)];
                }
        }

	delete [] tex;
#else
	printf("VQ compression disabled. Enable Gpu::decompressVQ if you want visible results.\n");
#endif
} 

// used for converting from ARGB --> RGBA
void Gpu::rotateTextureData(Word *texture, int size, Byte shift) 
{
	for(int i=0; i<size; i++) 
        {
		Byte temp = texture[i]>>(16-shift);
		texture[i] <<= shift;
		texture[i] |= temp;
	}
}

void Gpu::textureMap(Dword data2, Dword data3) 
{

	// XXX: pvr reg 0x039 -- modulo width for textures
	
	// XXX: move this to class definition
#define MAX_TEXTURES 28 // for now ... really an arbitrary limit

	static GLuint tex[MAX_TEXTURES];
	static Dword texList[MAX_TEXTURES] = { 0 };
	static int currTexNum = 0;
	
	Dword address = (data3<<3)&0x00fffff8;

	int w = texSizeList[data2&0x38>>3];
	int h = texSizeList[data2&0x7];
	
	if(tex[0]==0) glGenTextures(MAX_TEXTURES, tex);
	
	bool loaded = false;
	int index;
	for(index=0;index<MAX_TEXTURES; index++)
		if(texList[index]==address) 
                {
			glBindTexture(GL_TEXTURE_2D, tex[index]);
			loaded = true;
			break;
		}
	
	if(!loaded) 
	{
			
		if(currTexNum >= MAX_TEXTURES)
			cpu->debugger->flamingDeath("Too many textures loaded");
		
		bool vq          = (data3>>30)&1;
		int format       = (data3>>27)&3;
		bool nontwiddled = (data3>>26)&1;
		
		bool stride      = (data3>>21)&1; // not supported
		bool mipmap      = (data3>>31)&1; // not supported

#if 0
		int mipmapBias   = (data2>>8)&15; // not supported
#endif	
	
		// XXX: add this as a debugger command ("list textures")
		printf("texture #%d data: %s, %dx%d, address %08x\n", 
		       currTexNum, textureFormat[format], w, h, address);
		printf("texture #%d options: %s%s%s\n", currTexNum, 
		       nontwiddled ? "" : "twiddled ",
		       vq ? "VQ-compressed " : "",
		       stride ? "stride " : "");
		
		if(stride) cpu->debugger->flamingDeath("Stride not supported");
		if(mipmap) cpu->debugger->flamingDeath("Mipmap not supported");
		
		// generate the texture. 
		texList[currTexNum] = address;
		glBindTexture(GL_TEXTURE_2D, tex[currTexNum++]);
		
		Word *texture = new Word[w*h];
		
		if(nontwiddled) 
                {
			if(vq) cpu->debugger->flamingDeath("nontwiddled+VQ not supported");
			else   memcpy(texture, cpu->mmu->videoMem+address, w*h*2);
		} 
		else 
                {
			if(vq) decompressVQ(texture, address, w, h);
			else   twiddleTexture(texture, address, w, h);
		}
	    
		switch(format) 
		{
		case 0: // ARGB1555 (converted to RGBA5551)
			rotateTextureData(texture, w*h, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, 
				     GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, texture);
			break;
		case 1: // RGB565
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, 
				     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, texture);
			break;
		case 2: // ARGB4444 (converted to RGBA4444)
			rotateTextureData(texture, w*h, 4);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, 
				     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, texture);
			break;
		case 3: // YUV422
		case 4: // BUMP
		case 5: // 4BPP_PALETTE
		case 6: // 8BPP_PALETTE
		default:
			cpu->debugger->flamingDeath("Texture format not supported");
		}
	
		delete [] texture;
	}
	
#if 0
	bool flipV = (data2>>17)&1; // not supported
	bool flipU = (data2>>18)&1; // not supported
	bool trans = (data2>>19)&1; // not supported
	bool alpha = (data2>>20)&1; // not supported
#endif

	// clamp U or V textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texParam[(data2>>15)&1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texParam[(data2>>16)&1]);
	
	// texture environment parameters
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnv[(data2>>6)&3]);
	
	// XXX: this filter code is crap. we need bilinear and trilinear support ... 
	if((data2>>12)&7) 
        {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else 
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
}

void Gpu::setOptions(Dword data0, Dword data1, Dword data2) 
{
#if 0
	// 0: striplength 1, 1: striplength 2, 2: striplength 4, 3: striplength 6
	// dunno how old this information is. disabled for now. 
	// XXX: check pvrmark, seems to trigger this.
       	int stripLength = (data0>>18)&3;
	if(stripLength) cpu->debugger->flamingDeath("Different strip length %d set", stripLength);

	// XXX: does not work
	// clipmode: 0 == disable, 2 == inside, 3 == outside
	clipMode = (data0>>16)&3; 
#endif

	// XXX: maybe use glColorMask for this?
	if((data2>>20)&1) glEnable(GL_ALPHA_TEST);
	else             glDisable(GL_ALPHA_TEST);

	glDepthFunc(depthType[(data1>>29)&0x7]);
	//glShadeModel((recvBuf[0]>>1)&1 ? GL_FLAT : GL_SMOOTH);

	// enable or disable depth buffer writing (note: 0 == enabled, 1 == disabled)
	if((data1>>26)&1) glDepthMask(GL_FALSE);
	else              glDepthMask(GL_TRUE);

#if 0	
	bool colorClamp = (data2>>21)&1;  // not supported
	
	bool modifier = (data0>>7)&1;     // not supported
	bool modifierMode = (data0>>6)&1; // not supported
	
	int fogMode = (data2>>22)&3;  // 0: table 1: vertex 2: disable 3:table2
#endif	

	// culling options
	int culling = (data1>>27)&3;
	
	if(culling) 
        {
		glEnable(GL_CULL_FACE);
		switch(culling) 
                {
		case 3: glFrontFace(GL_CCW); break;
		case 2: glFrontFace(GL_CW); break;
		case 1: break; // XXX: CULLING_SMALL
		}
	} 
	else 
        {
		glDisable(GL_CULL_FACE);
	}

	// XXX: not working correctly atm.
	// KOS seems to disable blending but still passes options. 
	
	//if((data2>>25)&1)
	glBlendFunc(GL_SRC_ALPHA, blendFunc[(data2>>29)&7]);
	//else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	
	//if((data2>>24)&1)
	glBlendFunc(GL_DST_ALPHA, blendFunc[(data2>>26)&7]);
	// else glBlendFunc(GL_DST_ALPHA, GL_ZERO); 
}

void Gpu::handleTaWrite(Dword addr, Dword data) 
{

	static bool withinList = false;
	static bool textureEnable = false;
	static bool textureType = false; // 16 or 32 bit
	static bool need16 = false;
	static int colorType;
	static int actListType;
	
	Dword *recvBufDwords = (Dword*)recvBuf;
	float *recvFloat = (float*)recvBuf;
	
	recvBufDwords[dwordsReceived++] = data;
	
	if(dwordsReceived == 1) 
        {
		// see which command this is and decide how many dwords to receive
		switch(data >> 29) 
                {
		case 7: // vertex
			dwordsNeeded = need16 ? 16 : 8;
			break;
		case 4: // polygon / modifier value
			
			// parameters that need 16 dwords:
			//   XXX: affected by modifier
			//   textured and floating color
			//   XXX: color type intensity and specular highlight
			need16 = (data&0x80) || 
				((data&8) && ((data>>4)&3)==1) || 
				((data&4) && (data&0x20));
			
		case 1: // user clip
		case 0: // end of list
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
				
				Dword base = 0xa5000000 | taRegs[0x4e];
				
				// XXX: why this?
				if((cpu->mmu->readDword(base) != 0x90800000) || 
				   (cpu->mmu->readDword(base+4) != 0x20800440)) 
                                   {
					if(cpu->mmu->readDword(base) || 
					   cpu->mmu->readDword(base+4))
						printf("handleTaWrite: weird flags are %08x %08x\n", 
						       cpu->mmu->readDword(base), 
						       cpu->mmu->readDword(base+4));
				}
#if 0
				
				// we should do something with these. (but what???) 
				// three points are given, usually corresponding to the
				// corners of the screen (background plane?)
				
				// could be: different color for each corner...
				// why only three are given, no idea... 
				
				// XXX: needs testing on a real dreamcast
				printf("%f %f %f\n", cpu->mmu->readFloat(base+12), 
				       cpu->mmu->readFloat(base+16), 
				       cpu->mmu->readFloat(base+20));
#endif
				
				Dword background = cpu->mmu->readDword(base+24);
				
				glClearColor((float)((background>>16)&0xff)/255, 
					     (float)((background>>8)&0xff)/255, 
					     (float)(background&0xff)/255, 1.0);
				
				// translucency
				if(actListType==2) glEnable(GL_BLEND);
				else              glDisable(GL_BLEND);
				
				glBegin(GL_TRIANGLE_STRIP);
				withinList = true;
			}
			
			// XXX: need to implement intensity and affected by modifier mode
			// XXX: need to test offset color 
			switch(colorType) 
                        {
			case 0: // 32-bit ARGB packed color
				if(textureEnable) 
                                {
					glColor4f(((recvBuf[6]>>16)&0xff -(recvBuf[7]>>16)&0xff), 
						  ((recvBuf[6]>>8) &0xff -(recvBuf[7]>>8) &0xff),
						  ((recvBuf[6]     &0xff)-(recvBuf[7]     &0xff)),
						  ((recvBuf[6]>>24)&0xff -(recvBuf[7]>>24)&0xff));
				} 
				else 
				{
					glColor4f((float)((recvBuf[6]>>16)&0xff), 
						  (float)((recvBuf[6]>>8) &0xff),
						  (float) (recvBuf[6]     &0xff),
						  (float)((recvBuf[6]>>24)&0xff));
				}
				break;
			case 1: // 4 32-bit floating point color
				if(textureEnable) 
                                {
					glColor4f(recvFloat[9]- recvFloat[13], 
						  recvFloat[10]-recvFloat[14], 
						  recvFloat[11]-recvFloat[15], 
						  recvFloat[8]- recvFloat[12]);
				} 
				else 
				{
					glColor4f(recvFloat[5], recvFloat[6], 
						  recvFloat[7], recvFloat[4]);
				}
				break;
			case 2: // intensity
			case 3: // intensity from previous face
				cpu->debugger->flamingDeath("No support for intensity color");
			}
			
			if(textureEnable){
				if(textureType) // 16 bit
					glTexCoord2f((float)(recvBuf[4]>>16), 
						     (float)(recvBuf[4]&0xffff));
				else            // 32 bit
					glTexCoord2f(recvFloat[4], recvFloat[5]);
			}
			
			//glVertex3f(recvFloat[1], recvFloat[2], recvFloat[3]);
			glVertex3fv(&recvFloat[1]);

			if(recvBufDwords[0] & 0x10000000) 
                        {
				glEnd();
				withinList = false;
			}
			break;
		case 4: // polygon / modifier volume
			
			actListType = (recvBuf[0]>>24)&0x7;
			textureType = recvBuf[0]&1;
			colorType = (recvBuf[0]>>4)&3;
			
			setOptions(recvBuf[0], recvBuf[1], recvBuf[2]);

			// both cmd and poly options must set texture enable options
			textureEnable = (recvBuf[0]&8) && (recvBuf[1]>>25)&1;
			if(textureEnable) 
                        {
				glEnable(GL_TEXTURE_2D);
				textureMap(recvBuf[2], recvBuf[3]);
			} 
			else 
                        {
				glDisable(GL_TEXTURE_2D);
			}
			
			//cpu->debugger->print("TA: Polygon / modifier volume: Listtype: %s Colortype: %s (%s)\n", listType[actListType],colortype[(recvBuf[0]>>4)&0x3],(recvBuf[0]&0x8)?"texture":"no texture");
			//cpu->debugger->print("TA: depthType: %d\n", (recvBuf[1]>>29)&0x7);
			break;
		case 1: // user clip

			if(clipMode)
			{
				int h = height()/32; 
				if(h > 20) break; // we got some weird height value

				// XXX: not working
				// XXX: may need to do stencil test instead
				glEnable(GL_SCISSOR_TEST);

				glScissor(recvBuf[4]*32, recvBuf[5]*32, 
					  (recvBuf[6]-recvBuf[4]+1)*32, 
					  (recvBuf[7]-recvBuf[5]+1)*32);
			
			}
			else
			{
				glDisable(GL_SCISSOR_TEST);
			}

			break;
		case 0: // End of list
			
			//cpu->debugger->print("TA: end of %s list\n",listType[actListType]);

			cpu->maple->asicAckA = actListData[actListType];
			
			// have we gotten the last list type yet? then let's do our voodoo
			if(lastType==actListType) 
                        {
				SDL_GL_SwapBuffers();
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glLoadIdentity();
				updateFPS();
			}
			
			withinList = false;
			break;
		default: 
			cpu->debugger->flamingDeath("TA: *** unimplemented TA command\n");
		}
	}
	if(screen==NULL) SDL_FreeSurface(screen);
}

void Gpu::updateFPS() 
{
	
	static struct timeval tv, oldtv;
	
	fpsCounter++;
	gettimeofday(&tv, NULL);
	
	if(tv.tv_sec > oldtv.tv_sec) 
        {      
		fprintf(stderr, "FPS: %d  \r", fpsCounter);
		oldtv.tv_sec = tv.tv_sec;
		fpsCounter = 0;
	} 
}

void Gpu::draw2DFrame() 
{
	if(taEnabled() || (screen == NULL) || (currBackBuffer == NULL))
		return;
	
	SDL_LockSurface(screen);
	
	memcpy(screen->pixels, currBackBuffer->pixels, 
	       currBackBuffer->pitch*currBackBuffer->h);
	
	SDL_UnlockSurface(screen);
	SDL_Flip(screen);
	
	// below disabled because it's going too fast in 2d mode
	//updateFPS();
}

Dword Gpu::hook(int eventType, Dword addr, Dword data) 
{
	
	Dword offs = (addr>>2) & 0xfff;

	switch(eventType) 
        {
	case MMU_READ_DWORD:
		
		//if(offs<0x80 && regs[offs]==0)
		//  cpu->debugger->print("GPU::access WARNING: read reg %.3x\n", offs);
		taRegs[GPU_SYNC_STAT] ^= 0x1ff;

		return taRegs[offs];

		break;
	case MMU_WRITE_DWORD:

		taRegs[offs] = data;

		// special case: figure out how much of each active list type we have
		if((offs==PVR_OPB_CFG) && (lastOPBConfig != taRegs[offs])) 
                {
			
			lastOPBConfig = taRegs[offs];
			
			punchThru = (lastOPBConfig>>16)&3;
			transMod  = (lastOPBConfig>>12)&3;
			transPoly = (lastOPBConfig>>8)&3;
			opaqMod   = (lastOPBConfig>>4)&3;
			opaqPoly  = (lastOPBConfig&3);
			
			if(punchThru)      lastType = 4;
			else if(transMod)  lastType = 3;
			else if(transPoly) lastType = 2;
			else if(opaqMod)   lastType = 1;
			else               lastType = 0;
		}
		break;

	default:
		cpu->debugger->flamingDeath("GPU::access: non-dword access!");
	}

	// XXX: when exactly should we do this?
	modeChange();
	return 0;
}

int Gpu::width()
{
	int tmp;
	tmp = bits(taRegs[GPU_DISPLAY_SIZE], 9, 0);
	tmp++; // only works for VGA
	tmp *= 4;
	tmp /= (bpp() / 8);
	return tmp;
}

int Gpu::height()
{
	int tmp;
	tmp = bits(taRegs[GPU_DISPLAY_SIZE], 19, 10);
	tmp++; // only works for VGA
	return tmp;
}

int Gpu::bpp() 
{
	
	switch((taRegs[GPU_DISPLAY_ENABLE] & 0xf) >> 2) 
        {
	case 0: 
	case 1: return 16;
	case 3: return 32;
	default:
		cpu->debugger->flamingDeath("Gpu::bpp: Unknown bpp value in GPU_DISPLAY_ENABLE");
		return 0;
	}
}

void Gpu::masks(Dword *rm, Dword *gm, Dword *bm, Dword *am)
{
	switch((taRegs[GPU_DISPLAY_ENABLE] & 0xf) >> 2)
		{
		case 0: *bm = 0x1f; *gm = 0x1f << 5; *rm = 0x1f << 10; *am = 0; break;
		case 1: *bm = 0x1f; *rm = 0x1f << 11; *gm = 0x3f << 5; *am = 0; break;
		case 3: *rm = 0x00ff0000; *gm = 0x0000ff00; *bm = 0x000000ff; *am = 0xff000000;
			break;
		default:
			cpu->debugger->flamingDeath("Gpu::masks: Unknown values set in GPU_DISPLAY_ENABLE");
		}
}

int Gpu::pitch() 
{
	Dword mod = bits(taRegs[GPU_DISPLAY_SIZE], 29, 20) - 1;
	return width() * (bpp() / 8) + (mod * 4);
}

Dword Gpu::baseAddr() 
{
	return taRegs[GPU_DISPLAY_ADDR_ODD] & 0x00fffffc;
	//return taRegs[GPU_DISPLAY_ADDR_EVEN] & 0x00fffffc;
}

// are we in a sane enough state to attempt a host graphics mode switch?
bool Gpu::sane() 
{
	
	if((width() < 320) || (width() > 805))   return false;
	if((height() < 240) || (height() > 610)) return false;
	return true;
}

bool Gpu::taEnabled() 
{
	return (taRegs[GPU_VRAM_CFG3] == 0x15d1c951);
}

void Gpu::modeChange() 
{
	
	static bool taActive = false; 

	if(!sane()) return;
	if(width()%32) cpu->debugger->flamingDeath("Gpu::sane: width is not a multiple of 32");
	
	//cpu->debugger->print("Gpu::modeChange: mode change to %dx%d, %dbpp\n", width(), height(), bpp());
	
	// Switch to 3D mode if necessary
	if(taEnabled()) 
        {
		if(taActive) return;
		taActive = true;

		if(screen != 0) SDL_FreeSurface(screen);
		
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		
		screen = SDL_SetVideoMode(width(), height(), bpp(), SDL_OPENGL | SDL_DOUBLEBUF);

		if(screen == NULL)
			cpu->debugger->flamingDeath("Gpu::modeChange: Couldn't set OpenGL video mode to %dx%d, %dbpp!", width(), height(), bpp());
		
		char caption[512];
		sprintf(caption, "%s - %dx%d %dbpp - OpenGL", VERSION_STRING, width(), height(), bpp());
		SDL_WM_SetCaption(caption, NULL);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		//glFrustum(0, width(), 0, height(), -1.0, RegFloat(0x22));
		//printf("near plane: %f\n", *(float *)&taRegs[0x22]); // typically 0.0001f
		//glOrtho(0, width(), height(), 0, -1, 1);

		glOrtho(0, width(), height(), 0, -1000.0, 10000.0);
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glShadeModel(GL_SMOOTH);
		
		glClearDepth(1.0f);
		glEnable(GL_DEPTH_TEST);
		
		glViewport(0, 0, width(), height());

		
		return;
	}
	
	taActive = false;

	// See if we have a back buffer already made that matches the current GPU settings
	for(int i=0; i<GPU_MAXBACKBUFFERS; i++) 
        {
		if( // hooray for short-circuit evaluation of expressions
		   (backBuffers[i] != 0) &&
		   (backBufferDCAddrs[i] == baseAddr()) &&
		   (backBuffers[i]->format->BitsPerPixel == bpp()) &&
		   (backBuffers[i]->w == width()) &&
		   (backBuffers[i]->h == height()) ) {
			//cpu->debugger->print("Gpu::modeChange: reused back buffer for base addr %08x\n", baseAddr());
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
		currBackBuffer = SDL_CreateRGBSurfaceFrom((void*)(((Dword)cpu->mmu->videoMem)+baseAddr()),
							  width(), height(), bpp(),  pitch(),
							  rmask, gmask, bmask, amask);
		
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
