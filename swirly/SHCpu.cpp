// SHCpu.cpp: interpretive SH4 core
//
//////////////////////////////////////////////////////////////////////

#include <math.h>

#include "SHCpu.h"
#include "SHMmu.h"
#include "SHDmac.h"
#include "Debugger.h"
#include "Overlord.h"
#include "Gpu.h"
#include "dcdis.h"

float shfpu_sop0, shfpu_sop1, shfpu_sresult;
double shfpu_dop0, shfpu_dop1, shfpu_dresult;

SHCpu::SHCpu()
{
	mmu = new SHMmu(this);
	debugger = new Debugger(this);
	gpu = new Gpu(this);
	tmu = new SHTmu(this);
	bsc = new SHBsc(this);
	sci = new SHSci(this);
	dmac = new SHDmac(this);
	intc = new SHIntc(this);
	//gdrom = new Gdrom(this, 13986);
	gdrom = new Gdrom(this, 0);
	spu = new Spu(this);
	maple = new Maple(this);
	reset();
}

SHCpu::~SHCpu()
{
	delete mmu;
	delete debugger;
	delete gpu;
	delete tmu;
	delete bsc;
	delete sci;
	delete dmac;
	delete intc;
	delete gdrom;
	delete spu;
	delete maple;
}

// allows programs running on Swirly to hook into Swirly
// i.e. the GDROM - I don't know the proper hardware interface
// to it, but we can simulate it through faked syscalls.
// Obviously, this is not present on a real DC.
void SHCpu::dispatchSwirlyHook()
{
	Word hookId = mmu->fetchInstruction(PC+2);
	/*
	How to do a swirly hook

	.word 0xfffd	! invoke hook
	.word 1			! hook id=1

	The hook will get its parameters from the registers.  0xfffd is
	the SH4 undefined instruction, so we shouldn't have any conflict
	with real software.
	*/

	switch(hookId)
	{
	case HOOK_GDROM:
		gdrom->hook();
		break;
	case HOOK_LOAD1STREAD:
	{
		debugger->print("HOOK_LOAD1STREAD: loading 1ST_READ.BIN\n");

		int startoffs = (R[4] - gdrom->startSector())* gdrom->sectorSize;
		debugger->print("HOOK_LOAD1STREAD: startoffs = %08x = %d * %d\n", startoffs, gdrom->startSector(), gdrom->sectorSize);
		Overlord::loadAndDescramble(gdrom->cdImage, startoffs, R[5], this, 0x8c010000);
		debugger->print("HOOK_LOAD1STREAD: done loading 1ST_READ.BIN\n");
	}
		break;
	default:
		debugger->flamingDeath("SHCpu::dispatchSwirlyHook: Invalid Swirly hook - %d", hookId);
	}

	PC+=4;
}

void SHCpu::unknownOpcode()
{
	debugger->print("SHCpu: encountered unknown upcode at PC=%08X\n", PC);
	PC+=2;
}

void SHCpu:: XTRCT(int m, int n)
{
	Dword t;
	t=(R[m]<<16)&0xffff0000;
	R[n]=(R[n]>>16)&0xffff;
	R[n]|=t;
	PC+=2;
}

void SHCpu:: XORI(Byte i)
{
	R[0]^=(i&0xff);
	PC+=2;
}

void SHCpu:: XORM(Byte i)
{
	Dword t;
	t=mmu->readByte(GBR+R[0]);
	t^=(i&0xff);
	mmu->writeByte(GBR+R[0], t);
	PC+=2;
}

void SHCpu:: XOR(int m, int n)
{
	R[n]^=R[m];
	PC+=2;
}

