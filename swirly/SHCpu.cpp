// SHCpu.cpp: interpretive SH4 core
//
//////////////////////////////////////////////////////////////////////

#include <math.h>

#include <map>
using namespace std;

#include "SHCpu.h"
#include "SHMmu.h"
#include "SHDmac.h"
#include "Debugger.h"
#include "Overlord.h"
#include "Gpu.h"
#include "dcdis.h"
#include "ix86.h"

float shfpu_sop0, shfpu_sop1, shfpu_sresult;
double shfpu_dop0, shfpu_dop1, shfpu_dresult;

void (SHCpu::*sh_instruction_jump_table[0x10000])(Word);
void (SHCpu::*sh_recompile_jump_table[0x10000])(Word);

// used for Swirly hooks
#define HOOK_GDROM 1
#define HOOK_LOAD1STREAD 2

static opcode_handler_struct sh_opcode_handler_table[] = {

	/*   function                      mask    match */
	{&SHCpu::ADDI                , 0xf000, 0x7000},
	{&SHCpu::BRA                 , 0xf000, 0xa000},
	{&SHCpu::BSR                 , 0xf000, 0xb000},
	{&SHCpu::MOVI                , 0xf000, 0xe000},
	{&SHCpu::MOVWI               , 0xf000, 0x9000},
	{&SHCpu::MOVLI               , 0xf000, 0xd000},
	{&SHCpu::MOVLS4              , 0xf000, 0x1000},
	{&SHCpu::MOVLL4              , 0xf000, 0x5000},
	{&SHCpu::LDCRBANK            , 0xf08f, 0x408e},
	{&SHCpu::LDCMRBANK           , 0xf08f, 0x4087},
	{&SHCpu::STCRBANK            , 0xf08f, 0x0082},
	{&SHCpu::STCMRBANK           , 0xf08f, 0x4083},
	{&SHCpu::FADD                , 0xf00f, 0xf000},
	{&SHCpu::FMUL                , 0xf00f, 0xf002},
	{&SHCpu::FMOV                , 0xf00f, 0xf00c},
	{&SHCpu::FMOV_STORE          , 0xf00f, 0xf00a},
	{&SHCpu::FMOV_LOAD           , 0xf00f, 0xf008},
	{&SHCpu::FMOV_RESTORE        , 0xf00f, 0xf009},
	{&SHCpu::FMOV_SAVE           , 0xf00f, 0xf00b},
	{&SHCpu::FMOV_INDEX_LOAD     , 0xf00f, 0xf006},
	{&SHCpu::FMOV_INDEX_STORE    , 0xf00f, 0xf007},
	{&SHCpu::FCMPEQ              , 0xf00f, 0xf004},
	{&SHCpu::FCMPGT              , 0xf00f, 0xf005},
	{&SHCpu::FDIV                , 0xf00f, 0xf003},
	{&SHCpu::FMAC                , 0xf00f, 0xf00e},
	{&SHCpu::FSUB                , 0xf00f, 0xf001},
	{&SHCpu::ADD                 , 0xf00f, 0x300c},
	{&SHCpu::ADDC                , 0xf00f, 0x300e},
	{&SHCpu::ADDV                , 0xf00f, 0x300f},
	{&SHCpu::AND                 , 0xf00f, 0x2009},
	{&SHCpu::CMPEQ               , 0xf00f, 0x3000},
	{&SHCpu::CMPGE               , 0xf00f, 0x3003},
	{&SHCpu::CMPGT               , 0xf00f, 0x3007},
	{&SHCpu::CMPHI               , 0xf00f, 0x3006},
	{&SHCpu::CMPHS               , 0xf00f, 0x3002},
	{&SHCpu::CMPSTR              , 0xf00f, 0x200c},
	{&SHCpu::DIV0S               , 0xf00f, 0x2007},
	{&SHCpu::DIV1                , 0xf00f, 0x3004},
	{&SHCpu::DMULS               , 0xf00f, 0x300d},
	{&SHCpu::DMULU               , 0xf00f, 0x3005},
	{&SHCpu::EXTSB               , 0xf00f, 0x600e},
	{&SHCpu::EXTSW               , 0xf00f, 0x600f},
	{&SHCpu::EXTUB               , 0xf00f, 0x600c},
	{&SHCpu::EXTUW               , 0xf00f, 0x600d},
	{&SHCpu::DO_MACL             , 0xf00f, 0x000f},
	{&SHCpu::MACW                , 0xf00f, 0x400f},
	{&SHCpu::MOV                 , 0xf00f, 0x6003},
	{&SHCpu::MOVBS               , 0xf00f, 0x2000},
	{&SHCpu::MOVWS               , 0xf00f, 0x2001},
	{&SHCpu::MOVLS               , 0xf00f, 0x2002},
	{&SHCpu::MOVBL               , 0xf00f, 0x6000},
	{&SHCpu::MOVWL               , 0xf00f, 0x6001},
	{&SHCpu::MOVLL               , 0xf00f, 0x6002},
	{&SHCpu::MOVBM               , 0xf00f, 0x2004},
	{&SHCpu::MOVWM               , 0xf00f, 0x2005},
	{&SHCpu::MOVLM               , 0xf00f, 0x2006},
	{&SHCpu::MOVBP               , 0xf00f, 0x6004},
	{&SHCpu::MOVWP               , 0xf00f, 0x6005},
	{&SHCpu::MOVLP               , 0xf00f, 0x6006},
	{&SHCpu::MOVBS0              , 0xf00f, 0x0004},
	{&SHCpu::MOVWS0              , 0xf00f, 0x0005},
	{&SHCpu::MOVLS0              , 0xf00f, 0x0006},
	{&SHCpu::MOVBL0              , 0xf00f, 0x000c},
	{&SHCpu::MOVWL0              , 0xf00f, 0x000d},
	{&SHCpu::MOVLL0              , 0xf00f, 0x000e},
	{&SHCpu::MULL                , 0xf00f, 0x0007},
	{&SHCpu::MULS                , 0xf00f, 0x200f},
	{&SHCpu::MULU                , 0xf00f, 0x200e},
	{&SHCpu::NEGC                , 0xf00f, 0x600a},
	{&SHCpu::NEG                 , 0xf00f, 0x600b},
	{&SHCpu::NOT                 , 0xf00f, 0x6007},
	{&SHCpu::OR                  , 0xf00f, 0x200b},
	{&SHCpu::SHAD                , 0xf00f, 0x400c},
	{&SHCpu::SHLD                , 0xf00f, 0x400d},
	{&SHCpu::SUB                 , 0xf00f, 0x3008},
	{&SHCpu::SUBC                , 0xf00f, 0x300a},
	{&SHCpu::SUBV                , 0xf00f, 0x300b},
	{&SHCpu::SWAPB               , 0xf00f, 0x6008},
	{&SHCpu::SWAPW               , 0xf00f, 0x6009},
	{&SHCpu::TST                 , 0xf00f, 0x2008},
	{&SHCpu::XOR                 , 0xf00f, 0x200a},
	{&SHCpu::XTRCT               , 0xf00f, 0x200d},
	{&SHCpu::MOVBS4              , 0xff00, 0x8000},
	{&SHCpu::MOVWS4              , 0xff00, 0x8100},
	{&SHCpu::MOVBL4              , 0xff00, 0x8400},
	{&SHCpu::MOVWL4              , 0xff00, 0x8500},
	{&SHCpu::ANDI                , 0xff00, 0xc900},
	{&SHCpu::ANDM                , 0xff00, 0xcd00},
	{&SHCpu::BF                  , 0xff00, 0x8b00},
	{&SHCpu::BFS                 , 0xff00, 0x8f00},
	{&SHCpu::BT                  , 0xff00, 0x8900},
	{&SHCpu::BTS                 , 0xff00, 0x8d00},
	{&SHCpu::CMPIM               , 0xff00, 0x8800},
	{&SHCpu::MOVBLG              , 0xff00, 0xc400},
	{&SHCpu::MOVWLG              , 0xff00, 0xc500},
	{&SHCpu::MOVLLG              , 0xff00, 0xc600},
	{&SHCpu::MOVBSG              , 0xff00, 0xc000},
	{&SHCpu::MOVWSG              , 0xff00, 0xc100},
	{&SHCpu::MOVLSG              , 0xff00, 0xc200},
	{&SHCpu::MOVA                , 0xff00, 0xc700},
	{&SHCpu::ORI                 , 0xff00, 0xcb00},
	{&SHCpu::ORM                 , 0xff00, 0xcf00},
	{&SHCpu::TRAPA               , 0xff00, 0xc300},
	{&SHCpu::TSTI                , 0xff00, 0xc800},
	{&SHCpu::TSTM                , 0xff00, 0xcc00},
	{&SHCpu::XORI                , 0xff00, 0xca00},
	{&SHCpu::XORM                , 0xff00, 0xce00},
	{&SHCpu::BRAF                , 0xf0ff, 0x0023},
	{&SHCpu::BSRF                , 0xf0ff, 0x0003},
	{&SHCpu::CMPPL               , 0xf0ff, 0x4015},
	{&SHCpu::CMPPZ               , 0xf0ff, 0x4011},
	{&SHCpu::DT                  , 0xf0ff, 0x4010},
	{&SHCpu::JSR                 , 0xf0ff, 0x400b},
	{&SHCpu::JMP                 , 0xf0ff, 0x402b},
	{&SHCpu::LDCSR               , 0xf0ff, 0x400e},
	{&SHCpu::LDCGBR              , 0xf0ff, 0x401e},
	{&SHCpu::LDCVBR              , 0xf0ff, 0x402e},
	{&SHCpu::LDCSSR              , 0xf0ff, 0x403e},
	{&SHCpu::LDCSPC              , 0xf0ff, 0x404e},
	{&SHCpu::LDCDBR              , 0xf0ff, 0x40fa},
	{&SHCpu::LDCMSR              , 0xf0ff, 0x4007},
	{&SHCpu::LDCMGBR             , 0xf0ff, 0x4017},
	{&SHCpu::LDCMVBR             , 0xf0ff, 0x4027},
	{&SHCpu::LDCMSSR             , 0xf0ff, 0x4037},
	{&SHCpu::LDCMSPC             , 0xf0ff, 0x4047},
	{&SHCpu::LDCMDBR             , 0xf0ff, 0x40f6},
	{&SHCpu::LDSMACH             , 0xf0ff, 0x400a},
	{&SHCpu::LDSMACL             , 0xf0ff, 0x401a},
	{&SHCpu::LDSPR               , 0xf0ff, 0x402a},
	{&SHCpu::LDSFPSCR            , 0xf0ff, 0x406a},
	{&SHCpu::LDSFPUL             , 0xf0ff, 0x405a},
	{&SHCpu::LDSMFPUL            , 0xf0ff, 0x4056},
	{&SHCpu::LDSMFPSCR           , 0xf0ff, 0x4066},
	{&SHCpu::LDSMMACH            , 0xf0ff, 0x4006},
	{&SHCpu::LDSMMACL            , 0xf0ff, 0x4016},
	{&SHCpu::LDSMPR              , 0xf0ff, 0x4026},
	{&SHCpu::MOVCAL              , 0xf0ff, 0x00c3},
	{&SHCpu::MOVT                , 0xf0ff, 0x0029},
	{&SHCpu::OCBI                , 0xf0ff, 0x0093},
	{&SHCpu::OCBP                , 0xf0ff, 0x00a3},
	{&SHCpu::OCBWB               , 0xf0ff, 0x00b3},
	{&SHCpu::PREF                , 0xf0ff, 0x0083},
	{&SHCpu::ROTCL               , 0xf0ff, 0x4024},
	{&SHCpu::ROTCR               , 0xf0ff, 0x4025},
	{&SHCpu::ROTL                , 0xf0ff, 0x4004},
	{&SHCpu::ROTR                , 0xf0ff, 0x4005},
	{&SHCpu::SHAL                , 0xf0ff, 0x4020},
	{&SHCpu::SHAR                , 0xf0ff, 0x4021},
	{&SHCpu::SHLL                , 0xf0ff, 0x4000},
	{&SHCpu::SHLL2               , 0xf0ff, 0x4008},
	{&SHCpu::SHLL8               , 0xf0ff, 0x4018},
	{&SHCpu::SHLL16              , 0xf0ff, 0x4028},
	{&SHCpu::SHLR                , 0xf0ff, 0x4001},
	{&SHCpu::SHLR2               , 0xf0ff, 0x4009},
	{&SHCpu::SHLR8               , 0xf0ff, 0x4019},
	{&SHCpu::SHLR16              , 0xf0ff, 0x4029},
	{&SHCpu::STCSR               , 0xf0ff, 0x0002},
	{&SHCpu::STCGBR              , 0xf0ff, 0x0012},
	{&SHCpu::STCVBR              , 0xf0ff, 0x0022},
	{&SHCpu::STCSSR              , 0xf0ff, 0x0032},
	{&SHCpu::STCSPC              , 0xf0ff, 0x0042},
	{&SHCpu::STCSGR              , 0xf0ff, 0x003a},
	{&SHCpu::STCDBR              , 0xf0ff, 0x00fa},
	{&SHCpu::STCMSR              , 0xf0ff, 0x4003},
	{&SHCpu::STCMGBR             , 0xf0ff, 0x4013},
	{&SHCpu::STCMVBR             , 0xf0ff, 0x4023},
	{&SHCpu::STCMSSR             , 0xf0ff, 0x4033},
	{&SHCpu::STCMSPC             , 0xf0ff, 0x4043},
	{&SHCpu::STCMSGR             , 0xf0ff, 0x4032},
	{&SHCpu::STCMDBR             , 0xf0ff, 0x40f2},
	{&SHCpu::STSMACH             , 0xf0ff, 0x000a},
	{&SHCpu::STSMACL             , 0xf0ff, 0x001a},
	{&SHCpu::STSPR               , 0xf0ff, 0x002a},
	{&SHCpu::STSFPSCR            , 0xf0ff, 0x006a}, 
	{&SHCpu::STSFPUL             , 0xf0ff, 0x005a},
	{&SHCpu::STSMMACH            , 0xf0ff, 0x4002},
	{&SHCpu::STSMMACL            , 0xf0ff, 0x4012},
	{&SHCpu::STSMPR              , 0xf0ff, 0x4022},
	{&SHCpu::STSMFPSCR           , 0xf0ff, 0x4062},
	{&SHCpu::STSMFPUL            , 0xf0ff, 0x4052},
	{&SHCpu::TAS                 , 0xf0ff, 0x401b},
	{&SHCpu::FLDI0               , 0xf0ff, 0xf08d},
	{&SHCpu::FLDI1               , 0xf0ff, 0xf09d},
	{&SHCpu::FLOAT               , 0xf0ff, 0xf02d},
	{&SHCpu::FLDS                , 0xf0ff, 0xf01d},
	{&SHCpu::FNEG                , 0xf0ff, 0xf04d},
	{&SHCpu::FSQRT               , 0xf0ff, 0xf06d},
	{&SHCpu::FCNVDS              , 0xf0ff, 0xf0bd},
	{&SHCpu::FCNVSD              , 0xf0ff, 0xf0ad},
	{&SHCpu::FTRC                , 0xf0ff, 0xf03d},
	{&SHCpu::FSTS                , 0xf0ff, 0xf00d},
	{&SHCpu::FSRRA               , 0xf0ff, 0xf07d},
	{&SHCpu::CLRMAC              , 0xffff, 0x0028},
	{&SHCpu::CLRS                , 0xffff, 0x0048},
	{&SHCpu::CLRT                , 0xffff, 0x0008},
	{&SHCpu::DIV0U               , 0xffff, 0x0019},
	{&SHCpu::NOP                 , 0xffff, 0x0009},
	{&SHCpu::RTE                 , 0xffff, 0x002b},
	{&SHCpu::RTS                 , 0xffff, 0x000b},
	{&SHCpu::SETS                , 0xffff, 0x0058},
	{&SHCpu::SETT                , 0xffff, 0x0018},
	{&SHCpu::LDTLB               , 0xffff, 0x0038},
	{&SHCpu::SLEEP               , 0xffff, 0x001b},
	{&SHCpu::FSCA                , 0xf1ff, 0xf0fd}, // XXX: is this correct ???
	{&SHCpu::FTRV                , 0xf3ff, 0xf1fd},
	{&SHCpu::FRCHG               , 0xffff, 0xfbfd},
	{&SHCpu::FSCHG               , 0xffff, 0xf3fd},
	{&SHCpu::dispatchSwirlyHook  , 0xffff, 0xfffd}  // last entry
};

