#include <sys/time.h> // FPS
#include <string.h>

#include "Gpu.h"
#include "Debugger.h"
#include "SHMmu.h"
#include "Maple.h"
#include "SHIntc.h"

#include <map>

using namespace std;

typedef map<Dword, Dword> tex_map; 
tex_map texture_map;

// color matrix we're gonna use. simply switches RGBA-->ARGB.
static GLfloat rgbacolor[16] = {
  0.0, 0.0, 0.0, 1.0, 
  1.0, 0.0, 0.0, 0.0, 
  0.0, 1.0, 0.0, 0.0, 
  0.0, 0.0, 1.0, 0.0
};

// arrays to simplify printing out information

char *listType[] = { "opaque polygon", 
		     "opaque modifier", 
		     "transparent polygon",
		     "transparent modifier", 
		     "punchthru polygon"};

char *colortype[] = { "32bit ARGB packed colour", 
		      "32bit * 4 floating point colour",
		      "intensity", 
		      "intensity from previous face"};

// static arrays to simplify parameters for pvr/opengl commands

GLenum depthType[] = { GL_NEVER, GL_GEQUAL, GL_EQUAL, GL_GREATER,
                       GL_LEQUAL, GL_NOTEQUAL, GL_LESS, GL_ALWAYS};

GLenum blendFunc[] = { GL_ZERO, GL_ONE, GL_DST_COLOR, 
		       GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
		       GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
		       GL_ONE_MINUS_DST_ALPHA };

GLint texParam[] = { GL_REPEAT, GL_CLAMP };

GLfloat texEnv[] = { GL_REPLACE, GL_MODULATE, GL_DECAL, GL_MODULATE };
                    // XXX: GL_MODULATEALPHA (C = Cs * Ct, A = As * At) 

int texSizeList[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
	
int actListData[] = { OPAQUE, OPAQUEMOD, TRANS, TRANSMOD, PUNCHTHRU };

Gpu::Gpu() : dwordsReceived(0), screen(0), currBackBuffer(0)
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
    
    taQueueCount = 0;
    currTexNum = 0;
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
    
#define TWIDDLE(x) (x&0x01|((x&0x02)<<1)|((x&0x04)<<2)|((x&0x08)<<3)| \
             ((x&0x10)<<4)|((x&0x20)<<5)|((x&0x40)<<6)|((x&0x80)<<7)| \
             ((x&0x100)<<8)|((x&0x200)<<9))
  
    Dword texAddr;
    int min = w > h ? h : w;
    int mask = min-1;
    int i, ytwid, square = 0;
    
    bool invert = false; // we don't invert textures right now
    
    if(w != h) square = min*min;
    
    for(int y=0; y<h; y++) 
    {
	i = invert ? (h-1)-y : y; 
	ytwid = TWIDDLE((i&mask));
	for(int x=0; x<w; x++) 
	{
	    texAddr = (TWIDDLE((x&mask)) << 1) | ytwid;
	    if(x>min) texAddr += (x/min)*square;
	    if(i>min) texAddr += (i/min)*square;
	    
	    texAddr <<= 1;
	    
	    //texture[y*w+x] = *(Word *)(mmu->texMem+address+texAddr);
	    texture[y*w+x] = *(Word *)(mmu->videoMem+address+texAddr);
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

			texture[y*w+x] = *(Byte *)(mmu->texMem+address+texAddr);
		}
	}
}

void Gpu::decompressVQ(Word *texture, Dword address, int w, int h) 
{
	Word *codebook = (Word *)(mmu->texMem+address); // 256*4 word entries

#if 0
        Byte *buf = new Byte[w*h];
	Word *code = new Word[1024];

	FILE *fp;

	fp = fopen("fruit.vq", "rb");

	fread(code, 1024, 2, fp);
	fread(buf, w*h/4, 1, fp);

	fclose(fp);

	int x = 0;
	for(int i=0; i<w*h/4; i++) {
		Byte index = buf[i]<<2; 

		texture[x++] = code[index+0];
		texture[x++] = code[index+2]; 
		texture[x++] = code[index+1]; 
		texture[x++] = code[index+3]; 
	}
#if 0
	printf("twiddled texture follows:\n");
	Word *wb = (Word *)buf;
	for(int i=0; i<w*h/8; i++) {
		if((i%8)==0) printf("\n");
		printf("%04x ", wb[i]);
	}
#endif

#else
        Byte *buf = new Byte[w*h];

	// XXX:ls we need a correct twiddling algo
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
#endif
} 

