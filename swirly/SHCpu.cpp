#include <math.h>

#include "Gdrom.h"
#include "Maple.h"
#include "SHIntc.h"
#include "SHTmu.h"

#include "SHCpu.h"
#include "SHMmu.h"
#include "SHDmac.h"
#include "Debugger.h"
#include "Overlord.h"
#include "Gpu.h"

#include "SHCpuTable.h"

// used for Swirly hooks
#define HOOK_GDROM 1
#define HOOK_LOAD1STREAD 2

#define DYNAREC 0 // disable dynarec engine

SHCpu::SHCpu()
{
#if DYNAREC
    branch = 0;
#endif
    reset();

    printf("Building interpretative CPU jumptable\n\n");

    opcode_handler_struct *ostruct;

    for(int i = 0; i < 0x10000; i++) 
	sh_instruction_jump_table[i] = &SHCpu::unknownOpcode;
    
    ostruct = sh_opcode_handler_table;
    do {
	for(int i=0;i<0x10000;i++) 
	{
	    if((i&ostruct->mask) == ostruct->match) 
		sh_instruction_jump_table[i] = ostruct->opcode_handler;
	}
	ostruct++;
    } while(ostruct->match != 0xfffd);
	
    sh_instruction_jump_table[0xfffd] = &SHCpu::dispatchSwirlyHook;

}

SHCpu::~SHCpu() {}

void SHCpu::reset()
{
	setSR(0x70003cf0);
	PC = 0xA0000000;
	VBR = 0;
	*((Dword*)(ccnRegs+EXPEVT)) = 0;
	*((Dword*)((Dword)ccnRegs+CCR)) = 0;
	*((Dword*)((Dword)ccnRegs+MMUCR)) = 0xff000010;
	setFPSCR(0x00040001);
	numIterations = 0;
}

void SHCpu::executeInstruction(Word d) {
    (this->*sh_instruction_jump_table[d])(d); 
}

void SHCpu::go() {

    currInstruction = mmu->fetchInstruction(PC);

    for(;;)
    {
	if(debugger->prompt())
        {
		executeInstruction(cpu->currInstruction);
	    if (++numIterations == 0) intc->iterationsOverflow();
	    intc->checkInterrupt();
	    currInstruction = mmu->fetchInstruction(PC);
	}
    }
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

	switch(d&0xf0ff)
        {
	case 0x402b: // JMP
	case 0x400b: // JSR
	case 0x0023: // BRAF
	case 0x0003: // BSRF
	case 0x400e: // LDC Rm, SR
	case 0x4007: // LDC @Rm+, SR
	    intc->exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
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
	    intc->exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
	return;
	}

	switch(d&0xf000)
	{
	case 0xa000: // BRA
	case 0xb000: // BSR
	case 0x9000: // MOVWI
	case 0xd000: // MOVLI
	    intc->exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
	return;
	}

	switch(d)
	{
	case 0x000b: // RTS
	case 0x002b: // RTE
	case 0xfffd: // undefined instruction
	    intc->exception(E_SLOT_ILLEGAL_INSTRUCTION, PC, d, "Offending opcode");
	return;
	}

	executeInstruction(d);
}

// allows programs running on Swirly to hook into Swirly
// i.e. the GDROM - I don't know the proper hardware interface
// to it, but we can simulate it through faked syscalls.
// Obviously, this is not present on a real DC.
void SHCpu::dispatchSwirlyHook(Word op) 
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
	case HOOK_LOAD1STREAD: {
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

void SHCpu::unknownOpcode(Word op)
{
    intc->exception(E_GENERAL_ILLEGAL_INSTRUCTION, PC, op, "Offending opcode");
}

void SHCpu::XTRCT(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t;
	t=(R[m]<<16)&0xffff0000;
	R[n]=(R[n]>>16)&0xffff;
	R[n]|=t;
	PC+=2;
}

void SHCpu::XORI(Word op)
{
	R[0]^=(op&0xff);
	PC+=2;
}

void SHCpu::XORM(Word op)
{
	Byte t;
	if (mmu->readByteTLB(GBR+R[0], &t)) return;
	t^=(op&0xff);
	if (mmu->writeByteTLB(GBR+R[0], t)) return;
	PC+=2;
}

void SHCpu::XOR(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]^=R[m];
	PC+=2;
}