static opcode_handler_struct sh_recompile_handler_table[] =
{
	/*   function                      mask    match */
	{&SHCpu::recBranch           , 0xf0ff, 0x400b},
	{&SHCpu::recBranch           , 0xf000, 0xa000},
	{&SHCpu::recBranch           , 0xff00, 0x8b00},
	{&SHCpu::recBranch           , 0xff00, 0x8f00},
	{&SHCpu::recBranch           , 0xff00, 0x8900},
	{&SHCpu::recBranch           , 0xff00, 0x8d00},
	{&SHCpu::recBranch           , 0xf000, 0xb000},
	{&SHCpu::recBranch           , 0xff00, 0xc300},
	{&SHCpu::recBranch           , 0xf0ff, 0x0023},
	{&SHCpu::recBranch           , 0xf0ff, 0x0003},
	{&SHCpu::recBranch           , 0xf0ff, 0x402b},
	{&SHCpu::recBranch           , 0xffff, 0x002b},
	{&SHCpu::recBranch           , 0xffff, 0x000b},
	{&SHCpu::recNOP              , 0xffff, 0x0009},
	{&SHCpu::recMOV              , 0xf00f, 0x6003},
	{&SHCpu::recHook             , 0xffff, 0xfffd}	// last entry
};


SHCpu::SHCpu(Dword setting)
{
	
	mmu = new SHMmu(this);
	modem = new Modem(this);
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
	maple = new Maple(this, setting);
	overlord = new Overlord(this);
	branch = 0;
	reset();
}