Dword Gpu::checksum(Dword addr, Dword size)
{
    // crappy "checksum"
    Dword sum = 0;

    for(Dword i = 0; i<size; i++)
	sum += *(mmu->videoMem+addr+i);

    //printf("checksum is %08x\n", sum);
	    
    return sum;
}

void Gpu::textureMap(Dword data2, Dword data3) 
{
	Dword address = (data3<<3)&0x00fffff8;

	int w = texSizeList[data2&0x38>>3]; // XXX: pvr reg 0x39: modulo width
	int h = texSizeList[data2&0x7];

	bool loaded = false;

	if(texture_map.empty()) glGenTextures(MAX_TEXTURES, tex);
	
	tex_map::iterator i = texture_map.find(address);

	if(i->first == address) {
	    
	    glBindTexture(GL_TEXTURE_2D, tex[i->second]);

	    Dword oldchecksum = textures[i->second].checksum;
	    textures[i->second].checksum = checksum(address, w*h*2);

	    // reload if texture data or options have changed
	    loaded = textures[i->second].checksum == oldchecksum && 
		textures[i->second].data == data3 &&
		textures[i->second].width == w && 
		textures[i->second].height == h; 

	    if(!loaded) printf("reusing texture\n");
	}

	if(!loaded) 
	{
		if(currTexNum >= MAX_TEXTURES)
		    debugger->flamingDeath("Too many textures loaded");
	
		// retrieve and save texture options
		int format = (data3>>27)&3;

		textures[currTexNum].checksum = checksum(address, w*h*2);
		textures[currTexNum].address = address;
		textures[currTexNum].format = format;
		textures[currTexNum].width = w;
		textures[currTexNum].height = h;
		textures[currTexNum].data = data3;

		textures[currTexNum].options |= ~(data3>>26)&1;
		textures[currTexNum].options |= ((data3>>30)&1) << 1;
		textures[currTexNum].options |= ((data3>>21)&1) << 2;
		textures[currTexNum].options |= ((data3>>31)&1) << 3;
		textures[currTexNum].bias = (data2>>8)&15; 

		if(!(textures[currTexNum].options & TEX_LOADED))
		    debugger->printTexInfo(currTexNum);
		
		// bind the texture
		glBindTexture(GL_TEXTURE_2D, tex[currTexNum]);
		texture_map[address] = currTexNum;

		// now generate the actual texture
		Word *texture = new Word[w*h];

		switch(textures[currTexNum].options & 0xf)
		{
		case 0: memcpy(texture, mmu->videoMem+address, w*h*2); break;
		case 1: twiddleTexture(texture, address, w, h);        break;
		case 3: decompressVQ(texture, address, w, h);          break;

		default:
		    debugger->flamingDeath("Texture option not supported");
		}
	    
		switch(format) 
		{
		case 0: // ARGB1555 
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, 
				     GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, texture);
			break;
		case 1: // RGB565
			glMatrixMode(GL_COLOR);
			glLoadIdentity();
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, 
				     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, texture);

			glLoadMatrixf(rgbacolor);
			glMatrixMode(GL_MODELVIEW);
			break;
		case 2: // ARGB4444 
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, 
				     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, texture);
			break;
		case 3: // YUV422
		case 4: // BUMP
		case 5: // 4BPP_PALETTE
		case 6: // 8BPP_PALETTE
		default:
			debugger->flamingDeath("Texture format not supported");
		}
	
		delete [] texture;

		currTexNum++;
	}
}