void SHCpu::TST(Word op)
{
	int m = getM(op), n = getN(op);
	if((R[n]&R[m])==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu::TSTI(Word op)
{
	Dword t;
	t=R[0]&(0xff&op);
	if(t==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu::TSTM(Word op)
{
	Byte t;
	if (mmu->readByteTLB(GBR+R[0], &t)) return;
	t&=(op&0xff);
	if(t==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu::TRAPA(Word op)
{
	SSR=SR;
	SPC=PC+2;
	setSR(SR|F_SR_MD|F_SR_BL|F_SR_RB);
	mmu->access(0xff000000|TRA, MMU_WRITE_DWORD, (op&0xff)<<2);
	mmu->access(0xff000000|EXPEVT, MMU_WRITE_DWORD, 0x160);
	PC=VBR+0x100;
}

void SHCpu::TAS(Word op)
{
	int n = getN(op);
	Byte t, oldt;
	if (mmu->readByteTLB(R[n], &t)) return;
	oldt=t;
	t|=0x80;
	if (mmu->writeByteTLB(R[n], t)) return;
	if(oldt==0)T=1; else T=0;
	PC+=2;
}

void SHCpu::SWAPW(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t;
	t=(R[m]>>16)&0xffff;
	R[n]=R[m]<<16;
	R[n]|=t;
	PC+=2;
}

void SHCpu::SWAPB(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t0, t1;
	t0=R[m]&0xffff0000;
	t1=(R[m]&0xff)<<8;
	R[n]=(R[m]&0xff00)>>8;
	R[n]=R[n]|t1|t0;
	PC+=2;
}

void SHCpu::SUBV(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::SUBC(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t0, t1;
	t1=R[n]-R[m];
	t0=R[n];
	R[n]=t1-T;
	if(t0<t1)T=1;else T=0;
	if(t1<R[n]) T=1;
	PC+=2;
}

void SHCpu::SUB(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]-=R[m];
	PC+=2;
}

void SHCpu::STSMACH(Word op)
{
	int n = getN(op);
	R[n]=MACH;
	PC+=2;
}

void SHCpu::STSMACL(Word op)
{
	int n = getN(op);
	R[n]=MACL;
	PC+=2;
}

void SHCpu::STSPR(Word op)
{
	int n = getN(op);
	R[n]=PR;
	PC+=2;
}

void SHCpu::STSMMACH(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, MACH)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STSMMACL(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, MACL)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STSMPR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, PR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STSFPSCR(Word op)
{
	int n = getN(op);
	R[n] = FPSCR & 0x003fffff;
	PC+=2;
}

void SHCpu::STSFPUL(Word op)
{
	int n = getN(op);
	R[n] = FPUL;
	PC+=2;
}

void SHCpu::STSMFPSCR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, FPSCR & 0x003fffff)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STSMFPUL(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, FPUL)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCSR(Word op) // privileged
{
	int n = getN(op);
	R[n]=SR;
	PC+=2;
}

void SHCpu::STCGBR(Word op)
{
	int n = getN(op);
	R[n]=GBR;
	PC+=2;
}

void SHCpu::STCVBR(Word op)
{
	int n = getN(op);
	R[n]=VBR;
	PC+=2;
}

void SHCpu::STCSSR(Word op)
{
	int n = getN(op);
	R[n]=SSR;
	PC+=2;
}

void SHCpu::STCSPC(Word op)
{
	int n = getN(op);
	R[n]=SPC;
	PC+=2;
}

void SHCpu::STCSGR(Word op)
{
	int n = getN(op);
	R[n]=SGR;
	PC+=2;
}


void SHCpu::STCDBR(Word op)
{
	int n = getN(op);
	R[n]=DBR;
	PC+=2;
}

void SHCpu::STCRBANK(Word op)
{
	int m = getM(op)&0x7, n = getN(op);
	R[n]=RBANK[m];
	PC+=2;
}

void SHCpu::STCMSR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, SR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMGBR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, GBR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMVBR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, VBR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMSSR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, SSR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMSPC(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, SPC)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMSGR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, SGR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMDBR(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, DBR)) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::STCMRBANK(Word op)
{
	int m = getM(op)&0x7, n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, RBANK[m])) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::SLEEP(Word op)
{
        //PC += 2;
        numIterations += 2; // speed things up a bit
	return;
}

void SHCpu::SHLR(Word op)
{
	int n = getN(op);
	if((R[n]&1)==0) T=0;
	else T=1;
	R[n]>>=1;
	R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu::SHLR16(Word op)
{
	int n = getN(op);
	R[n]>>=16;
	PC+=2;
}

void SHCpu::SHLR8(Word op)
{
	int n = getN(op);
	R[n]>>=8;
	PC+=2;
}

void SHCpu::SHLR2(Word op)
{
	int n = getN(op);
	R[n]>>=2;
	PC+=2;
}

void SHCpu::SHLL16(Word op)
{
	int n = getN(op);
	R[n]<<=16;
	PC+=2;
}

void SHCpu::SHLL8(Word op)
{
	int n = getN(op);
	R[n]<<=8;
	PC+=2;
}

void SHCpu::SHLL2(Word op)
{
	int n = getN(op);
	R[n]<<=2;
	PC+=2;
}

void SHCpu::SHLL(Word op)
{
	int n = getN(op);
	if((R[n]&0x80000000)==0) T=0; else T=1;
	R[n]<<=1;
	PC+=2;
}

void SHCpu::SHLD(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::SHAR(Word op)
{
	int n = getN(op);
	Dword t;
	if((R[n]&1)==0)T=0;else T=1;
	if((R[n]&0x80000000)==0)t=0; else t=1;
	R[n]>>=1;
	if(t==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu::SHAL(Word op)
{
	int n = getN(op);
	if((R[n] & 0x80000000) == 0) T=0;
	else T=1;
	R[n] <<=1;
	PC+=2;
}

void SHCpu::SHAD(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::SETT(Word op)
{
	T=1;
	PC+=2;
}

void SHCpu::SETS(Word op)
{
	S=1;
	PC+=2;
}

void SHCpu::RTS(Word op)
{
	delaySlot();
	debugger->reportBranch("rts", PC, PR);
	PC=PR;
}	

void SHCpu::RTE(Word op) // privileged
{
	delaySlot();
	setSR(SSR);
	debugger->reportBranch("rte", PC, SPC);
	PC=SPC;
}

void SHCpu::ROTR(Word op)
{
	int n = getN(op);
	if((R[n]&1)==0)T=0;
	else T=1;
	R[n]>>=1;
	if(T==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu::ROTL(Word op)
{
	int n = getN(op);
	if((R[n]&0x80000000)==0)T=0;else T=1;
	R[n]<<=1;
	if(T==1)R[n]|=1;
	else R[n]&=0xfffffffe;
	PC+=2;
}

void SHCpu::ROTCR(Word op)
{
	int n = getN(op);
	Dword t;
	if((R[n]&1)==0)t=0;
	else t=1;
	R[n]>>=1;
	if(T==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	if(t==1)T=1; else T=0;
	PC+=2;
}

void SHCpu::ROTCL(Word op)
{
	int n = getN(op);
	Dword t;
	if((R[n]&0x80000000)==0)t=0;else t=1;
	R[n]<<=1;
	if(T==1)R[n]|=1;
	else R[n]&=0xfffffffe;;
	if(t==1)T=1;else T=0;
	PC+=2;
}

void SHCpu::PREF(Word op)
{
	int n = getN(op);
	if((R[n] & 0xfc000000) == 0xe0000000) // is this a store queue write?
		mmu->storeQueueSend(R[n]);
	//else
	//	printf("PREF: %x\n", R[n]);
	PC+=2;
}

void SHCpu::OR(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] |= R[m];
	PC+=2;
}

void SHCpu::ORI(Word op)
{
	R[0] |= (Dword) (Byte) (op&0xff);
	PC+=2;
}

void SHCpu::ORM(Word op)
{
	Byte t;
	if (mmu->readByteTLB(GBR+R[0], &t)) return;
	t |= (Byte)(op&0xff);
	if (mmu->writeByteTLB(GBR+R[0], t)) return;
	PC+=2;
}

void SHCpu::OCBWB(Word op)
{
	//int n = getN(op);
	// XXX: should this do something?
	PC+=2;
}

void SHCpu::OCBP(Word op)
{
	//int n = getN(op);
	// XXX: should this do something?
	PC+=2;
}

void SHCpu::OCBI(Word op)
{
	//int n = getN(op);
	// XXX: should this do something?
	PC+=2;
}

void SHCpu::NOT(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]= ~R[m];
	PC+=2;
}

void SHCpu::NOP(Word op)
{
	PC+=2;
}

void SHCpu::NEGC(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::NEG(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]=0-R[m];
	PC+=2;
}

void SHCpu::MULU(Word op)
{
	int m = getM(op), n = getN(op);
	MACL=((Dword)(Word)R[n]*(Dword)(Word)R[m]);
	PC+=2;
}

void SHCpu::MULS(Word op)
{
	int m = getM(op), n = getN(op);
	MACL=((Sdword)(Sword)R[n]*(Sdword)(Sword)R[m]);
	PC+=2;
}

void SHCpu::MULL(Word op)
{
	int m = getM(op), n = getN(op);
	MACL=R[m]*R[n];
	PC+=2;
}

void SHCpu::MOVT(Word op)
{
	int n = getN(op);
	R[n]=(SR&1);
	PC+=2;
}

void SHCpu::MOVCAL(Word op)
{
	int n = getN(op);
	if (mmu->writeDwordTLB(R[n], R[0])) return;
	PC+=2;
}

void SHCpu::MOVA(Word op)
{
	Dword dest;
	dest = ((op&0xff)<<2)+4+(PC&0xfffffffc);
	R[0]=dest;
	PC+=2;
}

void SHCpu::MOVBS4(Word op)
{
	int n = getM(op);
	Dword dest;
	dest=(op&0xf)+R[n];
	if (mmu->writeByteTLB(dest, R[0])) return;
	PC+=2;
}

void SHCpu::MOVWS4(Word op)
{
	int n = getM(op);
	Dword dest;
	dest=((op&0xf)<<1)+R[n];
	if (mmu->writeWordTLB(dest, R[0])) return;
	PC+=2;
}

void SHCpu::MOVLS4(Word op)
{
	int m = getM(op), n = getN(op);
	Dword dest;
	dest=((op&0xf)<<2)+R[n];
	if (mmu->writeDwordTLB(dest, R[m])) return;
	PC+=2;
}

void SHCpu::MOVBL4(Word op)
{
	int m = getM(op);
	Dword dest;
	dest = (op&0xf)+R[m];
	Byte t;
	if (mmu->readByteTLB(dest, &t)) return;
	R[0] = (Sdword) (Sbyte) t;
	PC+=2;
}

void SHCpu::MOVWL4(Word op)
{
	int m = getM(op);
	Dword dest;
	dest = ((op&0xf)<<1)+R[m];
	Word t;
	if (mmu->readWordTLB(dest, &t)) return;
	R[0] = (Sdword) (Sword) t;
	PC+=2;
}

void SHCpu::MOVLL4(Word op)
{
	int m = getM(op), n = getN(op);
	Dword dest;
	dest=((op&0xf)<<2)+R[m];
	if (mmu->readDwordTLB(dest, &R[n])) return;
	PC+=2;
}


void SHCpu::MOVBLG(Word op)
{
	Dword dest;
	dest = (Dword)(op&0xff)+GBR;
	Byte t;
	if (mmu->readByteTLB(dest, &t)) return;
	R[0] = (Sdword) (Sbyte) t;
	PC+=2;
}

void SHCpu::MOVWLG(Word op)
{
	Dword dest;
	dest = ((Dword)(op&0xff)<<1)+GBR;
	Word t;
	if (mmu->readWordTLB(dest, &t)) return;
	R[0] = (Sdword) (Sword) t;
	PC+=2;
}

void SHCpu::MOVLLG(Word op)
{
	Dword dest;
	dest=((Dword)(op&0xff)<<2)+GBR;
	if (mmu->readDwordTLB(dest, &R[0])) return;
	PC+=2;
}

void SHCpu::MOVBSG(Word op)
{
	Dword dest;
	dest = (Dword)(op&0xff)+GBR;
	if (mmu->writeByteTLB(dest, R[0])) return;
	PC+=2;
}

void SHCpu::MOVWSG(Word op)
{
	Dword dest;
	dest = ((Dword)(op&0xff)<<1)+GBR;
	if (mmu->writeWordTLB(dest, R[0])) return;
	PC+=2;
}

void SHCpu::MOVLSG(Word op)
{
	Dword dest;
	dest = ((Dword)(op&0xff)<<2)+GBR;
	if (mmu->writeDwordTLB(dest, R[0])) return;
	PC+=2;
}

void SHCpu::MOVI(Word op)
{
	int n = getN(op);
	R[n] = (Sdword) (Sbyte) (op & 0xff);
	PC+=2;
}

void SHCpu::MOVWI(Word op)
{
	int n = getN(op);
	Dword dest;
	dest = (((Dword)(op&0xff)) << 1)+4+PC;
	Word t;
	if (mmu->readWordTLB(dest, &t)) return;
	R[n] = (Sdword) (Sword) t;
	PC+=2;
}

void SHCpu::MOVLI(Word op)
{
	int n = getN(op);
	Dword dest;
	dest = (((Dword)(op&0xff)) << 2)+4+(PC&0xfffffffc);
	if (mmu->readDwordTLB(dest, &R[n])) return;
	PC+=2;
}

void SHCpu::MOV(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]=R[m];
	PC+=2;
}

void SHCpu::MOVBS(Word op) // mov.b Rm, @Rn
{
	int m = getM(op), n = getN(op);
	if (mmu->writeByteTLB(R[n], R[m])) return;
	PC+=2;
}

void SHCpu::MOVWS(Word op)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeWordTLB(R[n], R[m])) return;
	PC+=2;
}

void SHCpu::MOVLS(Word op)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeDwordTLB(R[n], R[m])) return;
	PC+=2;
}

void SHCpu::MOVBL(Word op) // mov.b @Rm, Rn
{
	int m = getM(op), n = getN(op);
	Byte t;
	if (mmu->readByteTLB(R[m], &t)) return;
	R[n] = (Sdword) (Sbyte) t;
	PC+=2;
}

void SHCpu::MOVWL(Word op) // mov.w @Rm, Rn
{
	int m = getM(op), n = getN(op);
	Word t;
	if (mmu->readWordTLB(R[m], &t)) return;
	R[n] = (Sdword) (Sword) t;
	PC+=2;
}

void SHCpu::MOVLL(Word op) // mov.l @Rm, Rn
{
	int m = getM(op), n = getN(op);
	if (mmu->readDwordTLB(R[m], &R[n])) return;
	PC+=2;
}

void SHCpu::MOVBM(Word op) // mov.b Rm, @-Rn
{
	int m = getM(op), n = getN(op);
	if (mmu->writeByteTLB(R[n]-1, R[m])) return;
	R[n]--;
	PC+=2;
}

void SHCpu::MOVWM(Word op)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeWordTLB(R[n]-2, R[m])) return;
	R[n]-=2;
	PC+=2;
}

void SHCpu::MOVLM(Word op)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeDwordTLB(R[n]-4, R[m])) return;
	R[n]-=4;
	PC+=2;
}