SHCpu::~SHCpu()
{
	delete modem;
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
	delete overlord;
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
	debugger->print("SHCpu: encountered unknown upcode at PC=%08X\n", PC);
	exception(E_GENERAL_ILLEGAL_INSTRUCTION, PC, op, "Offending opcode");
	PC+=2;
}

void SHCpu:: XTRCT(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t;
	t=(R[m]<<16)&0xffff0000;
	R[n]=(R[n]>>16)&0xffff;
	R[n]|=t;
	PC+=2;
}

void SHCpu:: XORI(Word op)
{
	R[0]^=(op&0xff);
	PC+=2;
}

void SHCpu:: XORM(Word op)
{
	Dword t;
	t=mmu->readByte(GBR+R[0]);
	t^=(op&0xff);
	mmu->writeByte(GBR+R[0], t);
	PC+=2;
}

void SHCpu:: XOR(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]^=R[m];
	PC+=2;
}

void SHCpu:: TST(Word op)
{
	int m = getM(op), n = getN(op);
	if((R[n]&R[m])==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu:: TSTI(Word op)
{
	Dword t;
	t=R[0]&(0xff&op);
	if(t==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu:: TSTM(Word op)
{
	Dword t;
	t=mmu->readByte(GBR+R[0]);
	t&=(op&0xff);
	if(t==0)T=1;
	else T=0;
	PC+=2;
}

void SHCpu:: TRAPA(Word op)
{
	/* TODO: this gets uncommented when we do exceptions*/
	SSR=SR;
	SPC=PC+2;
	setSR(SR|F_SR_MD|F_SR_BL|F_SR_RB);
	mmu->writeDword(0xff000000|TRA, (op&0xff)<<2);
	mmu->writeDword(0xff000000|EXPEVT, 0x160);
	PC=VBR+0x100;
}

void SHCpu:: TAS(Word op)
{
	int n = getN(op);
	Sdword t, oldt;
	oldt=t=(Sdword)mmu->readByte(R[n]);
	t|=0x80;
	mmu->writeByte(R[n],t);
	if(oldt==0)T=1; else T=0;
	PC+=2;
}

void SHCpu:: SWAPW(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t;
	t=(R[m]>>16)&0xffff;
	R[n]=R[m]<<16;
	R[n]|=t;
	PC+=2;
}

void SHCpu:: SWAPB(Word op)
{
	int m = getM(op), n = getN(op);
	Dword t0, t1;
	t0=R[m]&0xffff0000;
	t1=(R[m]&0xff)<<8;
	R[n]=(R[m]&0xff00)>>8;
	R[n]=R[n]|t1|t0;
	PC+=2;
}

void SHCpu:: SUBV(Word op)
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

void SHCpu:: SUBC(Word op)
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

void SHCpu:: SUB(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]-=R[m];
	PC+=2;
}

void SHCpu:: STSMACH(Word op)
{
	int n = getN(op);
	R[n]=MACH;
	PC+=2;
}

void SHCpu:: STSMACL(Word op)
{
	int n = getN(op);
	R[n]=MACL;
	PC+=2;
}

void SHCpu:: STSPR(Word op)
{
	int n = getN(op);
	R[n]=PR;
	PC+=2;
}

void SHCpu:: STSMMACH(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, MACH);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STSMMACL(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, MACL);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STSMPR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, PR);
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
	R[n]-=4;
	mmu->writeDword(R[n], FPSCR & 0x003fffff);
	PC+=2;
}

void SHCpu:: STSMFPUL(Word op)
{
	int n = getN(op);
	R[n]-=4;
	mmu->writeDword(R[n], FPUL);
	PC+=2;
}

void SHCpu:: STCSR(Word op) // privileged
{
	int n = getN(op);
	R[n]=SR;
	PC+=2;
}

void SHCpu:: STCGBR(Word op)
{
	int n = getN(op);
	R[n]=GBR;
	PC+=2;
}

void SHCpu:: STCVBR(Word op)
{
	int n = getN(op);
	R[n]=VBR;
	PC+=2;
}

void SHCpu:: STCSSR(Word op)
{
	int n = getN(op);
	R[n]=SSR;
	PC+=2;
}

void SHCpu:: STCSPC(Word op)
{
	int n = getN(op);
	R[n]=SPC;
	PC+=2;
}

void SHCpu:: STCSGR(Word op)
{
	int n = getN(op);
	R[n]=SGR;
	PC+=2;
}


void SHCpu:: STCDBR(Word op)
{
	int n = getN(op);
	R[n]=DBR;
	PC+=2;
}

void SHCpu:: STCRBANK(Word op)
{
	int m = getM(op)&0x7, n = getN(op);
	R[n]=RBANK[m];
	PC+=2;
}

void SHCpu:: STCMSR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, SR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMGBR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, GBR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMVBR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, VBR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMSSR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, SSR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMSPC(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, SPC);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMSGR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, SGR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMDBR(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n]-4, DBR);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: STCMRBANK(Word op)
{
	int m = getM(op)&0x7, n = getN(op);
	mmu->writeDword(R[n]-4, RBANK[m]);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: SLEEP(Word op)
{
	// some programs save SPC/SSR to stack beforehand. 
	// basically do nothing here. wait forever for an interrupt or reset.
	PC+=2; // hack
	
	return;
}

void SHCpu:: SHLR(Word op)
{
	int n = getN(op);
	if((R[n]&1)==0) T=0;
	else T=1;
	R[n]>>=1;
	R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu:: SHLR16(Word op)
{
	int n = getN(op);
	R[n]>>=16;
	PC+=2;
}

void SHCpu:: SHLR8(Word op)
{
	int n = getN(op);
	R[n]>>=8;
	PC+=2;
}

void SHCpu:: SHLR2(Word op)
{
	int n = getN(op);
	R[n]>>=2;
	PC+=2;
}

void SHCpu:: SHLL16(Word op)
{
	int n = getN(op);
	R[n]<<=16;
	PC+=2;
}

void SHCpu:: SHLL8(Word op)
{
	int n = getN(op);
	R[n]<<=8;
	PC+=2;
}

void SHCpu:: SHLL2(Word op)
{
	int n = getN(op);
	R[n]<<=2;
	PC+=2;
}

void SHCpu:: SHLL(Word op)
{
	int n = getN(op);
	if((R[n]&0x80000000)==0) T=0; else T=1;
	R[n]<<=1;
	PC+=2;
}

void SHCpu:: SHLD(Word op)
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

void SHCpu:: SHAR(Word op)
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

void SHCpu:: SHAL(Word op)
{
	int n = getN(op);
	if((R[n] & 0x80000000) == 0) T=0;
	else T=1;
	R[n] <<=1;
	PC+=2;
}

void SHCpu:: SHAD(Word op)
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

void SHCpu:: SETT(Word op)
{
	T=1;
	PC+=2;
}

void SHCpu:: SETS(Word op)
{
	S=1;
	PC+=2;
}

void SHCpu:: RTS(Word op)
{
	delaySlot();
	debugger->reportBranch("rts", PC, PR);
	PC=PR;
}	

void SHCpu:: RTE(Word op) // privileged
{
	delaySlot();
	setSR(SSR);
	debugger->reportBranch("rte", PC, SPC);
	PC=SPC;
}

void SHCpu:: ROTR(Word op)
{
	int n = getN(op);
	if((R[n]&1)==0)T=0;
	else T=1;
	R[n]>>=1;
	if(T==1) R[n]|=0x80000000;
	else R[n]&=0x7fffffff;
	PC+=2;
}

void SHCpu:: ROTL(Word op)
{
	int n = getN(op);
	if((R[n]&0x80000000)==0)T=0;else T=1;
	R[n]<<=1;
	if(T==1)R[n]|=1;
	else R[n]&=0xfffffffe;
	PC+=2;
}

void SHCpu:: ROTCR(Word op)
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

void SHCpu:: ROTCL(Word op)
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

void SHCpu:: OR(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] |= R[m];
	PC+=2;
}

void SHCpu:: ORI(Word op)
{
	R[0] |= (Dword) (Byte) (op&0xff);
	PC+=2;
}

void SHCpu:: ORM(Word op)
{
	mmu->writeByte(GBR+R[0], ((Dword) mmu->readByte(R[0]+GBR)) | (Dword) (Byte) (op&0xff));
	PC+=2;
}

void SHCpu:: OCBWB(Word op)
{
	//int n = getN(op);
	// XXX: should this do something?
	PC+=2;
}

void SHCpu:: OCBP(Word op)
{
	//int n = getN(op);
	// XXX: should this do something?
	PC+=2;
}

void SHCpu:: OCBI(Word op)
{
	//int n = getN(op);
	// XXX: should this do something?
	PC+=2;
}

void SHCpu:: NOT(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]= ~R[m];
	PC+=2;
}