void Gpu::setOptions(Dword data0, Dword data1, Dword data2) 
{
#if 0
	// 0: striplength 1, 1: striplength 2, 2: striplength 4, 3: striplength 6
	// dunno how old this information is. disabled for now. 
	// XXX: check pvrmark, seems to trigger this.
       	int stripLength = (data0>>18)&3;
	if(stripLength) 
		debugger->flamingDeath("Different strip length %d set", stripLength);

	// XXX: does not work
	// clipmode: 0 == disable, 2 == inside, 3 == outside
	clipMode = (data0>>16)&3; 
	// XXX: check to see if clipmode is enabled when transparent lists are sent
	if(((data0>>24)&7)==0) 
        {
		glEnable(GL_SCISSOR_TEST);
		clipMode = (data0>>16)&3;
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}
#endif

	glDepthFunc(depthType[(data1>>29)&0x7]);
	glShadeModel((data0>>1)&1 ? GL_SMOOTH : GL_FLAT);

	// enable or disable depth buffer writing (0 == enabled, 1 == disabled)
	if((data1>>26)&1) glDepthMask(GL_FALSE);
	else              glDepthMask(GL_TRUE);

#if 0	
	// XXX: maybe use glColorMask for this?
	if((data2>>20)&1) glEnable(GL_ALPHA_TEST);
	else             glDisable(GL_ALPHA_TEST);

	bool colorClamp = (data2>>21)&1;  // not supported
	
	bool modifier = (data0>>7)&1;     // not supported
	bool modifierMode = (data0>>6)&1; // not supported
	
	int fogMode = (data2>>22)&3;  // 0: table 1: vertex 2: disable 3: table2
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
	
    taQueueCount++;

    if(dwordsReceived == 1) 
    {
	// see which command this is and decide how many dwords to receive
	switch(data >> 29) 
	{
	case 7: // vertex
	    dwordsNeeded = need16 ? 16 : 8;
	    taQueueCount -= 8;
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
		if((mmu->readDword(base) != 0x90800000) || 
		   (mmu->readDword(base+4) != 0x20800440)) 
		{
		    if(mmu->readDword(base) || 
		       mmu->readDword(base+4))
			printf("handleTaWrite: weird flags are %08x %08x\n", 
			       mmu->readDword(base), 
			       mmu->readDword(base+4));

		    //handleTaWrite: weird flags are 80800000 20a984ed
		}
#if 0
				
		// we should do something with these. (but what???) 
		// three points are given, usually corresponding to the
		// corners of the screen (background plane?)
		
		// XXX: needs testing on a real dreamcast
		printf("%f %f %f\n", mmu->readFloat(base+12), 
		       mmu->readFloat(base+16), 
		       mmu->readFloat(base+20));
#endif
		
		Dword background = mmu->readDword(base+24);
		
		glClearColor((float)((background>>16)&0xff)/255, 
			     (float)((background>>8)&0xff)/255, 
			     (float)(background&0xff)/255, 1.0);
		
		// do we have transparent polys? if so -- blend
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
		    glColor4f((recvBuf[6]>>16)&0xff,
			      (recvBuf[6]>>8) &0xff,
			      (recvBuf[6]>>0) &0xff,
			      (recvBuf[6]>>24)&0xff);
		}
		break;
	    case 1: // 4 32-bit floating point color
		if(textureEnable) 
		{
		    
		    recvBuf[8] -= recvBuf[12];
		    recvBuf[9] -= recvBuf[13];
		    recvBuf[10] -= recvBuf[14];
		    recvBuf[11] -= recvBuf[15];
		    
		    glColor4f(recvFloat[9], recvFloat[10],
			      recvFloat[11], recvFloat[8]);
		} 
		else 
		{
		    glColor4f(recvFloat[5], recvFloat[6],
			      recvFloat[7], recvFloat[4]);
		}
		break;
	    case 2: // intensity
	    case 3: // intensity from previous face
		debugger->flamingDeath("No support for intensity color");
	    }
	    
	    if(textureType) { // 16 bit
		//glTexCoord2sv(&recvBuf[4]);
		glTexCoord2f((float)(recvBuf[4]>>16), 
			     (float)(recvBuf[4]&0xffff));
		printf("16 bit!\n");
	    }
	    else    
		glTexCoord2fv(&recvFloat[4]);
	    
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
	    
	    //debugger->print("TA: Polygon / modifier volume: Listtype: %s Colortype: %s (%s)\n", listType[actListType],colortype[(recvBuf[0]>>4)&0x3],(recvBuf[0]&0x8)?"texture":"no texture");
	    //debugger->print("TA: depthType: %d\n", (recvBuf[1]>>29)&0x7);
	    break;
	case 1: // user clip
	    
	    if(clipMode)
	    {
		int h = height()/32; 
		if(h > 18) break; // we got some weird height value
		
		// XXX: need to convert to glOrtho parms...
		glScissor(recvBuf[4]*32, recvBuf[5]*32, 
			  (recvBuf[6]-recvBuf[4]+1)*32, 
			  (recvBuf[7]-recvBuf[5]+1)*32);
		
	    }
	    
	    break;
	case 0: // End of list
	    
	    //debugger->print("TA: end of %s\n",listType[actListType]);
	    
	    maple->setAsicA(actListData[actListType]);
	    //intc->externalInt9a(actListData[actListType]);

	    // have we gotten the last list type yet? then let's do our voodoo
	    if(lastType==actListType) 
	    {
		maple->setAsicA(RENDER_DONE);
		//intc->externalInt9a(RENDER_DONE);

		SDL_GL_SwapBuffers();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		updateFPS();
	    }
	    
	    withinList = false;
	    break;
	default: 
	    debugger->flamingDeath("TA: *** unimplemented TA command\n");
	}
    }
}