void SHCpu::MOVBP(Word op) // mov.b @Rm+, Rn
{
	int m = getM(op), n = getN(op);
	Byte t;
	if (mmu->readByteTLB(R[m], &t)) return;
	R[n] = (Sdword) (Sbyte) t;
	if(n!=m) R[m]++;
	PC+=2;
}

void SHCpu::MOVWP(Word op) // mov.w @Rm+, Rn
{
	int m = getM(op), n = getN(op);
	Word t;
	if (mmu->readWordTLB(R[m], &t)) return;
	R[n] = (Sdword) (Sword) t;
	if(n!=m) R[m]+=2;
	PC+=2;
}

void SHCpu::MOVLP(Word op) // mov.l @Rm+, Rn
{
	int m = getM(op), n = getN(op);
	if (mmu->readDwordTLB(R[m], &R[n])) return;
	if(n!=m) R[m]+=4;
	PC+=2;
}

void SHCpu::MOVBS0(Word op) // mov.b Rm, @(r0, Rn)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeByteTLB(R[n]+R[0], R[m])) return;
	PC+=2;
}

void SHCpu::MOVWS0(Word op) // mov.w Rm, @(r0, Rn)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeWordTLB(R[n]+R[0], R[m])) return;
	PC+=2;
}

void SHCpu::MOVLS0(Word op) // mov.l Rm, @(r0, Rn)
{
	int m = getM(op), n = getN(op);
	if (mmu->writeDwordTLB(R[n]+R[0], R[m])) return;
	PC+=2;
}