void SHCpu:: NOP(Word op)
{
	PC+=2;
}

void SHCpu:: NEGC(Word op)
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

void SHCpu:: NEG(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]=0-R[m];
	PC+=2;
}

void SHCpu:: MULU(Word op)
{
	int m = getM(op), n = getN(op);
	MACL=((Dword)(Word)R[n]*(Dword)(Word)R[m]);
	PC+=2;
}

void SHCpu:: MULS(Word op)
{
	int m = getM(op), n = getN(op);
	MACL=((Sdword)(Sword)R[n]*(Sdword)(Sword)R[m]);
	PC+=2;
}

void SHCpu:: MULL(Word op)
{
	int m = getM(op), n = getN(op);
	MACL=R[m]*R[n];
	PC+=2;
}

void SHCpu:: MOVT(Word op)
{
	int n = getN(op);
	R[n]=(SR&1);
	PC+=2;
}

void SHCpu:: MOVCAL(Word op)
{
	int n = getN(op);
	mmu->writeDword(R[n], R[0]);
	PC+=2;
}

void SHCpu:: MOVA(Word op)
{
	Dword dest;
	dest = ((op&0xff)<<2)+4+(PC&0xfffffffc);
	R[0]=dest;
	PC+=2;
}

void SHCpu:: MOVBS4(Word op)
{
	int n = getM(op);
	Dword dest;
	dest=(op&0xf)+R[n];
	mmu->writeByte(dest, R[0]);
	PC+=2;
}