void SHCpu:: TST(int m, int n)
{
	if((R[n]&R[m])==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu:: TSTI(Byte i)
{
	Dword t;
	t=R[0]&(0xff&i);
	if(t==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu:: TSTM(Byte i)
{
	Dword t;
	t=mmu->readByte(GBR+R[0]);
	t&=(i&0xff);
	if(t==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu:: TRAPA(Byte i)
{
    /* TODO: this gets uncommented when we do exceptions*/
	SSR=SR;
	SPC=PC+2;
	setSR(SR|F_SR_MD|F_SR_BL|F_SR_RB);
	mmu->writeDword(0xff000000|TRA, (i&0xff)<<2);
	mmu->writeDword(0xff000000|EXPEVT, 0x160);
	PC=VBR+0x100;
}

void SHCpu:: TAS(int n)
{
	Sdword t, oldt;
	oldt=t=(Sdword)mmu->readByte(R[n]);
	t|=0x80;
	mmu->writeByte(R[n],t);
	if(oldt==0)T=1; else T=0;
	PC+=2;
}

void SHCpu:: SWAPW(int m, int n)
{
	Dword t;
	t=(R[m]>>16)&0xffff;
	R[n]=R[m]<<16;
	R[n]|=t;
	PC+=2;
}

void SHCpu:: SWAPB(int m, int n)
{
	Dword t0, t1;
	t0=R[m]&0xffff0000;
	t1=(R[m]&0xff)<<8;
	R[n]=(R[m]&0xff00)>>8;
	R[n]=R[n]|t1|t0;
	PC+=2;
}


void SHCpu:: SUBV(int m, int n)
{
	Sdword d,s,a;
	if((Sdword)R[n]>=0)d=0;else d=1;
	if((Sdword)R[m]>=0)s=0; else s=1;
	s+=d;
	R[n]-=R[m];
	if((Sdword)R[n]>=0)a=0; else a=1;
	a+=d;
	if(s==1){if(a==1)T=1;else T=0; } else T=0;
	PC+=2;
}

void SHCpu:: SUBC(int m, int n)
{
	Dword t0, t1;
	t1=R[n]-R[m];
	t0=R[n];
	R[n]=t1-T;
	if(t0<t1)T=1;else T=0;
	if(t1<R[n]) T=1;
	PC+=2;
}

void SHCpu:: SUB(int m, int n)
{
	R[n]-=R[m];
	PC+=2;
}

void SHCpu:: STSMACH(int n)
{
	R[n]=MACH;
	PC+=2;
}

void SHCpu:: STSMACL(int n)
{
	R[n]=MACL;
	PC+=2;
}

void SHCpu:: STSPR(int n)
{
	R[n]=PR;
	PC+=2;
}

void SHCpu:: STSMMACH(int n)
{
	mmu->writeDword(R[n]-4, MACH);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STSMMACL(int n)
{
	mmu->writeDword(R[n]-4, MACL);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STSMPR(int n)
{
	mmu->writeDword(R[n]-4, PR);
	R[n]-=4;
	PC+=2;
}

void SHCpu::STSFPSCR(int n)
{
	R[n] = FPSCR & 0x003fffff;
	PC+=2;
}

void SHCpu::STSFPUL(int n)
{
	R[n] = FPUL;
	PC+=2;
}

void SHCpu::STSMFPSCR(int n)
{
	R[n]-=4;
	mmu->writeDword(R[n], FPSCR & 0x003fffff);
	PC+=2;
}

void SHCpu:: STSMFPUL(int n)
{
	R[n]-=4;
	mmu->writeDword(R[n], FPUL);
	PC+=2;
}

void SHCpu:: STCSR(int n) // privileged
{
	R[n]=SR;
	PC+=2;
}

void SHCpu:: STCGBR(int n)
{
	R[n]=GBR;
	PC+=2;
}

void SHCpu:: STCVBR(int n)
{
	R[n]=VBR;
	PC+=2;
}

void SHCpu:: STCSSR(int n)
{
	R[n]=SSR;
	PC+=2;
}

void SHCpu:: STCSPC(int n)
{
	R[n]=SPC;
	PC+=2;
}

void SHCpu:: STCSGR(int n)
{
	R[n]=SGR;
	PC+=2;
}


void SHCpu:: STCDBR(int n)
{
	R[n]=DBR;
	PC+=2;
}

void SHCpu:: STCRBANK(int m, int n)
{
	R[n]=RBANK[m];
	PC+=2;
}

void SHCpu:: STCMSR(int n)
{
	mmu->writeDword(R[n]-4, SR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMGBR(int n)
{
	mmu->writeDword(R[n]-4, GBR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMVBR(int n)
{
	mmu->writeDword(R[n]-4, VBR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMSSR(int n)
{
	mmu->writeDword(R[n]-4, SSR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMSPC(int n)
{
	mmu->writeDword(R[n]-4, SPC);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMSGR(int n)
{
	mmu->writeDword(R[n]-4, SGR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMDBR(int n)
{
	mmu->writeDword(R[n]-4, DBR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMRBANK(int m, int n)
{
	mmu->writeDword(R[n]-4, RBANK[m]);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: SLEEP()
{
	// TODO: make this do something
	PC+=2;
}

void SHCpu:: SHLR(int n)
{
	if((R[n]&1)==0) T=0;
	else T=1;
	R[n]>>=1;
	R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu:: SHLR16(int n)
{
	R[n]>>=16;
	PC+=2;
}

void SHCpu:: SHLR8(int n)
{
	R[n]>>=8;
	PC+=2;
}

void SHCpu:: SHLR2(int n)
{
	R[n]>>=2;
	PC+=2;
}

void SHCpu:: SHLL16(int n)
{
	R[n]<<=16;
	PC+=2;
}

void SHCpu:: SHLL8(int n)
{
	R[n]<<=8;
	PC+=2;
}

void SHCpu:: SHLL2(int n)
{
	R[n]<<=2;
	PC+=2;
}

void SHCpu:: SHLL(int n)
{
	if((R[n]&0x80000000)==0) T=0; else T=1;
	R[n]<<=1;
	PC+=2;
}

void SHCpu:: SHLD(int m, int n)
{
	Sdword s;
	
	s = R[m]&0x80000000;
	if(s==0)
		R[n]<<=(R[m]&0x1f);
	else if((R[m]&0x1f)==0)
		R[n]=0;
	else
		R[n]=(Dword)R[n]>>((~R[m]&0x1f)+1);
	PC+=2;
}

void SHCpu:: SHAR(int n)
{
	Dword t;
	if((R[n]&1)==0)T=0;else T=1;
	if((R[n]&0x80000000)==0)t=0; else t=1;
	R[n]>>=1;
	if(t==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu:: SHAL(int n)
{
	if((R[n] & 0x80000000) == 0) T=0;
	else T=1;
	R[n] <<=1;
	PC+=2;
}

void SHCpu:: SHAD(int m, int n)
{
	Sdword s;
	s=R[m]&0x80000000;
	if(s==0)
		R[n]<<= (R[m]&0x1f);
	else if((R[m] & 0x1f) == 0)
	{
		if((R[n] & 0x80000000) == 0)
			R[n]=0;
		else
			R[n]=0xffffffff;
	}
	else
		R[n]=(Sdword)R[n] >> ((~R[m] & 0x1f)+1);
	PC+=2;
}		

void SHCpu:: SETT()
{
	T=1;
	PC+=2;
}

void SHCpu:: SETS()
{
	S=1;
	PC+=2;
}

void SHCpu:: RTS()
{
	delaySlot();
	debugger->reportBranch("rts", PC, PR);
	PC=PR;
}	

void SHCpu:: RTE() // privileged
{
	delaySlot();
	setSR(SSR);
	debugger->reportBranch("rte", PC, SPC);
	PC=SPC;
}

void SHCpu:: ROTR(int n)
{
	if((R[n]&1)==0)T=0;
	else T=1;
	R[n]>>=1;
	if(T==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu:: ROTL(int n)
{
	if((R[n]&0x80000000)==0)T=0;else T=1;
	R[n]<<=1;
	if(T==1)R[n]|=1;
	else R[n]&=0xfffffffe;
	PC+=2;
}

void SHCpu:: ROTCR(int n)
{
	Dword t;
	if((R[n]&1)==0)t=0;
	else t=1;
	R[n]>>=1;
	if(T==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	if(t==1)T=1; else T=0;
	PC+=2;
}

void SHCpu:: ROTCL(int n)
{
	Dword t;
	if((R[n]&0x80000000)==0)t=0;else t=1;
	R[n]<<=1;
	if(T==1)R[n]|=1;
	else R[n]&=0xfffffffe;;
	if(t==1)T=1;else T=0;
	PC+=2;
}

void SHCpu:: PREF(int n)
{
	if((R[n] & 0xfc000000) == 0xe0000000) // is this a store queue write?
		mmu->storeQueueSend(R[n]);
	PC+=2;
}

void SHCpu:: OR(int m, int n)
{
	R[n] |= R[m];
	PC+=2;
}

void SHCpu:: ORI(Byte i)
{
	R[0] |= (Dword) i;
	PC+=2;
}

void SHCpu:: ORM(Byte i)
{
	mmu->writeByte(GBR+R[0], ((Dword) mmu->readByte(R[0]+GBR)) | (Dword) i);
	PC+=2;
}

void SHCpu:: OCBWB(int n)
{
	// XXX: should this do something?
	PC+=2;
}

void SHCpu:: OCBP(int n)
{
	// XXX: should this do something?
	PC+=2;
}

void SHCpu:: OCBI(int n)
{
	// XXX: should this do something?
	PC+=2;
}

void SHCpu:: NOT(int m, int n)
{
	R[n]= ~R[m];
	PC+=2;
}

void SHCpu:: NOP()
{
	PC+=2;
}

void SHCpu:: NEGC(int m, int n)
{
	Dword temp;
	temp=0-R[m];
	R[n]=temp - T;
	if (temp>0)
		T=1;
	else
		T=0;
	if(temp<R[n])
		T=1;
	PC+=2;
}	

void SHCpu:: NEG(int m, int n)
{
	R[n]=0-R[m];
	PC+=2;
}

void SHCpu:: MULU(int m, int n)
{
	MACL=((Dword)(Word)R[n]*(Dword)(Word)R[m]);
	PC+=2;
}

void SHCpu:: MULS(int m, int n)
{
	MACL=((Sdword)(Sword)R[n]*(Sdword)(Sword)R[m]);
	PC+=2;
}

void SHCpu:: MULL(int m, int n)
{
	MACL=R[m]*R[n];
	PC+=2;
}

void SHCpu:: MOVT(int n)
{
	R[n]=(SR&1);
	PC+=2;
}

void SHCpu:: MOVCAL(int n)
{
	mmu->writeDword(R[n], R[0]);
	PC+=2;
}

void SHCpu:: MOVA(Byte d)
{
	Dword dest;
	dest = ((d&0xff)<<2)+4+(PC&0xfffffffc);
	R[0]=dest;
	PC+=2;
}

void SHCpu:: MOVBS4(Byte d, int n)
{
	Dword dest;
	dest=(d&0xf)+R[n];
	mmu->writeByte(dest, R[0]);
	PC+=2;
}

void SHCpu:: MOVWS4(Byte d, int n)
{
	Dword dest;
	dest=((d&0xf)<<1)+R[n];
	mmu->writeWord(dest, R[0]);
	PC+=2;
}

void SHCpu:: MOVLS4(int m, Byte d, int n)
{
	Dword dest;
	dest=((d&0xf)<<2)+R[n];
	mmu->writeDword(dest, R[m]);
	PC+=2;
}

void SHCpu:: MOVBL4(int m, Byte d)
{
	Dword dest;
	dest = (d&0xf)+R[m];
	R[0] = (Sdword) (Sbyte) mmu->readByte(dest);
	PC+=2;
}

void SHCpu:: MOVWL4(int m, Byte d)
{
	Dword dest;
	dest = ((d&0xf)<<1)+R[m];
	R[0] = (Sdword) (Sword) mmu->readWord(dest);
	PC+=2;
}

void SHCpu:: MOVLL4(int m, Byte d, int n)
{
	Dword dest;
	dest=((d&0xf)<<2)+R[m];
	R[n]=mmu->readDword(dest);
	PC+=2;
}
	

void SHCpu:: MOVBLG(Byte d)
{
	Dword dest;
	dest = (Dword)(d&0xff)+GBR;
	R[0]=(Sdword)(Sbyte)mmu->readByte(dest);
	PC+=2;
}

void SHCpu:: MOVWLG(Byte d)
{
	Dword dest;
	dest = ((Dword)(d&0xff)<<1)+GBR;
	R[0]=(Sdword)(Sword)mmu->readWord(dest);
	PC+=2;
}

void SHCpu:: MOVLLG(Byte d)
{
	Dword dest;
	dest=((Dword)(d&0xff)<<2)+GBR;
	R[0]=mmu->readDword(dest);
	PC+=2;
}

void SHCpu:: MOVBSG(Byte d)
{
	Dword dest;
	dest = (Dword)(d&0xff)+GBR;
	mmu->writeByte(dest, (Byte)R[0]);
	PC+=2;
}

void SHCpu:: MOVWSG(Byte d)
{
	Dword dest;
	dest = ((Dword)(d&0xff)<<1)+GBR;
	mmu->writeWord(dest, (Word)R[0]);
	PC+=2;
}

void SHCpu:: MOVLSG(Byte d)
{
	Dword dest;
	dest = ((Dword)(d&0xff)<<2)+GBR;
	mmu->writeDword(dest, R[0]);
	PC+=2;
}

void SHCpu:: MOVI(Byte i, int n)
{
	R[n] = (Sdword) (Sbyte) i;
	PC+=2;
}

void SHCpu:: MOVWI(Byte d, int n)
{
	Dword dest;
	
	dest = (((Dword)(d&0xff)) << 1)+4+PC;
	R[n]=(Sdword) (Sword) mmu->readWord(dest);
	PC+=2;
}

void SHCpu:: MOVLI(Byte d, int n)
{
	Dword dest;
	
	dest = (((Dword)(d&0xff)) << 2)+4+(PC&0xfffffffc);
	R[n]=mmu->readDword(dest);
	PC+=2;
}

void SHCpu:: MOV(int m, int n)
{
	R[n]=R[m];
	PC+=2;
}

void SHCpu:: MOVBS(int m, int n) // mov.b Rm, @Rn
{
	mmu->writeByte(R[n], (Byte)R[m]);
	PC+=2;
}

void SHCpu:: MOVWS(int m, int n)
{
	mmu->writeWord(R[n], (Word)R[m]);
	PC+=2;
}

void SHCpu:: MOVLS(int m, int n)
{
	mmu->writeDword(R[n], R[m]);
	PC+=2;
}

void SHCpu:: MOVBL(int m, int n) // mov.b @Rm, Rn
{
	R[n]=(Sdword) (Sbyte) mmu->readByte(R[m]); // sign-extend
	PC+=2;
}

void SHCpu:: MOVWL(int m, int n) // mov.w @Rm, Rn
{
	R[n]=(Sdword) (Sword) mmu->readWord(R[m]); // sign-extend
	PC+=2;
}

void SHCpu:: MOVLL(int m, int n) // mov.l @Rm, Rn
{
	R[n]= mmu->readDword(R[m]);
	PC+=2;
}

void SHCpu:: MOVBM(int m, int n) // mov.b Rm, @-Rn
{
	mmu->writeByte(R[n]-1, (Byte) R[m]);
	R[n]--;
	PC+=2;
}

void SHCpu:: MOVWM(int m, int n)
{
	mmu->writeWord(R[n]-2, (Word) R[m]);
	R[n]-=2;
	PC+=2;
}

void SHCpu:: MOVLM(int m, int n)
{
	mmu->writeDword(R[n]-4, R[m]);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: MOVBP(int m, int n) // mov.b @Rm+, Rn
{
	R[n]=(Sdword) (Sbyte) mmu->readByte(R[m]); // sign-extend
	if(n!=m) R[m]++;
	PC+=2;
}

void SHCpu:: MOVWP(int m, int n) // mov.w @Rm+, Rn
{
	R[n]=(Sdword) (Sword) mmu->readWord(R[m]); // sign-extend
	if(n!=m) R[m]+=2;
	PC+=2;
}

void SHCpu:: MOVLP(int m, int n) // mov.l @Rm+, Rn
{
	R[n]= mmu->readDword(R[m]);
	if(n!=m) R[m]+=4;
	PC+=2;
}

void SHCpu:: MOVBS0(int m, int n) // mov.b Rm, @(r0, Rn)
{
	mmu->writeByte(R[n]+R[0], R[m]);
	PC+=2;
}

void SHCpu:: MOVWS0(int m, int n) // mov.w Rm, @(r0, Rn)
{
	mmu->writeWord(R[n]+R[0], R[m]);
	PC+=2;
}

void SHCpu:: MOVLS0(int m, int n) // mov.l Rm, @(r0, Rn)
{
	mmu->writeDword(R[n]+R[0], R[m]);
	PC+=2;
}

void SHCpu:: MOVBL0(int m, int n) // mov.b @(r0, rm), rn
{
	R[n] = (Sdword) (Sbyte) mmu->readByte(R[0]+R[m]);
	PC+=2;
}

void SHCpu:: MOVWL0(int m, int n) // mov.w @(r0, rm), rn
{
	R[n] = (Sdword) (Sword) mmu->readWord(R[0]+R[m]);
	PC+=2;
}

void SHCpu:: MOVLL0(int m, int n) // mov.l @(r0, rm), rn
{
	R[n] = mmu->readDword(R[0]+R[m]);
	PC+=2;
}

void SHCpu:: MACW(int m, int n)
{
	Sdword tm, tn, d,s,a;
	Dword t1;
	tn=(Sdword)mmu->readWord(R[n]);
	tm=(Sdword)mmu->readWord(R[m]);
	R[n]+=2;
	R[m]+=2;
	t1=MACL;
	tm=((Sdword)(Sword)tn*(Sdword)(Sword)tm);
	if((Sdword)MACL>=0)d=0;else d=1;
	if((Sdword)tm>=0){s=0;tn=0;}else{s=1;tn=0xffffffff;}
	s+=d;
	MACL+=tm;
	if((Sdword)MACL>=0)a=0;else a=1;
	a+=d;
	if(S==1)
	{
		if(a==1)
		{
			if(s==0)MACL=0x7fffffff;
			if(s==2)MACL=0x80000000;
		}
	}
	else
	{
		MACH+=tn;
		if(t1>MACL) MACH+=1;
	}
	PC+=2;
}

void SHCpu:: DO_MACL(int m, int n)
{
/*
	Dword rnl, rnh, rml, rmh, r0, r1, r2, t0, t1, t2, t3;
	Sdword tm, tn, fnlml;
	tn=(Sdword)mmu->readDword(R[n]);
	tm=(Sdword)mmu->readDword(R[m]);
	R[n]+=4;
	R[m]+=4;
	if((Sdword)(tn^tm)<0) fnlml=-1; else fnlml=0;
	if(tn<0) tn=0-tn;
	if(tm<0) tm=0-tm;
	t1=(Dword)tn;
	t2=(Dword)tm;
	rnl=t1&0xffff;
	rnh=(t1>>16)&0xffff;
	rml=t2&0xffff;
	rmh=(t2>>16)&0xffff;
	t0=rml*rnl;
	t1=rmh*rnl;
	t2=rml*rnh;
	t3=rmh*rnh;
	r2=0;
	r1=t1+t2;
	if(r1<t1)r2+=0x10000;
	t1=(r1<<16)&0xffff0000;
	r0=t0+t1;
	if(r0<t0)r2++;
	r2=r2+((r1>>16)&0xffff)+t3;
	if(fnlml<0)
	{
		r2=~r2;
		if(r0==0)r2++;
		else r0=(~r0)+1;
	}
	if(S==1)
	{
		r0=MACL+r0;
		if(MACL>r0)r2++;
		if(MACH&0x8000);
		else r2+=MACH|0xffff0000;
		r2+=MACH&0x7fff;
		if(((Sdword)r2<0)&&(r2<0xffff8000))
		{
			r2=0xffff8000;
			r0=0;
		}
		if(((Sdword)r2>0)&&(r2>0x7fff))
		{
			r2=0x7fff;

			r0=0xffffffff;
		}
		MACH=(r2&0xffff)|(MACH&0xffff0000);
		MACL=r0;
	}
	else
	{
		r0=MACL+r0;
		if(MACL>r0)r2++;
		r2+=MACH;
		MACH=r2;
		MACL=r0;
	}
	PC+=2;
	*/

	debugger->flamingDeath("Untested MACL implementation invoked");
	shcpu_DO_MACL(R[n], R[m], &MACH, &MACL);

	if(S==1)
	{
		// do saturation at bit 48
		switch(Overlord::bits(MACH, 31, 16))
		{
		case 0x0000:
		case 0xffff:
			break;
		default:
			if(MACH >> 31) // sign bit on?
				MACH |= 0xffff0000;
			else
				MACH &= 0x0000ffff;
		}
	}
	R[m]+=4;
	R[n]+=4;
	PC+=2;
}


void SHCpu:: LDSFPSCR(int n)
{
	setFPSCR(R[n] & 0x003fffff);
	PC+=2;
}

void SHCpu::LDSMFPSCR(int n)
{
	setFPSCR(mmu->readDword(R[n]) & 0x003fffff);
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDSFPUL(int n)
{
	FPUL = R[n];
	PC+=2;	
}

void SHCpu::LDSMFPUL(int n)
{
	FPUL = mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSMPR(int n)
{
	PR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSMMACL(int n)
{
	MACL=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSMMACH(int n)
{
	MACH=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSPR(int n)
{
	PR=R[n];
	PC+=2;
}

void SHCpu:: LDSMACL(int n)
{
	MACL=R[n];
	PC+=2;
}

void SHCpu:: LDSMACH(int n)
{
	MACH=R[n];
	PC+=2;
}

void SHCpu:: LDCSR(int n) // privileged
{
	setSR(R[n]&0x700083f3);
	PC+=2;
}

void SHCpu:: LDCGBR(int n)
{
	GBR=R[n];
	PC+=2;
}

void SHCpu:: LDCVBR(int n) // privileged
{
	VBR=R[n];
	PC+=2;
}

void SHCpu:: LDCSSR(int n) // privileged
{
	SSR=R[n];
	PC+=2;
}

void SHCpu:: LDCSPC(int n) // privileged
{
	SPC=R[n];
	PC+=2;
}

void SHCpu:: LDCDBR(int n) // privileged
{
	DBR=R[n];
	PC+=2;
}

void SHCpu:: LDCRBANK(int m, int n) // privileged
{
	// n = source, m = dest
	RBANK[m]=R[n];
	PC+=2;
}

void SHCpu:: LDCMRBANK(int m, int n) // privileged
{
	// n = source, m = dest
	RBANK[m]=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMSR(int n)
{
	setSR(mmu->readDword(R[n])&0x700083f3);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMGBR(int n)
{
	GBR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMVBR(int n)
{
	VBR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}


void SHCpu:: LDCMSSR(int n)
{
	SSR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMSPC(int n)
{
	SPC=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMDBR(int n)
{
	DBR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: JSR(int n)
{
	Dword temp, oldpc;
	temp = R[n];
	oldpc = PC;
	delaySlot();
	PR=oldpc+4;
	
	debugger->reportBranch("jsr", PC, temp);
	PC=temp;
}

void SHCpu:: JMP(int n)
{
	Dword temp;
	temp = R[n];
	delaySlot();
	
	debugger->reportBranch("jmp", PC, temp);
	PC=temp;
}	

void SHCpu:: DMULU(int m, int n)
{
	Dword rnl, rnh, rml, rmh, r0, r1, r2, t0, t1, t2, t3;
	rnl=R[n]&0xffff;
	rnh=(R[n]>>16)&0xffff;
	rml=R[m]&0xffff;
	rmh=(R[m]>>16)&0xffff;
	t0=rml*rnl;
	t1=rmh*rnl;
	t2=rml*rnh;
	t3=rmh*rnh;
	r2=0;
	r1=t1+t2;
	if(r1<t1)r2+=0x10000;
	t1=(r1<<16)&0xffff0000;
	r0=t0+t1;
	if(r0<t0)r2++;
	r2=r2+((r1>>16)&0xffff)+t3;
	MACH=r2;
	MACL=r0;
	PC+=2;
}

void SHCpu:: EXTUW(int m, int n)
{
	R[n] = R[m] & 0xffff;
	PC+=2;
}

void SHCpu:: EXTUB(int m, int n)
{
	R[n] = R[m] & 0xff;
	PC+=2;
}

void SHCpu:: EXTSB(int m, int n)
{
	R[n]=	((R[m]&0x80)==0) ? R[m]&0xff : R[m] | 0xffffff00;
	PC+=2;
}

void SHCpu:: EXTSW(int m, int n)
{
	R[n]=	((R[m]&0x8000)==0) ? R[m]&0xffff : R[m] | 0xffff0000;
	PC+=2;
}

void SHCpu:: DT(int n)
{
	R[n]--;
	if(R[n]==0)
		T=1;
	else
		T=0;
	PC+=2;
}

void SHCpu:: DMULS(int m, int n)
{
	Dword rnl, rnh, rml, rmh, r0, r1, r2, t0, t1, t2, t3;
	Sdword tm, tn, fnlml;
	
	tn=(Sdword)R[n];
	tm=(Sdword)R[m];
	if(tn<0) tn=0-tn;
	if(tm<0) tm=0-tm;
	if((Sdword)(R[n]^R[m])<0) fnlml=-1; else fnlml=0;
	t1=(Dword) tn;
	t2=(Dword) tm;
	rnl=t1&0xffff;
	rnh=(t1>>16)&0xffff;
	rml=t2&0xffff;
	rmh=(t2>>16)&0xffff;
	t0=rml*rnl;
	t1=rmh*rnl;
	t2=rml*rnh;
	t3=rmh*rnh;
	r2=0;
	r1=t1+t2;
	if(r1<t1) r2+=0x10000;
	t1=(r1<<16)&0xffff0000;
	r0=t0+t1;
	if(r0<t0) r2++;
	r2+=((r1>>16)&0xffff)+t3;
	if(fnlml<0)
	{
		r2=~r2;
		if(r0==0)
			r2++;
		else
			r0=(~r0)+1;
	}
	MACH=r2;
	MACL=r0;
	PC+=2;
}

void SHCpu:: DIV1(int m, int n)
{
	Dword t0, t2;
	Byte oq, t1;
	oq=Q;
	Q=(Byte)((0x80000000 & R[n])!=0);
	t2=R[m];
	R[n]<<=1;
	R[n]|=(Dword)T;
	switch(oq)
	{
	case 0:
		switch(M)
		{
		case 0:
			t0=R[n];
			R[n]-=t2;
			t1=(R[n]>t0);
			switch(Q)
			{
			case 0: Q=t1; break;
			case 1: Q=(Byte)(t1==0); break;
			}
			break;
		case 1:
			t0=R[n];
			R[n]+=t2;
			t1=(R[n]<t0);
			switch(Q)
			{
			case 0:Q=(Byte)(t1==0); break;
			case 1:Q=t1;
			}
			break;
		}
		break;
	case 1:
		switch(M)
		{
			case 0:
				t0=R[n];
				R[n]+=t2;
				t1=(R[n]<t0);
				switch(Q)
				{
				case 0: Q=t1; break;
				case 1: Q=(Byte)(t1==0); break;
				}
				break;
			case 1:
				t0=R[n];
				R[n]-=t2;
				t1=(R[n]>t0);
				switch(Q)
				{
				case 0: Q=(Byte)(t1==0); break;
				case 1: Q=t1; break;
				}
				break;
		}
		break;
	}
	T=(Q==M);
	PC+=2;
}

void SHCpu:: DIV0U()
{
	setSR(SR & ((~F_SR_M)&(~F_SR_Q)&(~F_SR_T)));
	PC+=2;
}

void SHCpu:: DIV0S(int m, int n)
{
	
	if((R[n] & 0x80000000) == 0)
		setSR(SR & ~F_SR_Q);
	else
		setSR(SR | F_SR_Q);

	if((R[m] & 0x80000000) == 0)
		setSR(SR & ~F_SR_M);
	else
		setSR(SR | F_SR_M);
		
	if(!(((SR&F_SR_M)!=0)==((SR&F_SR_Q)!=0)))
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPIM(Byte i)
{
	if((Sdword)R[0] == (Sdword)(Sbyte)i)
		setSR(SR|F_SR_T);
	else
		setSR(SR& ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPSTR(int m, int n)
{
	Dword foo;
	foo = R[m] ^ R[n];
	if
	(
		(
			((foo&0xff000000)>>24) &&
			((foo&0xff0000)>>16) &&
			((foo&0xff00)>>8) &&
			(foo&0xff)
		) == 0
	)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);		
	PC+=2;
}
void SHCpu:: CMPPZ(int n)
{
	if((signed int)R[n] >= 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPPL(int n)
{
	if((signed int)R[n] > 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPHS(int m, int n)
{
	if(R[n] >= R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPHI(int m, int n)
{
	if(R[n] > R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPGT(int m, int n)
{
	if((Sdword)R[n] > (Sdword)R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPGE(int m, int n)
{
	if((Sdword)R[n] >= (Sdword)R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPEQ(int m, int n)
{
	if(R[n] == R[m])
		T=1;
	else
		T=0;
	PC+=2;
}

void SHCpu:: CLRT()
{
	setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CLRS()
{
	setSR(SR & ~F_SR_S);
	PC+=2;
}

void SHCpu:: CLRMAC()
{
	MACH=MACL=0;
	PC+=2;
}

void SHCpu:: BTS(Byte d)
{
	Dword dest, oldpc;
	dest = PC + 4 + (((Sdword) (Sbyte) d) * 2);
	oldpc = PC;
	if((SR & F_SR_T)==1)
	{
		delaySlot();
		debugger->reportBranch("bts", PC, dest);
		PC=dest;
	}
	else
	{
		delaySlot();
		PC=oldpc+4;
	}
}

void SHCpu:: BT(Byte d) // no delay slot!
{
	Dword dest;
	dest = PC + 4 + (((Sdword) (Sbyte) d) * 2);
	if((SR & F_SR_T)==1)
	{
		debugger->reportBranch("bt", PC, dest);
		PC=dest;
	}
	else
		PC+=2;
}

void SHCpu:: BSRF(int n)
{
	Dword dest, oldpc;
	dest = PC+4+(Sdword)R[n];
	oldpc = PC;
	delaySlot();
	PR=oldpc+4;
	debugger->reportBranch("bsrf", PC, dest);
	PC=dest;
}

void SHCpu:: BSR(Sword d)
{
	Dword dest, oldpc;
	// must do sign-extension ourselves here because it's a
	// 12-bit quantity
	if((d & 0x800) != 0)
		d|=0xfffff000;
	dest = PC+4+(Sdword)(Sword)(d<<1);
	oldpc=PC;
	delaySlot();
	PR=oldpc+4;
	debugger->reportBranch("bsr", PC, dest);
	PC=dest;
}

void SHCpu:: BRAF(int n)
{
	Dword dest;
	dest = PC+4+(Sdword)R[n];
	delaySlot();
	debugger->reportBranch("braf", PC, dest);
	PC=dest;
}

void SHCpu:: BRA(Sword d)
{
	Dword dest;
	// must do sign-extension ourselves here because it's a
	// 12-bit quantity
	if((d & 0x800) != 0)
		d|=0xfffff000;
	dest = PC+4+(Sdword)(Sword)(d<<1);
	delaySlot();
	debugger->reportBranch("bra", PC, dest);
	PC=dest;
}

void SHCpu:: BFS(Byte d)
{
	Dword dest, oldpc;
	dest = PC + 4 + (((Sdword) (Sbyte) d) * 2);
	oldpc = PC;
	if((SR & F_SR_T)==0)
	{
		delaySlot();
		debugger->reportBranch("bfs", PC, dest);
		PC=dest;
	}
	else
	{
		delaySlot();
		PC=oldpc+4;
	}
}

void SHCpu:: BF(Byte d) // no delay slot!
{
	Dword dest;
	dest = PC + 4 + (((Sdword) (Sbyte) d) * 2);
	if((SR & F_SR_T)==0)
	{
		debugger->reportBranch("bf", PC, dest);
		PC=dest;
	}
	else
		PC+=2;
}

void SHCpu:: AND(int m, int n)
{
	R[n] &= R[m];
	PC+=2;
}

void SHCpu:: ANDI(Byte i)
{
	R[0] &= (Dword) i;
	PC+=2;
}

void SHCpu:: ANDM(Byte i) // and.b #imm, @(r0, gbr)
{
	mmu->writeByte(R[0]+GBR, ((Dword) i) & (Dword) mmu->readByte(R[0]+GBR));
	PC+=2;
}

void SHCpu:: ADDV(int m, int n)
{
	Dword d=1,s=1,a=1;
	
	
	if((Sdword)R[n] >= 0)
		d=0;
	if((Sdword)R[m] >= 0)
		s=0;
	s+=d;
	R[n]+=R[m];
	if((Sdword)R[n]>=0)
		a=0;
	a+=d;
	if(s==0||s==2)
	{
		if(a==1)
			setSR(SR | F_SR_T);
		else
			setSR(SR & ~F_SR_T);
	}
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}		

void SHCpu:: ADD(int m, int n)
{
	R[n] += R[m];
	PC+=2;
}

void SHCpu:: ADDI(Byte i, int n)
{
	R[n] += (Sdword) (Sbyte) i; // sign-extend
	PC+=2;
}

void SHCpu:: ADDC(int m, int n)
{
	Dword foo;
	foo = R[n] + R[m] + (SR & F_SR_T);
	// figure out carry
	if( (R[n] > (R[n]+R[m])) || ((R[n]+R[m]) > (R[n]+R[m]+(SR&F_SR_T))) )	
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	R[n] = foo;
	PC+=2;
}

void SHCpu::FABS(int n)
{
	FPU_DP_FIX_N();

	if(FPU_DP()) // double-precision
		DR[n] = fabs(DR[n]);
		//*(((Qword*)DR)+n) &= 0x7fffffffffffffff;
	else // single-precision
		//FR[n] = (float) fabs((double) FR[n]);
		*(((Dword*) FR)+n) &= 0x7fffffff;
	PC+=2;
}

void SHCpu::FADD(int m, int n)
{
//	FPU_DP_FIX_MN();
	
	if(FPU_DP()) // double-precision
	{
		cnv_dbl tmpa, tmpb;
		tmpa.i[1] = FR_Dwords[n];
		tmpa.i[0] = FR_Dwords[n+1];
		tmpb.i[1] = FR_Dwords[m];
		tmpb.i[0] = FR_Dwords[m+1];
		printf("FADD: %f * %f = ", tmpa.d, tmpb.d);
		tmpa.d += tmpb.d;
		printf("%f\n", tmpa.d);
		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
//		DR[n]+=DR[m];
	} else // single-precision
		FR[n]+=FR[m];
	PC+=2;
}

void SHCpu::FMUL(int m, int n)
{
//	FPU_DP_FIX_MN();
	
	if(FPU_DP()) // double-precision
	{
		cnv_dbl tmpa, tmpb;
		tmpa.i[1] = FR_Dwords[n];
		tmpa.i[0] = FR_Dwords[n+1];
		tmpb.i[1] = FR_Dwords[m];
		tmpb.i[0] = FR_Dwords[m+1];
		printf("FMUL: %f * %f = ", tmpa.d, tmpb.d);
		tmpa.d *= tmpb.d;
		printf("%f\n", tmpa.d);
		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
//		DR[n]*=DR[m];
	}
	else // single-precision
	{
//		shfpu_sop0 = FR[n];
//		shfpu_sop1 = FR[m];
		FR[n]*=FR[m];
//		shfpu_sFMUL();
		FR[n] = shfpu_sresult;
	}
	PC+=2;
}

// sets the FPSCR register and does whatever whatever its contents dictate
// should be done--such as switching register banks and the like
void SHCpu::setFPSCR(Dword d)
{
	FPSCR = d;

	if(d & F_FPSCR_FR)
	{
		// bank 1 selected
		this->FR = this->FPR_BANK1;
		this->XF = this->FPR_BANK0;
		this->DR = (double *) this->FPR_BANK1;
		this->XD = (double *) this->FPR_BANK0;
	}
	else
	{
		// bank 0 selected
		this->FR = this->FPR_BANK0;
		this->XF = this->FPR_BANK1;
		this->DR = (double *) this->FPR_BANK0;
		this->XD = (double *) this->FPR_BANK1;
	}

	FR_Dwords = (Dword*)FR;
//	shfpu_setContext(FR, XF, (float*)&FPUL, &FPSCR);
}

void SHCpu::FMOV(int m, int n)
{
	FPU_SZ_FIX_MN();

	if(FPU_SZ())
		DR[n]=DR[m];
	else
		FR_Dwords[n] = FR_Dwords[m];
		//FR[n]=FR[m];
	PC+=2;
}

void SHCpu::FMOV_STORE(int m, int n)
{
	FPU_SZ_FIX_M();

	if(FPU_SZ()) // double-precision
		mmu->writeDouble(R[n], DR[m]);
	else // single-precision
		mmu->writeFloat(R[n], FR[m]);
	PC+=2;
}

void SHCpu::FMOV_LOAD(int m, int n)
{
	FPU_SZ_FIX_N();

	if(FPU_SZ()) // double-precision
		DR[n] = mmu->readDouble(R[m]);
	else // single-precision
		FR[n] = mmu->readFloat(R[m]);
	PC+=2;
}

void SHCpu::FMOV_RESTORE(int m, int n)
{
	FPU_SZ_FIX_N();
	if(FPU_SZ()) // double-precision
	{
		DR[n] = mmu->readDouble(R[m]);
		R[m]+=8;
	}
	else // single-precision
	{
		FR[n] = mmu->readFloat(R[m]);
		R[m]+=4;
	}
	PC+=2;
}

void SHCpu::FMOV_SAVE(int m, int n)
{
	FPU_SZ_FIX_M();

	if(FPU_SZ()) // double-precision
	{
		R[n]-=8;
		mmu->writeDouble(R[n], DR[m]);
	}
	else // single-precision
	{
		R[n]-=4;
		mmu->writeFloat(R[n], FR[m]);
	}
	PC+=2;
}

void SHCpu::FMOV_INDEX_LOAD(int m, int n)
{
	FPU_SZ_FIX_N();

	if(FPU_SZ()) // double-precision
		DR[n] = mmu->readDouble(R[m]+R[0]);
	else // single-precision
		FR[n] = mmu->readFloat(R[m]+R[0]);
	PC+=2;
}

void SHCpu::FMOV_INDEX_STORE(int m, int n)
{
	FPU_SZ_FIX_M();
	
	if(FPU_SZ()) // double-precision
		mmu->writeDouble(R[0]+R[n], DR[m]);
	else // single-precision
		mmu->writeFloat(R[0]+R[n], FR[m]);
	PC+=2;
}

void SHCpu::FCMPEQ(int m, int n)
{
	FPU_DP_FIX_MN();

	if(FPU_DP()) // double-precision
	{
		if(DR[m] == DR[n])
			setSR(SR | F_SR_T);
	}
	else // single-precision
	{
		if(FR[m] == FR[n])
			setSR(SR | F_SR_T);
	}
	PC+=2;
}

void SHCpu::FCMPGT(int m, int n)
{
	FPU_DP_FIX_MN();

	if(FPU_DP())
	{
		// double-precision
		if(DR[n] > DR[m])
			setSR(SR | F_SR_T);
	}
	else
	{
		// single-precision
		if(FR[n] > FR[m]) setSR(SR | F_SR_T);
	}
	PC+=2;
}

void SHCpu::FCNVSD(int n)
{
	FPU_DP_FIX_N();

	DR[n] = (double) FPUL;
	PC+=2;
}

void SHCpu::FDIV(int m, int n)
{
//	FPU_DP_FIX_MN();

	if(FPSCR & F_FPSCR_PR)
	{
		cnv_dbl tmpa, tmpb;
		tmpa.i[1] = FR_Dwords[n];
		tmpa.i[0] = FR_Dwords[n+1];
		tmpb.i[1] = FR_Dwords[m];
		tmpb.i[0] = FR_Dwords[m+1];
		printf("FDIV: %f / %f = ", tmpa.d, tmpb.d);
		tmpa.d /= tmpb.d;
		printf("%f\n", tmpa.d);
		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
//		DR[n] /= DR[m];
	}
	else
		FR[n] /= FR[m];
	PC+=2;
}

void SHCpu::FLDI0(int n)
{

	FR[n] = 0.0f;
	PC+=2;
}

void SHCpu::FLDI1(int n)
{
	FR[n] = 1.0f;
	PC+=2;
}

void SHCpu::FLDS(int n)
{
	FPUL = FR_Dwords[n];
	PC+=2;
}

void SHCpu::FLOAT(int n)
{
//	FPU_DP_FIX_N();
	if(FPU_DP()) {
		cnv_dbl tmpa;
		tmpa.d = (double)*((signed int*)&FPUL);
		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
//		DR[n] = (double)*((float*)(signed int*)&FPUL);
//		DR[n] = (double)*((signed int*)&FPUL);
	}
	else
	{
//		shfpu_sFLOAT();
//		FR[n] = shfpu_sresult;
		FR[n] = (float)*((signed int*)&FPUL);
	}
	PC+=2;
}

void SHCpu::FMAC(int m, int n)
{
	FR[n] = (FR[0] * FR[m]) + FR[n];
	PC+=2;
}

void SHCpu::FMOV_STORE_XD(int m, int n)
{
	FPU_DP_FIX_M();

	mmu->writeDouble(R[n], XD[m]);
	PC+=2;
}

void SHCpu::FMOV_LOAD_XD(int m, int n)
{
	FPU_DP_FIX_N();

	XD[n] = mmu->readDouble(R[m]);
	PC+=2;
}

void SHCpu::FMOV_RESTORE_XD(int m, int n)
{
	FPU_DP_FIX_N();

	XD[n] = mmu->readDouble(R[m]);
	R[m]+=8;
	PC+=2;
}

void SHCpu::FMOV_SAVE_XD(int m, int n)
{
	FPU_DP_FIX_M();

	R[n]-=8;
	mmu->writeDouble(R[n], XD[m]);
	PC+=2;
}


void SHCpu::FMOV_INDEX_LOAD_XD(int m, int n)
{
	FPU_DP_FIX_N();

	XD[n] = mmu->readDouble(R[0]+R[m]);
	PC+=2;
}


void SHCpu::FMOV_INDEX_STORE_XD(int m, int n)
{
	FPU_DP_FIX_M();

	mmu->writeDouble(R[0]+R[n], XD[m]);
	PC+=2;
}


void SHCpu::FMOV_XDXD(int m, int n)
{
	FPU_DP_FIX_MN();
	XD[n] = XD[m];
	PC+=2;
}

void SHCpu::FMOV_XDDR(int m, int n)
{
	FPU_DP_FIX_MN();
	DR[n] = XD[m];
	PC+=2;
}

void SHCpu::FMOV_DRXD(int m, int n)
{
	FPU_DP_FIX_MN();
	XD[n] = DR[m];
	PC+=2;
}

void SHCpu::FCNVDS(int n)
{
	FPU_DP_FIX_N();
	if(FPU_DP())
		FPUL = (float) DR[n];
	else
		debugger->flamingDeath("FCNVDS: Can't access DR regs when double-prec is off");
	PC+=2;
}

void SHCpu::FNEG(int n)
{
	FPU_DP_FIX_N();
	if(FPU_DP())
		DR[n] = -DR[n];
	else
		FR[n] = -FR[n];
	PC+=2;
}

void SHCpu::FRCHG()
{
	setFPSCR(FPSCR ^ 0x00200000);
	PC+=2;
}

void SHCpu::FSCHG()
{
	setFPSCR(FPSCR ^ 0x00100000);
	PC+=2;
}

void SHCpu::FSQRT(int n)
{
	FPU_DP_FIX_N();
	if(FPU_DP())
		DR[n] = sqrt(DR[n]);
	else
		FR[n] = (float) sqrt((double) FR[n]);
	PC+=2;
}

void SHCpu::FSTS(int n)
{
	FR_Dwords[n] = FPUL;
	PC+=2;
}

void SHCpu::FSUB(int m, int n)
{
	FPU_DP_FIX_MN();
	if(FPU_DP())
		DR[n]-=DR[m];
	else
		FR[n]-=FR[m];
	PC+=2;

}

void SHCpu::FTRC(int n)
{
	FPU_DP_FIX_N();
	if(FPU_DP()) {
		cnv_dbl tmpa;
		tmpa.i[1] = *((unsigned int*)&FR[n<<1]);
		tmpa.i[0] = *((unsigned int*)&FR[(n<<1) + 1]);
		FPUL = (Dword)((float)tmpa.d);
//		FPUL = (float) DR[n];
	}
	else {
		FPUL = (Dword)FR[n];
//		FPUL = FR[n];
	}
	PC+=2;
}

void SHCpu::FSCA(int n)
{
	float angle = ((float)FPUL / 65535.0) * 2 * M_PI;
	FR[n] = sin(angle);
	FR[n+1] = cos(angle);
	PC+=2;
}

void SHCpu::FSRRA(int n)
{
	// FIXME: should this work on DR regs as well?
	FR[n] = 1/sqrt(FR[n]);
	PC+=2;
}

void SHCpu::setSR(Dword d)
{
	Dword *oldR = R;

	if(d & F_SR_MD)
	{
		// set the register banks
		if(d & F_SR_RB)
		{
			R = RBANK1;
			RBANK = RBANK0;
		}
		else
		{
			R = RBANK0;
			RBANK = RBANK1;
		}
	}
	else
	{
		// RB doesn't matter in user mode
		R = RBANK0;
		RBANK = RBANK1;
	}
	// Only one copy of R8-R15 exist on a real SH4, but in emulation we have two -
	// we need to keep them in sync
	if(R != oldR) // only sync the regs if the banks were switched
	{
		for(int i=8; i<16; i++)
			R[i] = RBANK[i];
	}
	SR = d;
}

void SHCpu::delaySlot()
{
	Word d;

	/*
	Slot illegal instructions:

	JMP JSR BRA BRAF BSR BSRF RTS RTE BT/S BF/S 0xFFFD
	BT BF	TRAPA MOVA
	LDC Rm, SR
	LDC.L @Rm+, SR

	PC-relative MOV instructions
	*/

	d = mmu->fetchInstruction(PC+2);
	// only check for slot illegal if we're actually executing
	/*
	// XXX: since we don't have exceptions going yet,
	// no need to try and invoke them!
	switch(d&0xf0ff)
	{
	case 0x402b: // JMP
	case 0x400b: // JSR
	case 0x0023: // BRAF
	case 0x0003: // BSRF
	case 0x400e: // LDC Rm, SR
	case 0x4007: // LDC @Rm+, SR
		exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
		return;
	}
	switch(d&0xff00)
	{
	case 0x8b00: // BF
	case 0x8f00: // BF/S
	case 0x8900: // BT
	case 0x8d00: // BT/S
	case 0xc300: // TRAPA
	case 0xc700: // MOVAa
		exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
		return;
	}
	switch(d&0xf000)
	{
	case 0xa000: // BRA
	case 0xb000: // BSR
	case 0x9000: // MOVWI
	case 0xd000: // MOVLI
		exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
		return;
	}
	switch(d)
	{
	case 0x000b: // RTS
	case 0x002b: // RTE
	case 0xfffd: // undefined instruction
		exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
		return;
	}
	*/

	executeInstruction(d);
}

void SHCpu::reset()
{
	setSR(0x70003cf0);
	PC = 0xA0000000;
	VBR = 0;
	*((Dword*)(ccnRegs+EXPEVT)) = 0;
	*((Dword*)((Dword)ccnRegs+CCR)) = 0;
	*((Dword*)((Dword)ccnRegs+MMUCR)) = 0xff000010;
	setFPSCR(0x00040001);
	exceptionsPending = 0;
}

// Executes a single instruction.
// This is a really stupid way to do this, unless the compiler
// somehow transforms this crap into reasonably fast code.
void SHCpu::executeInstruction(Word d)
{
	switch(d & 0xF000)
	{
	case 0x6000:
		switch(d & 0xF00F)
		{
			case 0x6000: MOVBL(getM(d), getN(d)); return;
			case 0x6001: MOVWL(getM(d), getN(d)); return;
			case 0x6002: MOVLL(getM(d), getN(d)); return;
			case 0x6003: MOV(getM(d), getN(d)); return;
			case 0x6004: MOVBP(getM(d), getN(d)); return;
			case 0x6005: MOVWP(getM(d), getN(d)); return;
			case 0x6006: MOVLP(getM(d), getN(d)); return;
			case 0x6007: NOT(getM(d), getN(d)); return;
			case 0x6008: SWAPB(getM(d), getN(d)); return;
			case 0x6009: SWAPW(getM(d), getN(d)); return;
			case 0x600a: NEGC(getM(d), getN(d)); return;
			case 0x600b: NEG(getM(d), getN(d)); return;
			case 0x600c: EXTUB(getM(d), getN(d)); return;
			case 0x600d: EXTUW(getM(d), getN(d)); return;
			case 0x600e: EXTSB(getM(d), getN(d)); return;
			case 0x600f: EXTSW(getM(d), getN(d)); return;
		} break;

	case 0x2000: 
		switch(d & 0xF00F)
		{
			case 0x2000: MOVBS(getM(d), getN(d)); return;
			case 0x2001: MOVWS(getM(d), getN(d)); return;
			case 0x2002: MOVLS(getM(d), getN(d)); return;
			case 0x2004: MOVBM(getM(d), getN(d)); return;
			case 0x2005: MOVWM(getM(d), getN(d)); return;
			case 0x2006: MOVLM(getM(d), getN(d)); return;
			case 0x2007: DIV0S(getM(d), getN(d)); return;
			case 0x2008: TST(getM(d), getN(d)); return;
			case 0x2009: AND(getM(d), getN(d)); return;
			case 0x200a: XOR(getM(d), getN(d)); return;
			case 0x200b: OR(getM(d), getN(d)); return;
			case 0x200c: CMPSTR(getM(d), getN(d)); return;
			case 0x200d: XTRCT(getM(d), getN(d)); return;
			case 0x200e: MULU(getM(d), getN(d)); return;
			case 0x200f: MULS(getM(d), getN(d)); return;
		} break;

	case 0x3000: 
		switch(d & 0xF00F)
		{
			case 0x3000: CMPEQ(getM(d), getN(d)); return;
			case 0x3002: CMPHS(getM(d), getN(d)); return;
			case 0x3003: CMPGE(getM(d), getN(d)); return;
			case 0x3004: DIV1(getM(d), getN(d)); return;
			case 0x3005: DMULU(getM(d), getN(d)); return;
			case 0x3006: CMPHI(getM(d), getN(d)); return;
			case 0x3007: CMPGT(getM(d), getN(d)); return;
			case 0x3008: SUB(getM(d), getN(d)); return;
			case 0x300a: SUBC(getM(d), getN(d)); return;
			case 0x300b: SUBV(getM(d), getN(d)); return;
			case 0x300c: ADD(getM(d), getN(d)); return;
			case 0x300d: DMULS(getM(d), getN(d)); return;
			case 0x300e: ADDC(getM(d), getN(d)); return;
			case 0x300f: ADDV(getM(d), getN(d)); return;
		} break;

	case 0x8000: 
		switch(d & 0xFF00)
		{
		case 0x8000: MOVBS4(getI(d), getM(d)); return;
		case 0x8100: MOVWS4(getI(d), getM(d)); return;
		case 0x8400: MOVBL4(getM(d), getI(d)); return;
		case 0x8500: MOVWL4(getM(d), getI(d)); return;
		case 0x8800: CMPIM(getI(d)); return;
		case 0x8900: BT(getI(d)); return;
		case 0x8b00: BF(getI(d)); return;
		case 0x8d00: BTS(getI(d)); return;
		case 0x8f00: BFS(getI(d)); return;
		} break;

	case 0xc000: 
		switch(d & 0xFF00)
		{
		case 0xc000: MOVBSG(getI(d)); return;
		case 0xc100: MOVWSG(getI(d)); return;
		case 0xc200: MOVLSG(getI(d)); return;
		case 0xc300: TRAPA(getI(d)); return;
		case 0xc400: MOVBLG(getI(d)); return;
		case 0xc500: MOVWLG(getI(d)); return;
		case 0xc600: MOVLLG(getI(d)); return;
		case 0xc700: MOVA(getI(d)); return;
		case 0xc800: TSTI(getI(d)); return;
		case 0xc900: ANDI(getI(d)); return;
		case 0xca00: XORI(getI(d)); return;
		case 0xcb00: ORI(getI(d)); return;
		case 0xcc00: TSTM(getI(d)); return;
		case 0xcd00: ANDM(getI(d)); return;
		case 0xce00: XORM(getI(d)); return;
		case 0xcf00: ORM(getI(d)); return;
		} break;

	case 0x4000: 
		switch(d & 0xF0FF)
		{
		case 0x4000: SHLL(getN(d)); return;
		case 0x4010: DT(getN(d)); return;
		case 0x4020: SHAL(getN(d)); return;
		case 0x4001: SHLR(getN(d)); return;
		case 0x4011: CMPPZ(getN(d)); return;
		case 0x4021: SHAR(getN(d)); return;
		case 0x4002: STSMMACH(getN(d)); return;
		case 0x4012: STSMMACL(getN(d)); return;
		case 0x4022: STSMPR(getN(d)); return;
		case 0x4032: STCMSGR(getN(d)); return;
		case 0x4052: STSMFPUL(getN(d)); return;
		case 0x4062: STSMFPSCR(getN(d)); return;
		case 0x40f2: STCMDBR(getN(d)); return;
		case 0x4003: STCMSR(getN(d)); return;
		case 0x4013: STCMGBR(getN(d)); return;
		case 0x4023: STCMVBR(getN(d)); return;
		case 0x4033: STCMSSR(getN(d)); return;
		case 0x4043: STCMSPC(getN(d)); return;
		case 0x4004: ROTL(getN(d)); return;
		case 0x4024: ROTCL(getN(d)); return;
		case 0x4005: ROTR(getN(d)); return;
		case 0x4015: CMPPL(getN(d)); return;
		case 0x4025: ROTCR(getN(d)); return;
		case 0x4056: LDSMFPUL(getN(d)); return;
		case 0x4066: LDSMFPSCR(getN(d)); return;
		case 0x4006: LDSMMACH(getN(d)); return;
		case 0x4016: LDSMMACL(getN(d)); return;
		case 0x4026: LDSMPR(getN(d)); return;
		case 0x40f6: LDCMDBR(getN(d)); return;
		case 0x4007: LDCMSR(getN(d)); return;
		case 0x4017: LDCMGBR(getN(d)); return;
		case 0x4027: LDCMVBR(getN(d)); return;
		case 0x4037: LDCMSSR(getN(d)); return;
		case 0x4047: LDCMSPC(getN(d)); return;
		case 0x4008: SHLL2(getN(d)); return;
		case 0x4018: SHLL8(getN(d)); return;
		case 0x4028: SHLL16(getN(d)); return;
		case 0x4009: SHLR2(getN(d)); return;
		case 0x4019: SHLR8(getN(d)); return;
		case 0x4029: SHLR16(getN(d)); return;
		case 0x400a: LDSMACH(getN(d)); return;
		case 0x401a: LDSMACL(getN(d)); return;
		case 0x402a: LDSPR(getN(d)); return;
		case 0x406a: LDSFPSCR(getN(d)); return;
		case 0x405a: LDSFPUL(getN(d)); return;
		case 0x40fa: LDCDBR(getN(d)); return;
		case 0x400b: JSR(getN(d)); return;
		case 0x401b: TAS(getN(d)); return;
		case 0x402b: JMP(getN(d)); return;
		case 0x400e: LDCSR(getN(d)); return;
		case 0x401e: LDCGBR(getN(d)); return;
		case 0x402e: LDCVBR(getN(d)); return;
		case 0x403e: LDCSSR(getN(d)); return;
		case 0x404e: LDCSPC(getN(d)); return;
		} break;
		
	case 0x7000: ADDI(getI(d), getN(d)); return;
	case 0xa000: BRA((Sword)((Word) d & 0xfff)); return;
	case 0xb000: BSR((Sword)((Word) d & 0xfff)); return;
	case 0xe000: MOVI(getI(d), getN(d)); return;
	case 0x9000: MOVWI(getI(d), getN(d)); return;
	case 0xd000: MOVLI(getI(d), getN(d)); return;
	case 0x1000: MOVLS4(getM(d), getI(d), getN(d)); return;
	case 0x5000: MOVLL4(getM(d), getI(d), getN(d)); return;
	}

	switch(d & 0xf08f)
	{
	case 0x408e: LDCRBANK(getM(d)&0x7, getN(d)); return;
	case 0x4087: LDCMRBANK(getM(d)&0x7, getN(d)); return;
	case 0x0082: STCRBANK(getM(d)&0x7, getN(d)); return;
	case 0x4083: STCMRBANK(getM(d)&0x7, getN(d)); return;
	}

	switch(d)
	{
	case 0x0028: CLRMAC(); return;
	case 0x0048: CLRS(); return;
	case 0x0008: CLRT(); return;
	case 0x0019: DIV0U(); return;
	case 0x0009: NOP(); return;
	case 0x002b: RTE(); return;
	case 0x000b: RTS(); return;
	case 0x0058: SETS(); return;
	case 0x0018: SETT(); return;
	case 0x0038: mmu->ldtlb(); PC+=2; return;
	case 0x001b: SLEEP(); return;
	case 0xfffd: dispatchSwirlyHook(); return;
	}

	switch(d & 0xF00F)
	{
	case 0xf000: FADD(getM(d), getN(d)); return;
	case 0xf001: FSUB(getM(d), getN(d)); return;
	case 0xf002: FMUL(getM(d), getN(d)); return;
	case 0xf003: FDIV(getM(d), getN(d)); return;
	case 0xf004: FCMPEQ(getM(d), getN(d)); return;
	case 0xf005: FCMPGT(getM(d), getN(d)); return;
	case 0xf006: FMOV_INDEX_LOAD(getM(d), getN(d)); return;
	case 0xf007: FMOV_INDEX_STORE(getM(d), getN(d)); return;
	case 0xf008: FMOV_LOAD(getM(d), getN(d)); return;
	case 0xf009: FMOV_RESTORE(getM(d), getN(d)); return;
	case 0xf00a: FMOV_STORE(getM(d), getN(d)); return;
	case 0xf00b: FMOV_SAVE(getM(d), getN(d)); return;
	case 0xf00c: FMOV(getM(d), getN(d)); return;
	case 0xf00e: FMAC(getM(d), getN(d)); return;

	case 0x000f: DO_MACL(getM(d), getN(d)); return;
	case 0x0004: MOVBS0(getM(d), getN(d)); return;
	case 0x0005: MOVWS0(getM(d), getN(d)); return;
	case 0x0006: MOVLS0(getM(d), getN(d)); return;
	case 0x000c: MOVBL0(getM(d), getN(d)); return;
	case 0x000d: MOVWL0(getM(d), getN(d)); return;
	case 0x000e: MOVLL0(getM(d), getN(d)); return;
	case 0x0007: MULL(getM(d), getN(d)); return;

	case 0x400f: MACW(getM(d), getN(d)); return;
	case 0x400c: SHAD(getM(d), getN(d)); return;
	case 0x400d: SHLD(getM(d), getN(d)); return;
	}

	switch(d & 0xF0FF)
	{
	case 0x0023: BRAF(getN(d)); return;
	case 0x0003: BSRF(getN(d)); return;
	case 0x00c3: MOVCAL(getN(d)); return;
	case 0x0029: MOVT(getN(d)); return;
	case 0x0093: OCBI(getN(d)); return;
	case 0x00a3: OCBP(getN(d)); return;
	case 0x00b3: OCBWB(getN(d)); return;
	case 0x0083: PREF(getN(d)); return;
	case 0x0002: STCSR(getN(d)); return;
	case 0x0012: STCGBR(getN(d)); return;
	case 0x0022: STCVBR(getN(d)); return;
	case 0x0032: STCSSR(getN(d)); return;
	case 0x0042: STCSPC(getN(d)); return;
	case 0x003a: STCSGR(getN(d)); return;
	case 0x00fa: STCDBR(getN(d)); return;
	case 0x000a: STSMACH(getN(d)); return;
	case 0x001a: STSMACL(getN(d)); return;
	case 0x002a: STSPR(getN(d)); return;
	case 0x0062: STSFPSCR(getN(d)); return;
	case 0x005a: STSFPUL(getN(d)); return;

	case 0xf00d: FSTS(getN(d)); return;
	case 0xf01d: FLDS(getN(d)); return;
	case 0xf02d: FLOAT(getN(d)); return;
	case 0xf03d: FTRC(getN(d)); return;
	case 0xf04d: FNEG(getN(d)); return;
	case 0xf07d: FSRRA(getN(d)); return;
	case 0xf08d: FLDI0(getN(d)); return;
	case 0xf09d: FLDI1(getN(d)); return;
	case 0xf0ad: FCNVSD(getN(d)); return;
	case 0xf0bd: FCNVDS(getN(d)); return;
	case 0xf0fd: FSCA(getN(d)); return;
	}

	// Uh-oh; we can't figure out this instruction
	exception(E_GENERAL_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
}

// starts executing instructions
void SHCpu::go()
{
	Word d;
	Dword numIterations = 0;

	for(;;)
	{
		if(debugger->prompt())
		{
//			try {
			d = mmu->fetchInstruction(PC);
			executeInstruction(d);
//			} catch (...) { // SHMmu::xMMUException
//				printf("Exception caught\n");
//			}
			numIterations++;
			if((numIterations & 0x7) == 0x0)
			{
				tmu->updateTCNT0();
				tmu->updateTCNT1();
			}	
			if((numIterations & 0x7ffff) == 0x7ffff)
			{
				// make SDL handle events
				overlord->handleEvents();
				gpu->drawFrame();
			}
		}
	} // for
}

void SHCpu::exception(Dword type, Dword addr, Dword data, char *datadesc)
{
	exceptionsPending++;
	if(datadesc == 0)
	{
		debugger->flamingDeath("Received an exception of type 0x%x (%s) at %08X",
			type,
			debugger->getExceptionName(type),
			addr,
			data);
	} else
	{
		debugger->flamingDeath("Received an exception of type 0x%x (%s) at %08X.  %s: %08x",
			type,
			debugger->getExceptionName(type),
			addr,
			datadesc,
			data);
	}
}