void SHCpu::MOVBL0(Word op) // mov.b @(r0, rm), rn
{
	int m = getM(op), n = getN(op);
	Byte t;
	if (mmu->readByteTLB(R[0]+R[m], &t)) return;
	R[n] = (Sdword) (Sbyte) t;
	PC+=2;
}

void SHCpu::MOVWL0(Word op) // mov.w @(r0, rm), rn
{
	int m = getM(op), n = getN(op);
	Word t;
	if (mmu->readWordTLB(R[0]+R[m], &t)) return;
	R[n] = (Sdword) (Sword) t;
	PC+=2;
}

void SHCpu::MOVLL0(Word op) // mov.l @(r0, rm), rn
{
	int m = getM(op), n = getN(op);
	if (mmu->readDwordTLB(R[0]+R[m], &R[n])) return;
	PC+=2;
}

void SHCpu::MACW(Word op)
{
	int m = getM(op), n = getN(op);
	Sdword tm, tn, d,s,a;
	Word t;
	Dword t1;
	if (mmu->readWordTLB(R[n], &t)) return;
	tn = (Sdword) t;
	if (mmu->readWordTLB(R[m], &t)) return;
	tm = (Sdword) t;
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

void SHCpu::DO_MACL(Word op)
{
	int m = getM(op), n = getN(op);
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
	//	shcpu_DO_MACL(R[n], R[m], &MACH, &MACL);
    
	if(S==1)
	{
		// do saturation at bit 48
		switch(bits(MACH, 31, 16))
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

void SHCpu::FSTS(Word op)
{
	int n = getN(op);
	FR_Dwords[n] = FPUL;
	PC+=2;
}

void SHCpu::LDSFPSCR(Word op)
{
	int n = getN(op);
	setFPSCR(R[n] & 0x003fffff);
	PC+=2;
}

void SHCpu::LDSMFPSCR(Word op)
{
	int n = getN(op);
	Dword t;
	if (mmu->readDwordTLB(R[n], &t)) return;
	setFPSCR(t & 0x003fffff);
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDSFPUL(Word op)
{
	int n = getN(op);
	FPUL = R[n];
	PC+=2;	
}

void SHCpu::LDSMFPUL(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &FPUL)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDSMPR(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &PR)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDSMMACL(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &MACL)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDSMMACH(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &MACH)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDSPR(Word op)
{
	int n = getN(op);
	PR=R[n];
	PC+=2;
}

void SHCpu::LDSMACL(Word op)
{
	int n = getN(op);
	MACL=R[n];
	PC+=2;
}

void SHCpu::LDSMACH(Word op)
{
	int n = getN(op);
	MACH=R[n];
	PC+=2;
}

void SHCpu::LDCSR(Word op) // privileged
{
	int n = getN(op);
	setSR(R[n]&0x700083f3);
	PC+=2;
}

void SHCpu::LDCGBR(Word op)
{
	int n = getN(op);
	GBR=R[n];
	PC+=2;
}

void SHCpu::LDCVBR(Word op) // privileged
{
	int n = getN(op);
	VBR=R[n];
	PC+=2;
}

void SHCpu::LDCSSR(Word op) // privileged
{
	int n = getN(op);
	SSR=R[n];
	PC+=2;
}

void SHCpu::LDCSPC(Word op) // privileged
{
	int n = getN(op);
	SPC=R[n];
	PC+=2;
}

void SHCpu::LDCDBR(Word op) // privileged
{
	int n = getN(op);
	DBR=R[n];
	PC+=2;
}

void SHCpu::LDCRBANK(Word op) // privileged
{
	int m = getM(op)&0x7, n = getN(op);
	// n = source, m = dest
	RBANK[m]=R[n];
	PC+=2;
}

void SHCpu::LDCMRBANK(Word op) // privileged
{
	int m = getM(op)&0x7, n = getN(op);
	// n = source, m = dest
	if (mmu->readDwordTLB(R[n], &RBANK[m])) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDCMSR(Word op)
{
	int n = getN(op);
	Dword t;
	if (mmu->readDwordTLB(R[n], &t)) return;
	setSR(t&0x700083f3);
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDCMGBR(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &GBR)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDCMVBR(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &VBR)) return;
	R[n]+=4;
	PC+=2;
}


void SHCpu::LDCMSSR(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &SSR)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDCMSPC(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &SPC)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::LDCMDBR(Word op)
{
	int n = getN(op);
	if (mmu->readDwordTLB(R[n], &DBR)) return;
	R[n]+=4;
	PC+=2;
}

void SHCpu::JSR(Word op)
{
	int n = getN(op);
	Dword temp, oldpc;
	temp = R[n];
	oldpc = PC;
	delaySlot();
	PR=oldpc+4;
	
	debugger->reportBranch("jsr", PC, temp);
	PC=temp;
}

void SHCpu::JMP(Word op)
{
	int n = getN(op);
	Dword temp;
	temp = R[n];
	delaySlot();
	
	debugger->reportBranch("jmp", PC, temp);
	PC=temp;
}	

void SHCpu::DMULU(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::EXTUW(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] = R[m] & 0xffff;
	PC+=2;
}

void SHCpu::EXTUB(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] = R[m] & 0xff;
	PC+=2;
}

void SHCpu::EXTSB(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]=	((R[m]&0x80)==0) ? R[m]&0xff : R[m] | 0xffffff00;
	PC+=2;
}

void SHCpu::EXTSW(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]= ((R[m]&0x8000)==0) ? R[m]&0xffff : R[m] | 0xffff0000;
	PC+=2;
}

void SHCpu::DT(Word op)
{
	int n = getN(op);
	R[n]--;
	if(R[n]==0)
		T=1;
	else
		T=0;
	PC+=2;
}

void SHCpu::DMULS(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::DIV1(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::DIV0U(Word op)
{
	setSR(SR & ((~F_SR_M)&(~F_SR_Q)&(~F_SR_T)));
	PC+=2;
}

void SHCpu::DIV0S(Word op)
{
	int m = getM(op), n = getN(op);
    
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

void SHCpu::CMPIM(Word op)
{
	if((Sdword)R[0] == (Sdword)(Sbyte)(op&0xff))
		setSR(SR|F_SR_T);
	else
		setSR(SR& ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPSTR(Word op)
{
	int m = getM(op), n = getN(op);
	Dword foo;
	foo = R[m] ^ R[n];
	if((((foo&0xff000000)>>24) &&
	  ((foo&0xff0000)>>16) &&
	  ((foo&0xff00)>>8) &&
	  (foo&0xff)) == 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);		
	PC+=2;
}
void SHCpu::CMPPZ(Word op)
{
	int n = getN(op);
	if((signed int)R[n] >= 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPPL(Word op)
{
	int n = getN(op);

	if((signed int)R[n] > 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPHS(Word op)
{
	int m = getM(op), n = getN(op);
	if(R[n] >= R[m]) 
		setSR(SR | F_SR_T);
	else 
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPHI(Word op)
{
	int m = getM(op), n = getN(op);
	if(R[n] > R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPGT(Word op)
{
	int m = getM(op), n = getN(op);
	
	if((Sdword)R[n] > (Sdword)R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPGE(Word op)
{
	int m = getM(op), n = getN(op);
	if((Sdword)R[n] >= (Sdword)R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CMPEQ(Word op)
{
	int m = getM(op), n = getN(op);
	if(R[n] == R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	/*		T=1;
			else
			T=0;
	*/	PC+=2;
}

void SHCpu::CLRT(Word op)
{
	setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu::CLRS(Word op)
{
	setSR(SR & ~F_SR_S);
	PC+=2;
}

void SHCpu::CLRMAC(Word op)
{
	MACH=MACL=0;
	PC+=2;
}

void SHCpu::BTS(Word op)
{
	Dword dest, oldpc;
	dest = PC + 4 + (((Sdword) (Sbyte) (op&0xff)) * 2);
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

void SHCpu::BT(Word op) // no delay slot!
{
	Dword dest;
	dest = PC + 4 + (((Sdword) (Sbyte) (op&0xff)) * 2);
	if(SR & F_SR_T) 
        {
		debugger->reportBranch("bt", PC, dest);
		PC=dest;
	}
	else
		PC+=2;
}

void SHCpu::BSRF(Word op)
{
	int n = getN(op);
	Dword dest, oldpc;
	dest = PC+4+(Sdword)R[n];
	oldpc = PC;
	delaySlot();
	PR=oldpc+4;
	debugger->reportBranch("bsrf", PC, dest);
	PC=dest;
}

void SHCpu::BSR(Word op)
{
	Sword d = (op&0xfff);
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

void SHCpu::BRAF(Word op)
{
	int n = getN(op);
	Dword dest;
	dest = PC+4+(Sdword)R[n];
	delaySlot();
	debugger->reportBranch("braf", PC, dest);
	PC=dest;
}

void SHCpu::BRA(Word op)
{
	Sword d = (op&0xfff);
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

void SHCpu::BFS(Word op)
{
	Dword dest, oldpc;
	dest = PC + 4 + (((Sdword) (Sbyte) (op&0xff)) * 2);
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

void SHCpu::BF(Word op) // no delay slot!
{
	if((SR & F_SR_T)==0)
	{
		Dword dest = PC + 4 + (((Sdword) (Sbyte) (op&0xff)) * 2);
		debugger->reportBranch("bf", PC, dest);
		PC=dest;
	}
	else
		PC+=2;
}

void SHCpu::AND(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] &= R[m];
	PC+=2;
}

void SHCpu::ANDI(Word op)
{
	R[0] &= (Dword) (op&0xff);
	PC+=2;
}

void SHCpu::ANDM(Word op) // and.b #imm, @(r0, gbr)
{
	Byte t;
	if (mmu->readByteTLB(GBR+R[0], &t)) return;
	t &= (Byte)(op&0xff);
	if (mmu->writeByteTLB(GBR+R[0], t)) return;
	PC+=2;
}

void SHCpu::ADDV(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::ADD(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] += R[m];
	PC+=2;
}

void SHCpu::ADDI(Word op)
{
	int n = getN(op);
	R[n] += (Sdword) (Sbyte) (op&0xff); // sign-extend
	PC+=2;
}

void SHCpu::ADDC(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu::LDTLB(Word op)
{
	mmu->ldtlb();
	PC+=2;
}


void SHCpu::FMOV(Word op)
{
	int m = getM(op), n = getN(op);
	
	if(FPU_SZ()) 
	{
		n &= 0xe; m &= 0xe;
		// split up in different instructions
		switch(op&0x0110) 
                {
		case 0x0000:	// FMOV DRm, DRn
			FR_Dwords[n] = FR_Dwords[m];
			FR_Dwords[n+1] = FR_Dwords[m+1];
			break;
		case 0x0010:	// FMOV XDm, DRn
			FR_Dwords[n] = XF_Dwords[m];
			FR_Dwords[n+1] = XF_Dwords[m+1];
			break;
		case 0x0100:	// FMOV DRm, XDn
			XF_Dwords[n] = FR_Dwords[m];
			XF_Dwords[n+1] = FR_Dwords[m+1];
			break;
		case 0x0110:	// FMOV XDm, XDn
			XF_Dwords[n] = XF_Dwords[m];
			XF_Dwords[n+1] = XF_Dwords[m+1];
			break;
		}
	}
	else
		FR_Dwords[n] = FR_Dwords[m];
	PC+=2;
}

void SHCpu::FMOV_STORE(Word op)
{
	int m = getM(op), n = getN(op);

	if(FPU_SZ()) // double-precision
	{
		m &= 0xe;
		if (op & 0x0010)
		{
			if (mmu->writeQwordTLB(R[n], XF_Dwords[m], XF_Dwords[m+1])) return;
		}
		else
		{
			if (mmu->writeQwordTLB(R[n], FR_Dwords[m], FR_Dwords[m+1])) return;
		}
	}
	else // single-precision
		if (mmu->writeDwordTLB(R[n], FR_Dwords[m])) return;
	PC+=2;
}

void SHCpu::FMOV_LOAD(Word op)
{
	int m = getM(op), n = getN(op);

	if(FPU_SZ()) // double-precision
	{
		n &= 0xe;
		if (op & 0x0100)
		{
			if (mmu->readQwordTLB(R[m], &XF_Dwords[n], &XF_Dwords[n+1])) return;
		}
		else
		{
			if (mmu->readQwordTLB(R[m], &FR_Dwords[n], &FR_Dwords[n+1])) return;
		}
	}
	else // single-precision
		if (mmu->readDwordTLB(R[m], &FR_Dwords[n])) return;
	PC+=2;
}

void SHCpu::FMOV_RESTORE(Word op)
{
	int m = getM(op), n = getN(op);
    
	if(FPU_SZ()) // double-precision
	{
		n &= 0xe;
		if (op & 0x0100)
		{
			if (mmu->readQwordTLB(R[m], &XF_Dwords[n], &XF_Dwords[n+1])) return;
		}
		else
		{
			if (mmu->readQwordTLB(R[m], &FR_Dwords[n], &FR_Dwords[n+1])) return;
		}
		R[m]+=8;
	}
	else // single-precision
	{
		if (mmu->readDwordTLB(R[m], &FR_Dwords[n])) return;
		R[m]+=4;
	}
	PC+=2;
}

void SHCpu::FMOV_SAVE(Word op)
{
	int m = getM(op), n = getN(op);
    
	if(FPU_SZ()) // double-precision
	{
		m &= 0xe;
		if (op & 0x0010)
		{
			if (mmu->writeQwordTLB(R[n]-8, XF_Dwords[m], XF_Dwords[m+1])) return;
		}
		else
		{
			if (mmu->writeQwordTLB(R[n]-8, FR_Dwords[m], FR_Dwords[m+1])) return;
		}
		R[n]-=8;
	}
	else // single-precision
	{
		if (mmu->writeDwordTLB(R[n]-4, FR_Dwords[m])) return;
		R[n]-=4;
	}
	PC+=2;
}

void SHCpu::FMOV_INDEX_LOAD(Word op)
{
	int m = getM(op), n = getN(op);
    
	if(FPU_SZ()) // double-precision
	{
		n &= 0xe;
		if (op & 0x0100)
		{
			if (mmu->readQwordTLB(R[m]+R[0], &XF_Dwords[n], &XF_Dwords[n+1])) return;
		}
		else
		{
			if (mmu->readQwordTLB(R[m]+R[0], &FR_Dwords[n], &FR_Dwords[n+1])) return;
		}
	}
	else // single-precision
		if (mmu->readDwordTLB(R[m]+R[0], &FR_Dwords[n])) return;
	PC+=2;
}

void SHCpu::FMOV_INDEX_STORE(Word op)
{
	int m = getM(op), n = getN(op);

	if(FPU_SZ()) // double-precision
	{
		m &= 0xe;
		if (op & 0x0010)
		{
			if (mmu->writeQwordTLB(R[0]+R[n], XF_Dwords[m], XF_Dwords[m+1])) return;
		}
		else
		{
			if (mmu->writeQwordTLB(R[0]+R[n], FR_Dwords[m], FR_Dwords[m+1])) return;
		}
	}
	else // single-precision
		if (mmu->writeDwordTLB(R[0]+R[n], FR_Dwords[m])) return;
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
	    for(int i=8; i<16; i++) {
		R[i] = RBANK[i];
	    }
	}
	SR = d;
}

void SHCpu::FLDI0(Word op)
{
	int n = getN(op);
	FR[n] = 0.0f;
	PC+=2;
}

void SHCpu::FLDI1(Word op)
{
	int n = getN(op);
	FR[n] = 1.0f;
	PC+=2;
}

void SHCpu::FLDS(Word op)
{
	int n = getN(op);
	FPUL = FR_Dwords[n];
	PC+=2;
}

void SHCpu::FNEG(Word op)
{
	int n = getN(op);

	FPU_DP_FIX_N();
	if(FPU_DP()) DR[n] = -DR[n];
	else         FR[n] = -FR[n];

	PC+=2;
}

void SHCpu::FRCHG(Word op)
{
    // XXX: the dreamcast linux kernel triggers this...hrm
    //if(FPU_DP()) debugger->flamingDeath("FRCHG modified in double precision mode\n");
	setFPSCR(FPSCR ^ 0x00200000);
	PC+=2;
}

void SHCpu::FSCHG(Word op)
{
	if(FPU_DP()) debugger->flamingDeath("FSCHG modified in double precision mode\n");
	setFPSCR(FPSCR ^ 0x00100000);
	PC+=2;
}

void SHCpu::FABS(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
	
	if(FPU_DP()) *(((Qword*)DR)+n) &= 0x7fffffffffffffff;
	else         *(((Dword*)FR)+n) &= 0x7fffffff;

	PC+=2;
}

inline int SHCpu::test_pinf(int n) 
{
	if(FPU_DP()) return *(((Qword*)DR)+n) == 0x7ff0000000000000;
	else         return *(((Dword*)FR)+n) == 0x7f800000;
}

inline int SHCpu::test_ninf(int n)
{
	if(FPU_DP()) return *(((Qword*)DR)+n) == 0xfff0000000000000;
	else         return *(((Dword*)FR)+n) == 0xff800000;
}

inline int SHCpu::test_snan(int n)
{
	if(FPU_DP()) 
	{ 
		Qword abs = *(((Qword*)DR)+n) &= 0x7fffffffffffffff;
		return (abs >= 0x7ff8000000000000);
	} 
	else
	{
		Dword abs = *(((Dword*)FR)+n) &= 0x7fffffff;
		return (abs >= 0x7fc00000);
	}
}

inline int SHCpu::test_qnan(int n)
{
	if(FPU_DP()) 
	{ 
		Qword abs = *(((Qword*)DR)+n) &= 0x7fffffffffffffff;
		return (abs > 0x7ff0000000000000 && abs < 0x7ff8000000000000);
	} 
	else
	{
		Dword abs = *(((Dword*)FR)+n) &= 0x7fffffff;
		return (abs > 0x7f800000 && abs < 0x7fc00000);
	}
}

inline int SHCpu::test_denorm(int n)
{
	if(FPU_DP())
        {
		Qword abs = *(((Qword*)DR)+n) &= 0x7fffffffffffffff;
		return (abs < 0x0010000000000000) && !(FPSCR & F_FPSCR_DN) && 
			(abs != 0x0000000000000000); 
	}
	else
       	{
		Dword abs = *(((Dword*)FR)+n) &= 0x7fffffff;
		return (abs < 0x00800000) && !(FPSCR & F_FPSCR_DN) && 
			(abs != 0x00000000); 
	}
}

inline void SHCpu::qnan(int n)
{ 
	if(FPU_DP()) *(((Qword*)DR)+n) = 0x7ff7ffffffffffff;
	else         *(((Dword*)FR)+n) = 0x7fbfffff;
	
	PC+=2;
}

inline void SHCpu::invalid(int n)
{
	FPSCR |= F_FPSCR_V;
	if(!(FPSCR & F_ENABLE_V))
       	{
		if(FPU_DP()) *(((Qword*)DR)+n) = 0x7ff7ffffffffffff;
		else         *(((Dword*)FR)+n) = 0x7fbfffff;
				     //0xffffffdc;
	}
	else
       	{
		debugger->flamingDeath("FPU exception caught!\n");
	}
	PC+=2; 
}


void SHCpu::FADD(Word op)
{
	int m = getM(op), n = getN(op);
	//FPU_DP_FIX_MN();
	
	CLEAR_CAUSE();

	//if(test_snan(n) || test_snan(m)) { invalid(n); return; }
	//if(test_qnan(n) || test_qnan(m)) { qnan(n); return; }

	if(FPU_DP()) 
        {
		cnv_dbl tmpa, tmpb;
		tmpa.i[1] = FR_Dwords[n];
		tmpa.i[0] = FR_Dwords[n+1];
		tmpb.i[1] = FR_Dwords[m];
		tmpb.i[0] = FR_Dwords[m+1];
		//printf("FADD: %f + %f = ", tmpa.d, tmpb.d);
		tmpa.d += tmpb.d;
		//printf("%f\n", tmpa.d);
		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
		//		DR[n]+=DR[m];
	} 
	else 
	{
		double test = (float)FR[n] + (float)FR[m];
	
		FR[n] = (float)test;
		//FR[n]+=FR[m];
		//printf("FADD result is %f\n", FR[n]);
	}


	PC+=2;
}

void SHCpu::FCMPEQ(Word op)
{
	int m = getM(op), n = getN(op);
	FPU_DP_FIX_MN();
	
	if(FPU_DP()) 
	{
		if(DR[m] == DR[n])
			setSR(SR | F_SR_T);
		else
			setSR(SR & ~F_SR_T);
	}
	else
	{
		if(FR[m] == FR[n])
			setSR(SR | F_SR_T);
		else
			setSR(SR & ~F_SR_T);
	}
	PC+=2;
}

void SHCpu::FCMPGT(Word op)
{
	int m = getM(op), n = getN(op);
	FPU_DP_FIX_MN();
    
	if(FPU_DP())
	{
		if(DR[n] > DR[m])
			setSR(SR | F_SR_T);
		else
			setSR(SR & ~F_SR_T);
	}
	else
	{
		if(FR[n] > FR[m])
			setSR(SR | F_SR_T); 
		else
			setSR(SR & ~F_SR_T);
	}
	PC+=2;
}

void SHCpu::FCNVDS(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
	if(FPU_DP())
		//FPUL = (float) DR[n];
		FPUL = (Dword)DR[n];
	else
		debugger->flamingDeath("FCNVDS: Can't access DR regs when double-prec is off");
	PC+=2;
}

void SHCpu::FCNVSD(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
    
	DR[n] = (double) FPUL;
	PC+=2;
}

void SHCpu::FDIV(Word op)
{
	int m = getM(op), n = getN(op);
	//	FPU_DP_FIX_MN();
    
	if(FPSCR & F_FPSCR_PR)
	{
		cnv_dbl tmpa, tmpb;
		tmpa.i[1] = FR_Dwords[n]; tmpa.i[0] = FR_Dwords[n+1];
		tmpb.i[1] = FR_Dwords[m]; tmpb.i[0] = FR_Dwords[m+1];
		//printf("FDIV: %f / %f = ", tmpa.d, tmpb.d);
		tmpa.d /= tmpb.d;
		//printf("%f\n", tmpa.d);
		FR_Dwords[n] = tmpa.i[1]; FR_Dwords[n+1] = tmpa.i[0];
		//		DR[n] /= DR[m];
	}
	else
       	{
		FR[n] /= FR[m];
	}
	PC+=2;
}

void SHCpu::FMUL(Word op)
{
	int m = getM(op), n = getN(op);
	//	FPU_DP_FIX_MN();
	
	if(FPU_DP()) // double-precision
	{
		cnv_dbl tmpa, tmpb;
		tmpa.i[1] = FR_Dwords[n];
		tmpa.i[0] = FR_Dwords[n+1];
		tmpb.i[1] = FR_Dwords[m];
		tmpb.i[0] = FR_Dwords[m+1];
		//printf("FMUL: %f * %f = ", tmpa.d, tmpb.d);
		tmpa.d *= tmpb.d;
		//printf("%f\n", tmpa.d);

		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
		//		DR[n]*=DR[m];
	}
	else // single-precision
	{
		//		shfpu_sop0 = FR[n];
		//		shfpu_sop1 = FR[m];
		FR[n]*=FR[m];
		//		shfpu_sFMUL(Word op);
		//		FR[n] = shfpu_sresult;
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
	XF_Dwords = (Dword*)XF;
	//	shfpu_setContext(FR, XF, (float*)&FPUL, &FPSCR);
}

void SHCpu::FLOAT(Word op)
{
	int n = getN(op);
	//	FPU_DP_FIX_N();
	if(FPU_DP()) 
        {
		cnv_dbl tmpa;
		tmpa.d = (double)*((Sdword*)&FPUL);
		FR_Dwords[n] = tmpa.i[1];
		FR_Dwords[n+1] = tmpa.i[0];
		//		DR[n] = (double)*((float*)(signed int*)&FPUL);
		//		DR[n] = (double)*((signed int*)&FPUL);
	}
	else
	{
		//		shfpu_sFLOAT(Word op);
		//		FR[n] = shfpu_sresult;
		FR[n] = (float)*((Sdword*)&FPUL);
	}
    PC+=2;
}

void SHCpu::FMAC(Word op)
{
	int m = getM(op), n = getN(op);
	FR[n] = (FR[0] * FR[m]) + FR[n];
	PC+=2;
}

void SHCpu::FSQRT(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
	if(FPU_DP())
		DR[n] = sqrt(DR[n]);
	else
		FR[n] = sqrtf(FR[n]); //(float) sqrt((double) FR[n]);
	PC+=2;
}

void SHCpu::FSUB(Word op)
{
	int m = getM(op), n = getN(op);
	FPU_DP_FIX_MN();

	if(FPU_DP())
		DR[n]-=DR[m];
	else
		FR[n]-=FR[m];

	PC+=2;
}

void SHCpu::FTRC(Word op)
{
	int n = getN(op);
	//	FPU_DP_FIX_N();
	if(FPU_DP()) 
        {
		n &= 0xe;
		cnv_dbl tmpa;
		tmpa.i[1] = FR_Dwords[n];
		tmpa.i[0] = FR_Dwords[n+1];
		//printf("FTRC: %f = ", tmpa.d);
		FPUL = (Dword)(tmpa.d);	// XXX: (float) - always truncates too much
		//printf(" %08x (%d)\n", FPUL, FPUL);
		//		FPUL = (float) DR[n];
	}
	else 
        {
		FPUL = (Dword)FR[n];
		//		FPUL = FR[n];
	}
    PC+=2;
}

void SHCpu::FSCA(Word op)
{
	int n = getN(op);
	float angle = ((float)FPUL / 65535.0) * 2 * M_PI;
	FR[n] = sin(angle);
	FR[n+1] = cos(angle);
	PC+=2;
}

void SHCpu::FSRRA(Word op)
{
	int n = getN(op);

	// XXX: should this work on DR regs as well?
	if(FPU_DP()) 
	    debugger->flamingDeath("FSRRA with DR regs\n");

	FR[n] = 1/sqrt(FR[n]);
	PC+=2;
}

void SHCpu::FTRV(Word op)
{
	int n = getN(op) & 0xc;
	float tmp[4];
	tmp[0] = FR[n]; tmp[1] = FR[n+1]; tmp[2] = FR[n+2]; tmp[3] = FR[n+3];
	for (int i = 0; i < 4; i++)
       	{
		FR[n+i] = XF[0+i]*tmp[0] + XF[4+i]*tmp[1] + XF[8+i]*tmp[2] + XF[12+i]*tmp[3];
	}
	PC+=2;
}


#if DYNAREC 
// ***************************** Recompiler *****************************
#include "ix86.h"

float shfpu_sop0, shfpu_sop1, shfpu_sresult;
double shfpu_dop0, shfpu_dop1, shfpu_dresult;

int SHCpu::build_cl(Word op)
{
	Word d;
	
	if (((Dword)recPtr - (Dword)recMem) >= (0x800000 - 0x8000)) 
        {
		printf("Recompile Memory exceeded!\n");
		exit(-1);
		// recReset(Word op); -> clear recR*M and reset recPtr
	}
	
	char *p = NULL;
	Dword tpc = (PC & 0x1fffffff);
	if ((tpc >= 0x0c000000) && (tpc < 0x0d000000)) {
		p = (char*)(recRAM+((tpc-0x0c000000)<<1));
	} else if ((tpc >= 0x00000000) && (tpc < 0x00200000)) {
		p = (char*)(recROM+((tpc-0x00000000)<<1));
	}
	else { debugger->flamingDeath("Access to illegal mem!\n"); }
	
	*((Dword*)p) = (Dword)recPtr;
	recPC = PC;
	
	// write function header
	PUSHA32(Word op);
	
	unsigned long i = 0;
	for (; i<1000; i++) {
		d = mmu->fetchInstruction(recPC);
		(this->*sh_recompile_jump_table[d])(d);
		
		if (branch) {
			break;
		}
		recPC += 2;
	}
	
	// write function end
	ADD32ItoM((unsigned long)&numIterations, (unsigned long)i);
	POPA32(Word op);
	RET(Word op);
	branch = 0;
	
	return 0;
}

void SHCpu::go_rec(Word op)
{
	opcode_handler_struct *ostruct;
	int i;
	
	recMem = new char[0x800000]; // 8mb
	recRAM = new char[0x1000000*2]; // 32mb (2*16mb -> sh-opcode word size)
	recROM = new char[0x200000*2];
	if (recRAM == NULL || recROM == NULL || recMem == NULL) {
		printf("Error allocating memory"); exit(-1);
	}
	
	recPtr = recMem;
	
	printf("Building Jumptable\n");	
	for(i = 0; i < 0x10000; i++)
	{
		/* default to native */
		sh_recompile_jump_table[i] = &SHCpu::recNativeOpcode;
	}
    
	ostruct = sh_recompile_handler_table;
	do
	{
		for(i = 0;i < 0x10000;i++)
		{
			if((i & ostruct->mask) == ostruct->match)
			{
				sh_recompile_jump_table[i] = ostruct->opcode_handler;
			}
		}
		ostruct++;
	} while(ostruct->match != 0xfffd);
	sh_recompile_jump_table[0xfffd] = &SHCpu::recHook;
    
	for(i = 0; i < 0x10000; i++)
	{
		/* default to illegal */
		sh_instruction_jump_table[i] = &SHCpu::unknownOpcode;
	}
    
	ostruct = sh_opcode_handler_table;
	do
	{
		for(i = 0;i < 0x10000;i++)
		{
			if((i & ostruct->mask) == ostruct->match)
			{
				sh_instruction_jump_table[i] = ostruct->opcode_handler;
			}
		}
		ostruct++;
	} while(ostruct->match != 0xfffd);
	sh_instruction_jump_table[0xfffd] = &SHCpu::dispatchSwirlyHook;
    
	printf("Start execution\n");	

	for(;;)
	{
		if(debugger->prompt(Word op))
		{
		    
			void (**recFunc)(Word op);
			char *p;
		    
			Dword tpc = (PC & 0x1fffffff);
			if ((tpc >= 0x0c000000) && (tpc < 0x0d000000)) {
				p = (char*)(recRAM+((tpc-0x0c000000)<<1));
			} else if ((tpc >= 0x00000000) && (tpc < 0x00200000)) {
				p = (char*)(recROM+((tpc-0x00000000)<<1));
			}
			else { printf("Access to illegal mem: %08x\n", tpc); return; }
			
			recFunc = (void (**)(Word op)) (Dword)p;
			if (*recFunc) {
				(*recFunc)(Word op);
			} else {
				build_cl(Word op);
				(*recFunc)(Word op);
			}
			
			checkInterrupt();
		}
	} // for
}

void SHCpu::recNativeOpcode(Word op)
{
	PUSH32I  ((unsigned long)op);	// XXX: op only 16bit
	PUSH32I  ((unsigned long)this);	
	MOV32ItoR(EAX, (unsigned long)(this->*sh_instruction_jump_table[op]));	
	CALL32R  (EAX);
	ADD32ItoR(ESP, 8);
}

void SHCpu::recBranch(Word op)
{
	PUSH32I  ((unsigned long)op);	// XXX: op only 16bit
	PUSH32I  ((unsigned long)this);	
	MOV32ItoR(EAX, (unsigned long)(this->*sh_instruction_jump_table[op]));	
	CALL32R  (EAX);
	ADD32ItoR(ESP, 8);
	branch = 1;
}

void SHCpu::recHook(Word op)
{
	PUSH32I  ((unsigned long)op);	// XXX: op only 16bit
	PUSH32I  ((unsigned long)this);	
	MOV32ItoR(EAX, (unsigned long)(this->*sh_instruction_jump_table[op]));	
	CALL32R  (EAX);
	ADD32ItoR(ESP, 8);
	recPC += 2;
}

void SHCpu::recNOP(Word op)
{
	//	PC+=2;
	ADD32ItoM((unsigned long)&PC, 0x2);
}

void SHCpu::recMOV(Word op)
{
	int m = getM(op), n = getN(op);
	//	R[n]=R[m];
	MOV32MtoR(EBX, (unsigned long)&R);
	MOV32R8toR(EAX, EBX, m*4);
	MOV32RtoR8(EBX, n*4, EAX);
	//	PC+=2;
	ADD32ItoM((unsigned long)&PC, 0x2);
}
#endif