void SHCpu:: MOVWS4(Word op)
{
	int n = getM(op);
	Dword dest;
	dest=((op&0xf)<<1)+R[n];
	mmu->writeWord(dest, R[0]);
	PC+=2;
}

void SHCpu:: MOVLS4(Word op)
{
	int m = getM(op), n = getN(op);
	Dword dest;
	dest=((op&0xf)<<2)+R[n];
	mmu->writeDword(dest, R[m]);
	PC+=2;
}

void SHCpu:: MOVBL4(Word op)
{
	int m = getM(op);
	Dword dest;
	dest = (op&0xf)+R[m];
	R[0] = (Sdword) (Sbyte) mmu->readByte(dest);
	PC+=2;
}

void SHCpu:: MOVWL4(Word op)
{
	int m = getM(op);
	Dword dest;
	dest = ((op&0xf)<<1)+R[m];
	R[0] = (Sdword) (Sword) mmu->readWord(dest);
	PC+=2;
}

void SHCpu:: MOVLL4(Word op)
{
	int m = getM(op), n = getN(op);
	Dword dest;
	dest=((op&0xf)<<2)+R[m];
	R[n]=mmu->readDword(dest);
	PC+=2;
}


void SHCpu:: MOVBLG(Word op)
{
	Dword dest;
	dest = (Dword)(op&0xff)+GBR;
	R[0]=(Sdword)(Sbyte)mmu->readByte(dest);
	PC+=2;
}

void SHCpu:: MOVWLG(Word op)
{
	Dword dest;
	dest = ((Dword)(op&0xff)<<1)+GBR;
	R[0]=(Sdword)(Sword)mmu->readWord(dest);
	PC+=2;
}

void SHCpu:: MOVLLG(Word op)
{
	Dword dest;
	dest=((Dword)(op&0xff)<<2)+GBR;
	R[0]=mmu->readDword(dest);
	PC+=2;
}

void SHCpu:: MOVBSG(Word op)
{
	Dword dest;
	dest = (Dword)(op&0xff)+GBR;
	mmu->writeByte(dest, (Byte)R[0]);
	PC+=2;
}

void SHCpu:: MOVWSG(Word op)
{
	Dword dest;
	dest = ((Dword)(op&0xff)<<1)+GBR;
	mmu->writeWord(dest, (Word)R[0]);
	PC+=2;
}

void SHCpu:: MOVLSG(Word op)
{
	Dword dest;
	dest = ((Dword)(op&0xff)<<2)+GBR;
	mmu->writeDword(dest, R[0]);
	PC+=2;
}

void SHCpu:: MOVI(Word op)
{
	int n = getN(op);
	R[n] = (Sdword) (Sbyte) (op & 0xff);
	PC+=2;
}

void SHCpu:: MOVWI(Word op)
{
	int n = getN(op);
	Dword dest;
	dest = (((Dword)(op&0xff)) << 1)+4+PC;
	R[n]=(Sdword) (Sword) mmu->readWord(dest);
	PC+=2;
}

void SHCpu:: MOVLI(Word op)
{
	int n = getN(op);
	Dword dest;
	dest = (((Dword)(op&0xff)) << 2)+4+(PC&0xfffffffc);
	R[n]=mmu->readDword(dest);
	PC+=2;
}

void SHCpu:: MOV(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]=R[m];
	PC+=2;
}

void SHCpu:: MOVBS(Word op) // mov.b Rm, @Rn
{
	int m = getM(op), n = getN(op);
	mmu->writeByte(R[n], (Byte)R[m]);
	PC+=2;
}

void SHCpu:: MOVWS(Word op)
{
	int m = getM(op), n = getN(op);
	mmu->writeWord(R[n], (Word)R[m]);
	PC+=2;
}

void SHCpu:: MOVLS(Word op)
{
	int m = getM(op), n = getN(op);
	mmu->writeDword(R[n], R[m]);
	PC+=2;
}

void SHCpu:: MOVBL(Word op) // mov.b @Rm, Rn
{
	int m = getM(op), n = getN(op);
	R[n]=(Sdword) (Sbyte) mmu->readByte(R[m]); // sign-extend
	PC+=2;
}

void SHCpu:: MOVWL(Word op) // mov.w @Rm, Rn
{
	int m = getM(op), n = getN(op);
	R[n]=(Sdword) (Sword) mmu->readWord(R[m]); // sign-extend
	PC+=2;
}

void SHCpu:: MOVLL(Word op) // mov.l @Rm, Rn
{
	int m = getM(op), n = getN(op);
	R[n]= mmu->readDword(R[m]);
	//printf("PC is %08x, new PC is %08x\n", PC, PC+2);
	PC+=2;
}

void SHCpu:: MOVBM(Word op) // mov.b Rm, @-Rn
{
	int m = getM(op), n = getN(op);
	mmu->writeByte(R[n]-1, (Byte) R[m]);
	R[n]--;
	PC+=2;
}