void Gpu::updateFPS() 
{
	
	static struct timeval tv, oldtv;
	
	fpsCounter++;
	gettimeofday(&tv, NULL);

	int e = glGetError();
	if(e) printf("glGetError returned 0x%x -- something messed up\n", e);

	if(tv.tv_sec > oldtv.tv_sec) 
        {      
	    fprintf(stderr, "TA: %d; FPS: %d  \r", 
	    		(taQueueCount>>3)/fpsCounter, fpsCounter);

	    oldtv.tv_sec = tv.tv_sec;
	    fpsCounter = 0;
	    taQueueCount = 0;
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
	
	// disabled because it's going too fast in 2d mode
	//updateFPS();
}

Dword Gpu::hook(int eventType, Dword addr, Dword data) 
{
	
	Dword offs = (addr>>2) & 0xfff;

	switch(eventType) 
        {
	case MMU_READ_DWORD:
		
		//if(offs<0x80 && regs[offs]==0)
		//  debugger->print("GPU::access WARNING: read reg %.3x\n", offs);
		taRegs[GPU_SYNC_STAT] ^= 0x1ff;
		taRegs[GPU_SYNC_STAT] ^= 0x2000;

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
		debugger->flamingDeath("GPU::access: non-dword access!");
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
	debugger->flamingDeath("Gpu::bpp: Unknown bpp value in GPU_DISPLAY_ENABLE");
	return 0;
    }
}

void Gpu::masks(Dword *rm, Dword *gm, Dword *bm, Dword *am)
{
    switch((taRegs[GPU_DISPLAY_ENABLE] & 0xf) >> 2)
    {
    case 0: *bm = 0x1f; *gm = 0x1f << 5; *rm = 0x1f << 10; *am = 0; break;
    case 1: *bm = 0x1f; *rm = 0x1f << 11; *gm = 0x3f << 5; *am = 0; break;
    case 3: *rm = 0x00ff0000; *gm = 0x0000ff00; 
	    *bm = 0x000000ff; *am = 0xff000000;
	    break;
    default:
	debugger->flamingDeath("Gpu::masks: Unknown values set in GPU_DISPLAY_ENABLE");
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

    if(width()%32) 
	debugger->flamingDeath("Gpu::sane: width is not a multiple of 32");
    
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
	    debugger->flamingDeath("Gpu::modeChange: Couldn't set OpenGL video mode to %dx%d, %dbpp!", width(), height(), bpp());

	char caption[512];
	sprintf(caption, "%s - %dx%d %dbpp - OpenGL", VERSION_STRING, 
		width(), height(), bpp());
	SDL_WM_SetCaption(caption, NULL);

	glMatrixMode(GL_COLOR);
	glLoadMatrixf(rgbacolor); 
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	//glFrustum(0, width(), 0, height(), -1.0, 0.0001f);
	printf("near plane: %f\n", *(float *)&taRegs[0x22]); 
	//glOrtho(0, width(), height(), 0, -1, 1);
	
	glOrtho(0, width(), height(), 0, -1000.0, 10000.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_SMOOTH);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	
	glViewport(0, 0, width(), height());

	// sometimes programs (e.g., KOS >= 1.2.0) want to wait for a 
	// complete pvr list before doing anything so here it is... 

	if(punchThru) maple->setAsicA(PUNCHTHRU);
	if(transMod)  maple->setAsicA(TRANSMOD);
	if(transPoly) maple->setAsicA(TRANS);
	if(opaqMod)   maple->setAsicA(OPAQUEMOD);
	if(opaqPoly)  maple->setAsicA(OPAQUE);

	maple->setAsicA(RENDER_DONE);

	//SDL_WM_ToggleFullScreen(screen);

	return;
    }
	
    taActive = false;
    
    for(int i=0; i<GPU_MAXBACKBUFFERS; i++)
    {
	if( // hooray for short-circuit evaluation of expressions
	   (backBuffers[i] != 0) &&
	   (backBufferDCAddrs[i] == baseAddr()) &&
	   (backBuffers[i]->format->BitsPerPixel == bpp()) &&
	   (backBuffers[i]->w == width()) &&
	   (backBuffers[i]->h == height()) ) {
	    //debugger->print("Gpu::modeChange: reused back buffer for base addr %08x\n", baseAddr());
	    currBackBuffer = backBuffers[i];
	    break;
	}
    }
    
    // If we didn't have an already made back buffer, make one
    // and put it in the back buffers array
    if((currBackBuffer == 0) ||
       ((currBackBuffer->w != width()) || (currBackBuffer->h != height()) ||
	(currBackBuffer->format->BitsPerPixel != bpp())))
        {
	    Dword rmask, gmask, bmask, amask;
	    
	    masks(&rmask, &gmask, &bmask, &amask);
	    currBackBuffer = SDL_CreateRGBSurfaceFrom((void*)(((Dword)mmu->videoMem)+baseAddr()),width(), height(), bpp(), pitch(), rmask, gmask, bmask, amask);

	    int i;
	    for(i=0; i<GPU_MAXBACKBUFFERS; i++)
            {
		if(backBuffers[i] == 0)
		    break;
	    }
	    
	    // XXX: make this kill one of the old back buffers to make room
	    //for the new one
	    if(i >= GPU_MAXBACKBUFFERS)
		debugger->flamingDeath("Gpu::modeChange: too many back buffers allocated");

	    backBuffers[i] = currBackBuffer;
	    backBufferDCAddrs[i] = baseAddr();
        }

    if(screen != 0)
    {
	if((screen->w != width()) || (screen->h != height()) || (screen->format->BitsPerPixel != bpp()))
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
	sprintf(caption, "%s - %dx%d %dbpp", VERSION_STRING, 
		width(), height(), bpp());
	SDL_WM_SetCaption(caption, NULL);
    }

    if(currBackBuffer->format->BitsPerPixel != screen->format->BitsPerPixel)
	*((Dword*)0) = 0xdeadbeef;
}