void SHCpu:: MOVWM(Word op)
{
	int m = getM(op), n = getN(op);
	mmu->writeWord(R[n]-2, (Word) R[m]);
	R[n]-=2;
	PC+=2;
}

void SHCpu:: MOVLM(Word op)
{
	int m = getM(op), n = getN(op);
	mmu->writeDword(R[n]-4, R[m]);
	R[n]-=4;
	PC+=2;
}

void SHCpu:: MOVBP(Word op) // mov.b @Rm+, Rn
{
	int m = getM(op), n = getN(op);
	R[n]=(Sdword) (Sbyte) mmu->readByte(R[m]); // sign-extend
	if(n!=m) R[m]++;
	PC+=2;
}

void SHCpu:: MOVWP(Word op) // mov.w @Rm+, Rn
{
	int m = getM(op), n = getN(op);
	R[n]=(Sdword) (Sword) mmu->readWord(R[m]); // sign-extend
	if(n!=m) R[m]+=2;
	PC+=2;
}

void SHCpu:: MOVLP(Word op) // mov.l @Rm+, Rn
{
	int m = getM(op), n = getN(op);
	R[n]= mmu->readDword(R[m]);
	if(n!=m) R[m]+=4;
	PC+=2;
}

void SHCpu:: MOVBS0(Word op) // mov.b Rm, @(r0, Rn)
{
	int m = getM(op), n = getN(op);
	mmu->writeByte(R[n]+R[0], R[m]);
	PC+=2;
}

void SHCpu:: MOVWS0(Word op) // mov.w Rm, @(r0, Rn)
{
	int m = getM(op), n = getN(op);
	mmu->writeWord(R[n]+R[0], R[m]);
	PC+=2;
}

void SHCpu:: MOVLS0(Word op) // mov.l Rm, @(r0, Rn)
{
	int m = getM(op), n = getN(op);
	mmu->writeDword(R[n]+R[0], R[m]);
	PC+=2;
}

void SHCpu:: MOVBL0(Word op) // mov.b @(r0, rm), rn
{
	int m = getM(op), n = getN(op);
	R[n] = (Sdword) (Sbyte) mmu->readByte(R[0]+R[m]);
	PC+=2;
}

void SHCpu:: MOVWL0(Word op) // mov.w @(r0, rm), rn
{
	int m = getM(op), n = getN(op);
	R[n] = (Sdword) (Sword) mmu->readWord(R[0]+R[m]);
	PC+=2;
}

void SHCpu:: MOVLL0(Word op) // mov.l @(r0, rm), rn
{
	int m = getM(op), n = getN(op);
	R[n] = mmu->readDword(R[0]+R[m]);
	PC+=2;
}

void SHCpu:: MACW(Word op)
{
	int m = getM(op), n = getN(op);
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

void SHCpu:: DO_MACL(Word op)
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

void SHCpu:: LDSFPSCR(Word op)
{
	int n = getN(op);
	setFPSCR(R[n] & 0x003fffff);
	PC+=2;
}

void SHCpu::LDSMFPSCR(Word op)
{
	int n = getN(op);
	setFPSCR(mmu->readDword(R[n]) & 0x003fffff);
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
	FPUL = mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSMPR(Word op)
{
	int n = getN(op);
	PR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSMMACL(Word op)
{
	int n = getN(op);
	MACL=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSMMACH(Word op)
{
	int n = getN(op);
	MACH=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDSPR(Word op)
{
	int n = getN(op);
	PR=R[n];
	PC+=2;
}

void SHCpu:: LDSMACL(Word op)
{
	int n = getN(op);
	MACL=R[n];
	PC+=2;
}

void SHCpu:: LDSMACH(Word op)
{
	int n = getN(op);
	MACH=R[n];
	PC+=2;
}

void SHCpu:: LDCSR(Word op) // privileged
{
	int n = getN(op);
	setSR(R[n]&0x700083f3);
	PC+=2;
}

void SHCpu:: LDCGBR(Word op)
{
	int n = getN(op);
	GBR=R[n];
	PC+=2;
}

void SHCpu:: LDCVBR(Word op) // privileged
{
	int n = getN(op);
	VBR=R[n];
	PC+=2;
}

void SHCpu:: LDCSSR(Word op) // privileged
{
	int n = getN(op);
	SSR=R[n];
	PC+=2;
}

void SHCpu:: LDCSPC(Word op) // privileged
{
	int n = getN(op);
	SPC=R[n];
	PC+=2;
}

void SHCpu:: LDCDBR(Word op) // privileged
{
	int n = getN(op);
	DBR=R[n];
	PC+=2;
}

void SHCpu:: LDCRBANK(Word op) // privileged
{
	int m = getM(op)&0x7, n = getN(op);
	// n = source, m = dest
	RBANK[m]=R[n];
	PC+=2;
}

void SHCpu:: LDCMRBANK(Word op) // privileged
{
	int m = getM(op)&0x7, n = getN(op);
	// n = source, m = dest
	RBANK[m]=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMSR(Word op)
{
	int n = getN(op);
	setSR(mmu->readDword(R[n])&0x700083f3);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMGBR(Word op)
{
	int n = getN(op);
	GBR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMVBR(Word op)
{
	int n = getN(op);
	VBR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}


void SHCpu:: LDCMSSR(Word op)
{
	int n = getN(op);
	SSR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMSPC(Word op)
{
	int n = getN(op);
	SPC=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: LDCMDBR(Word op)
{
	int n = getN(op);
	DBR=mmu->readDword(R[n]);
	R[n]+=4;
	PC+=2;
}

void SHCpu:: JSR(Word op)
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

void SHCpu:: JMP(Word op)
{
	int n = getN(op);
	Dword temp;
	temp = R[n];
	delaySlot();
	
	debugger->reportBranch("jmp", PC, temp);
	PC=temp;
}	

void SHCpu:: DMULU(Word op)
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

void SHCpu:: EXTUW(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] = R[m] & 0xffff;
	PC+=2;
}

void SHCpu:: EXTUB(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] = R[m] & 0xff;
	PC+=2;
}

void SHCpu:: EXTSB(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]=	((R[m]&0x80)==0) ? R[m]&0xff : R[m] | 0xffffff00;
	PC+=2;
}

void SHCpu:: EXTSW(Word op)
{
	int m = getM(op), n = getN(op);
	R[n]= ((R[m]&0x8000)==0) ? R[m]&0xffff : R[m] | 0xffff0000;
	PC+=2;
}

void SHCpu:: DT(Word op)
{
	int n = getN(op);
	R[n]--;
	if(R[n]==0)
		T=1;
	else
		T=0;
	PC+=2;
}

void SHCpu:: DMULS(Word op)
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

void SHCpu:: DIV1(Word op)
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

void SHCpu:: DIV0U(Word op)
{
	setSR(SR & ((~F_SR_M)&(~F_SR_Q)&(~F_SR_T)));
	PC+=2;
}

void SHCpu:: DIV0S(Word op)
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

void SHCpu:: CMPIM(Word op)
{
	if((Sdword)R[0] == (Sdword)(Sbyte)(op&0xff))
		setSR(SR|F_SR_T);
	else
		setSR(SR& ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPSTR(Word op)
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
void SHCpu:: CMPPZ(Word op)
{
	int n = getN(op);
	if((signed int)R[n] >= 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPPL(Word op)
{
	int n = getN(op);
	if((signed int)R[n] > 0)
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPHS(Word op)
{
	int m = getM(op), n = getN(op);
	if(R[n] >= R[m]) 
		setSR(SR | F_SR_T);
	else 
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPHI(Word op)
{
	int m = getM(op), n = getN(op);
	if(R[n] > R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPGT(Word op)
{
	int m = getM(op), n = getN(op);
	
	if((Sdword)R[n] > (Sdword)R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPGE(Word op)
{
	int m = getM(op), n = getN(op);
	if((Sdword)R[n] >= (Sdword)R[m])
		setSR(SR | F_SR_T);
	else
		setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CMPEQ(Word op)
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

void SHCpu:: CLRT(Word op)
{
	setSR(SR & ~F_SR_T);
	PC+=2;
}

void SHCpu:: CLRS(Word op)
{
	setSR(SR & ~F_SR_S);
	PC+=2;
}

void SHCpu:: CLRMAC(Word op)
{
	MACH=MACL=0;
	PC+=2;
}

void SHCpu:: BTS(Word op)
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

void SHCpu:: BT(Word op) // no delay slot!
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

void SHCpu:: BSRF(Word op)
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

void SHCpu:: BSR(Word op)
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

void SHCpu:: BRAF(Word op)
{
	int n = getN(op);
	Dword dest;
	dest = PC+4+(Sdword)R[n];
	delaySlot();
	debugger->reportBranch("braf", PC, dest);
	PC=dest;
}

void SHCpu:: BRA(Word op)
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

void SHCpu:: BFS(Word op)
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

void SHCpu:: BF(Word op) // no delay slot!
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

void SHCpu:: AND(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] &= R[m];
	PC+=2;
}

void SHCpu:: ANDI(Word op)
{
	R[0] &= (Dword) (op&0xff);
	PC+=2;
}

void SHCpu:: ANDM(Word op) // and.b #imm, @(r0, gbr)
{
	mmu->writeByte(R[0]+GBR, ((Dword) (op&0xff)) & (Dword) mmu->readByte(R[0]+GBR));
	PC+=2;
}

void SHCpu:: ADDV(Word op)
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

void SHCpu:: ADD(Word op)
{
	int m = getM(op), n = getN(op);
	R[n] += R[m];
	PC+=2;
}

void SHCpu:: ADDI(Word op)
{
	int n = getN(op);
	R[n] += (Sdword) (Sbyte) (op&0xff); // sign-extend
	PC+=2;
}

void SHCpu:: ADDC(Word op)
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

void SHCpu::FABS(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
	
	if(FPU_DP()) // double-precision
		DR[n] = fabs(DR[n]);
	//*(((Qword*)DR)+n) &= 0x7fffffffffffffff;
	else // single-precision
		//FR[n] = (float) fabs((double) FR[n]);
		*(((Dword*) FR)+n) &= 0x7fffffff;
	PC+=2;
}

void SHCpu::FADD(Word op)
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
	    //printf("FADD: %f * %f = ", tmpa.d, tmpb.d);
	    tmpa.d += tmpb.d;
	    //printf("%f\n", tmpa.d);
	    FR_Dwords[n] = tmpa.i[1];
	    FR_Dwords[n+1] = tmpa.i[0];
	    //		DR[n]+=DR[m];
    } 
    else // single-precision
	    FR[n]+=FR[m];
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
		//		shfpu_sFMUL();
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
			mmu->writeDword(R[n], XF_Dwords[m]);
			mmu->writeDword(R[n]+4, XF_Dwords[m+1]);
		}
		else
		{
			mmu->writeDword(R[n], FR_Dwords[m]);
			mmu->writeDword(R[n]+4, FR_Dwords[m+1]);
		}
	}
	else // single-precision
		mmu->writeDword(R[n], FR_Dwords[m]);
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
			XF_Dwords[n] = mmu->readDword(R[m]);
			XF_Dwords[n+1] = mmu->readDword(R[m]+4);
		}
		else
		{
			FR_Dwords[n] = mmu->readDword(R[m]);
			FR_Dwords[n+1] = mmu->readDword(R[m]+4);
		}
	}
	else // single-precision
		FR_Dwords[n] = mmu->readDword(R[m]);
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
			XF_Dwords[n] = mmu->readDword(R[m]);
			XF_Dwords[n+1] = mmu->readDword(R[m]+4);
		}
		else
		{
			FR_Dwords[n] = mmu->readDword(R[m]);
			FR_Dwords[n+1] = mmu->readDword(R[m]+4);
		}
		R[m]+=8;
	}
	else // single-precision
	{
		FR_Dwords[n] = mmu->readDword(R[m]);
		R[m]+=4;
	}
	PC+=2;
}

void SHCpu::FMOV_SAVE(Word op)
{
	int m = getM(op), n = getN(op);
    
	if(FPU_SZ()) // double-precision
	{
		R[n]-=8;
		m &= 0xe;
		if (op & 0x0010)
		{
			mmu->writeDword(R[n], XF_Dwords[m]);
			mmu->writeDword(R[n]+4, XF_Dwords[m+1]);
		}
		else
		{
			mmu->writeDword(R[n], FR_Dwords[m]);
			mmu->writeDword(R[n]+4, FR_Dwords[m+1]);
		}
	}
	else // single-precision
	{
		R[n]-=4;
		mmu->writeDword(R[n], FR_Dwords[m]);
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
			XF_Dwords[n] = mmu->readDword(R[m]+R[0]);
			XF_Dwords[n+1] = mmu->readDword(R[m]+R[0]+4);
		}
		else
		{
			FR_Dwords[n] = mmu->readDword(R[m]+R[0]);
			FR_Dwords[n+1] = mmu->readDword(R[m]+R[0]+4);
		}
	}
	else // single-precision
		FR_Dwords[n] = mmu->readDword(R[m]+R[0]);
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
			mmu->writeDword(R[0]+R[n], XF_Dwords[m]);
			mmu->writeDword(R[0]+R[n]+4, XF_Dwords[m+1]);
		}
		else
		{
			mmu->writeDword(R[0]+R[n], FR_Dwords[m]);
			mmu->writeDword(R[0]+R[n]+4, FR_Dwords[m+1]);
		}
	}
	else // single-precision
		mmu->writeDword(R[0]+R[n], FR_Dwords[m]);
	PC+=2;
}

void SHCpu::FCMPEQ(Word op)
{
	int m = getM(op), n = getN(op);
	FPU_DP_FIX_MN();
	
	if(FPU_DP()) // double-precision
	{
		if(DR[m] == DR[n])
			setSR(SR | F_SR_T);
		else
			setSR(SR & ~F_SR_T);
	}
	else // single-precision
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
		// double-precision
		if(DR[n] > DR[m])
			setSR(SR | F_SR_T);
		else
			setSR(SR & ~F_SR_T);
	}
	else
	{
		// single-precision
		if(FR[n] > FR[m])
			setSR(SR | F_SR_T); 
		else
			setSR(SR & ~F_SR_T);
	}
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
		FR[n] /= FR[m];
	PC+=2;
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

void SHCpu::FLOAT(Word op)
{
	int n = getN(op);
	//	FPU_DP_FIX_N();
	if(FPU_DP()) 
        {
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

void SHCpu::FMAC(Word op)
{
	int m = getM(op), n = getN(op);
	FR[n] = (FR[0] * FR[m]) + FR[n];
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

void SHCpu::FNEG(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
	if(FPU_DP())
		DR[n] = -DR[n];
	else
		FR[n] = -FR[n];
	PC+=2;
}

void SHCpu::FRCHG(Word op)
{
	setFPSCR(FPSCR ^ 0x00200000);
	PC+=2;
}

void SHCpu::FSCHG(Word op)
{
	setFPSCR(FPSCR ^ 0x00100000);
	PC+=2;
}

void SHCpu::FSQRT(Word op)
{
	int n = getN(op);
	FPU_DP_FIX_N();
	if(FPU_DP())
		DR[n] = sqrt(DR[n]);
	else
		FR[n] = (float) sqrt((double) FR[n]);
	PC+=2;
}

void SHCpu::FSTS(Word op)
{
	int n = getN(op);
	FR_Dwords[n] = FPUL;
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
	// FIXME: should this work on DR regs as well?
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

void SHCpu::LDTLB(Word op)
{
	mmu->ldtlb();
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
	numIterations = 0;
}

void SHCpu::executeInstruction(Word d) 
{
	(this->*sh_instruction_jump_table[d])(d);
}

// starts executing instructions
void SHCpu::go() 
{

	int i;
	opcode_handler_struct *ostruct;
	
	printf("Building interpretative CPU jumptable\n");
	for(i = 0; i < 0x10000; i++) 
        {
		/* default to illegal */
		sh_instruction_jump_table[i] = &SHCpu::unknownOpcode;
	}
	
	ostruct = sh_opcode_handler_table;
	do {
		for(i=0;i<0x10000;i++) 
                {
			if((i&ostruct->mask) == ostruct->match) 
                        {
				sh_instruction_jump_table[i] = ostruct->opcode_handler;
			}
		}
		ostruct++;
	} while(ostruct->match != 0xfffd);
	
	sh_instruction_jump_table[0xfffd] = &SHCpu::dispatchSwirlyHook;
	
#define FRAME 0x8000
	
	addInterrupt(FRAME, 0);     // first SDL event
	addInterrupt(0x1000, 1);    // first VBL event
	
	currInstruction = mmu->fetchInstruction(PC);
	
	for(;;) 
        {
		if(debugger->prompt()) 
                {
			executeInstruction(currInstruction);
			numIterations++;
			checkInterrupt();
			currInstruction = mmu->fetchInstruction(PC);
		}
	}

}

void SHCpu::exception(Dword type, Dword addr, Dword data, char *datadesc) 
{

	exceptionsPending++;

	if(datadesc == 0) 
        {
		debugger->flamingDeath("Exception of type 0x%x (%s) at %08X",
				       type, debugger->getExceptionName(type),
				       addr, data);
	} 
	else 
	{
		debugger->flamingDeath("Exception of type 0x%x (%s) at %08X. %s: %08x",
				       type, debugger->getExceptionName(type),
				       addr, datadesc, data);
	}
}

typedef map<Dword, Dword> intr_map;
intr_map intrpts;

void SHCpu::addInterrupt(Dword at, Dword type)
{
	intrpts[numIterations + at] = type;
}

inline void SHCpu::checkInterrupt() 
{
	
	intr_map::iterator ci = intrpts.begin();
	while (ci->first <= numIterations) 
        {
		switch (ci->second) 
                {
		case 0: 		
			gpu->draw2DFrame();
			maple->controller->checkInput();
			addInterrupt(FRAME, 0);     // next SDL event
			break;
		case 1:
			maple->asicAckA |= 0x20; 
			addInterrupt(FRAME, 1);	// next VBL event
			break;
		case 2:
			tmu->updateTCNT0();
			break;
		case 3:
			tmu->updateTCNT1();
			break;
		default:
			printf("Unknown Interrupt Type: %d\n", ci->second);
		}
		intrpts.erase(ci);
		ci = intrpts.begin();	
	}
	
	if (maple->asic9a & maple->asicAckA) 
        {
		intc->externalInt(9, 0x320);
	}
}

// ***************************** Recompiler *****************************
int SHCpu::build_cl()
{
	Word d;
	
	if (((Dword)recPtr - (Dword)recMem) >= (0x800000 - 0x8000)) 
        {
		printf("Recompile Memory exceeded!\n");
		exit(-1);
		// recReset(); -> clear recR*M and reset recPtr
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
	PUSHA32();
	
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
	POPA32();
	RET();
	branch = 0;
	
	return 0;
}

void SHCpu::go_rec()
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
	
	addInterrupt(0x80000, 0);	// first SDL event
	addInterrupt(0x1000, 1);	// first VBL event
	
	for(;;)
	{
		if(debugger->prompt())
		{
		    
			void (**recFunc)();
			char *p;
		    
			Dword tpc = (PC & 0x1fffffff);
			if ((tpc >= 0x0c000000) && (tpc < 0x0d000000)) {
				p = (char*)(recRAM+((tpc-0x0c000000)<<1));
			} else if ((tpc >= 0x00000000) && (tpc < 0x00200000)) {
				p = (char*)(recROM+((tpc-0x00000000)<<1));
			}
			else { printf("Access to illegal mem: %08x\n", tpc); return; }
			
			recFunc = (void (**)()) (Dword)p;
			if (*recFunc) {
				(*recFunc)();
			} else {
				build_cl();
				(*recFunc)();
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
