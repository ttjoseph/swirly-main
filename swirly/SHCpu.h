// SHCpu.h: interface for the SHCpu class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _SHCPU_H_
#define _SHCPU_H_

#include "swirly.h"
#include "Debugger.h"
#include "SHMmu.h"
#include "SHTmu.h"
#include "SHBsc.h"
#include "SHSci.h"
#include "SHDmac.h"
#include "SHIntc.h"
#include "Gpu.h"
#include "Spu.h"
#include "Gdrom.h"
#include "Maple.h"
#include "Overlord.h"

#include <stdio.h>
#include <math.h>

// utility macros
#define getM(a) (((Word)a >> 4) & 0xf)
#define getN(a) (((Word)a >> 8) & 0xf)
#define getI(a) ((Word)a & 0xff)
// these next two only work on byte values!!
#define BCD(n) ((n % 10) | ((n/10) << 4))
#define PACKED(n) ((n&0xf) + (10*(n>>4)))

// used in FPU instructions
#define FPU_DP_FIX_M() if(FPSCR & F_FPSCR_PR) m>>=1
#define FPU_DP_FIX_N() if(FPSCR & F_FPSCR_PR) n>>=1
#define FPU_DP_FIX_MN() if(FPSCR & F_FPSCR_PR) { m >>= 1; n>>=1; }
#define FPU_DP() (FPSCR & F_FPSCR_PR)
#define FPU_SZ_FIX_M() if(FPSCR & F_FPSCR_SZ) m>>=1
#define FPU_SZ_FIX_N() if(FPSCR & F_FPSCR_SZ) n>>=1
#define FPU_SZ_FIX_MN() if(FPSCR & F_FPSCR_SZ) { m >>= 1; n>>=1; }
#define FPU_SZ() (FPSCR & F_FPSCR_SZ)

// Flags
// these are named like so: F_[regname]_[flagname]
#define F_SR_MD 0x40000000
#define F_SR_RB 0x20000000
#define F_SR_BL 0x10000000
#define F_SR_FD 0x00008000
#define F_SR_M  0x00000200
#define F_SR_Q  0x00000100
#define F_SR_S  0x00000002
#define F_SR_T  0x00000001

#define F_FPSCR_PR 0x00080000
#define F_FPSCR_DN 0x00040000
#define F_FPSCR_SZ 0x00100000
#define F_FPSCR_FR 0x00200000

// makes it easier to access bits in SR
#define M (((struct SR_t *)(&SR))->M0)
#define Q (((struct SR_t *)(&SR))->Q0)
#define S (((struct SR_t *)(&SR))->S0)
#define T (((struct SR_t *)(&SR))->T0)

// exception codes
#define E_USER_BREAK_BEFORE_INSTRUCTION_EXECUTION 0x1e0
#define E_INSTRUCTION_ADDRESS_ERROR             0x0e0
#define E_INSTRUCTION_TLB_MISS                  0x040
#define E_INSTRUCTION_TLB_PROTECTION_VIOLATION  0x0a0
#define E_GENERAL_ILLEGAL_INSTRUCTION           0x180
#define E_SLOT_ILLEGAL_INSTRUCTION              0x1a0
#define E_GENERAL_FPU_DISABLE                   0x800
#define E_SLOT_FPU_DISABLE                      0x820
#define E_DATA_ADDRESS_ERROR_READ               0x0e0
#define E_DATA_ADDRESS_ERROR_WRITE              0x100
#define E_DATA_TLB_MISS_READ                    0x040
#define E_DATA_TLB_MISS_WRITE                   0x060
#define E_DATA_TLB_PROTECTION_VIOLATION_READ    0x0a0
#define E_DATA_TLB_PROTECTION_VIOLATION_WRITE   0x0c0
#define E_FPU									0x120
#define E_INITAL_PAGE_WRITE						0x080

// functions in shfpu.s
extern "C"
{
void shfpu_setContext(float *FR, float *XF, float *FPUL, Dword *FPSCR);
void shcpu_DO_MACL(Sdword rn, Sdword rm, Dword *mach, Dword *macl);
void shfpu_sFLOAT();
void shfpu_sFMUL();
}

typedef union {
	double d;
	unsigned int i[2];
} cnv_dbl;

typedef struct
{
	void (SHCpu::*opcode_handler)(Word);        /* handler function */
	Word mask;                  /* mask on opcode */
	Word match;                 /* what to match after masking */
} opcode_handler_struct;

class SHCpu
{
public:
	void go();
	void go_rec();
	void exception(Dword type, Dword addr, Dword data, char *datadesc = 0);
	void executeInstruction(Word d);
	void reset();
	void delaySlot();
	SHCpu();
	virtual ~SHCpu();
	void setSR(Dword d);

	Dword *R, *RBANK, RBANK0[16], RBANK1[16], SR, GBR, VBR, MACH, MACL, PR, PC,
	SPC, DBR, SSR, SGR, *FR_Dwords, *XF_Dwords;
	// FPU regs
	Dword FPSCR;
	Dword FPUL;
	float FPR_BANK0[16], FPR_BANK1[16], *FR, *XF;
	double *DR, *XD;

	Byte ccnRegs[1024]; // XXX: is this the best way to do this?
	int exceptionsPending;
    Dword numIterations;

	class SHMmu *mmu;
	class SHTmu *tmu;
	class SHBsc *bsc;
	class SHSci *sci;
	class SHDmac *dmac;
	class SHIntc *intc;
	class Debugger *debugger;
	class Gpu *gpu;
	class Gdrom *gdrom;
	class Spu *spu;
	class Maple *maple;

	class Overlord *overlord;

	// This is used to make accessing SR flags easier.
	// XXX: This could be very compiler-dependent.  Is the order of the
	//      bitfields within the Dword defined by C++?
	struct SR_t
	{
		Dword
			T0:1,
			S0:1,
			foo1:2,
			IMASK0:4,
			Q0:1,
			M0:1,
			foo0:22;
	};

	void checkInterrupt();
	void addInterrupt(Dword at, Dword type);

	Dword recPC;
	char* recMem;
	char* recRAM;
	char* recROM;
	char* recPtr;
	int branch;
	
	int build_cl();
	void recNativeOpcode(Word op);
	void recHook(Word op);
	void recBranch(Word op);
	void recNOP(Word op);
	void recMOV(Word op);
		
public:	// Needs to be public
	void dispatchSwirlyHook(Word op);
	void unknownOpcode(Word op);
	// XXX: I don't trust these FP instructions yet.
	void FSCA(Word op);
	void FSRRA(Word op);
	void FTRV(Word op);
	void FTRC(Word op);
	void FSUB(Word op);
	void FSTS(Word op);
	void FSQRT(Word op);
	void FSCHG(Word op);
	void FRCHG(Word op);
	void FNEG(Word op);
	void FMAC(Word op);
	void FLOAT(Word op);
	void FLDS(Word op);
	void FLDI1(Word op);
	void FLDI0(Word op);
	void FDIV(Word op);
	void FCNVSD(Word op);
	void FCNVDS(Word op);
	void FCMPGT(Word op);
	void FCMPEQ(Word op);
	void FMOV_INDEX_STORE(Word op);
	void FMOV_INDEX_LOAD(Word op);
	void FMOV_SAVE(Word op);
	void FMOV_RESTORE(Word op);
	void FMOV_LOAD(Word op);
	void FMOV_STORE(Word op);
	void FMOV(Word op);
	void FMUL(Word op);
	void setFPSCR(Dword d);
	void FADD(Word op);
	void FABS(Word op);
	void LDSFPUL(Word op);
	void LDSMFPSCR(Word op);
	void LDSMFPUL(Word op);

	// the non-FP instructions
	void ADD(Word op);
	void ADDI(Word op);
	void ADDC(Word op);
	void ADDV(Word op);
	void AND(Word op);
	void ANDI(Word op);
	void ANDM(Word op);
	void BF(Word op);
	void BFS(Word op);
	void BRA(Word op);
	void BRAF(Word op);
	void BSR(Word op);
	void BSRF(Word op);
	void BT(Word op);
	void BTS(Word op);
	void CLRMAC(Word op);
	void CLRS(Word op);
	void CLRT(Word op);
	void CMPEQ(Word op);
	void CMPGE(Word op);
	void CMPGT(Word op);
	void CMPHI(Word op);
	void CMPHS(Word op);
	void CMPPL(Word op);
	void CMPPZ(Word op);
	void CMPSTR(Word op);
	void CMPIM(Word op);
	void DIV0S(Word op);
	void DIV0U(Word op);
	void DIV1(Word op);
	void DMULS(Word op);
	void DMULU(Word op);
	void DT(Word op);
	void EXTSB(Word op);
	void EXTSW(Word op);
	void EXTUB(Word op);
	void EXTUW(Word op);
	void JMP(Word op);
	void JSR(Word op);
	void LDCSR(Word op);
	void LDCGBR(Word op);
	void LDCVBR(Word op);
	void LDCSSR(Word op);
	void LDCSPC(Word op);
	void LDCDBR(Word op);
	void LDCRBANK(Word op);
	void LDCMSR(Word op);
	void LDCMGBR(Word op);
	void LDCMVBR(Word op);
	void LDCMSSR(Word op);
	void LDCMSPC(Word op);
	void LDCMDBR(Word op);
	void LDCMRBANK(Word op);
	void LDSMPR(Word op);
	void LDSMMACH(Word op);
	void LDSMMACL(Word op);
	void LDSPR(Word op);
	void LDSFPSCR(Word op);
	void LDSMACH(Word op);
	void LDSMACL(Word op);
	void LDTLB(Word op);
	void DO_MACL(Word op);
	void MACW(Word op);
	void MOV(Word op);
	void MOVBS(Word op);
	void MOVWS(Word op);
	void MOVLS(Word op);
	void MOVBL(Word op);
	void MOVWL(Word op);
	void MOVLL(Word op);
	void MOVBM(Word op);
	void MOVWM(Word op);
	void MOVLM(Word op);
	void MOVBP(Word op);
	void MOVWP(Word op);
	void MOVLP(Word op);
	void MOVBS0(Word op);
	void MOVWS0(Word op);
	void MOVLS0(Word op);
	void MOVBL0(Word op);
	void MOVWL0(Word op);
	void MOVLL0(Word op);
	void MOVI(Word op);
	void MOVWI(Word op);
	void MOVLI(Word op);
	void MOVBLG(Word op);
	void MOVWLG(Word op);
	void MOVLLG(Word op);
	void MOVBSG(Word op);
	void MOVWSG(Word op);
	void MOVLSG(Word op);
	void MOVBS4(Word op);
	void MOVWS4(Word op);
	void MOVBL4(Word op);
	void MOVWL4(Word op);
	void MOVLS4(Word op);
	void MOVLL4(Word op);
	void MOVA(Word op);
	void MOVCAL(Word op);
	void MOVT(Word op);
	void MULL(Word op);
	void MULS(Word op);
	void MULU(Word op);
	void NEG(Word op);
	void NEGC(Word op);
	void NOT(Word op);
	void NOP(Word op);
	void OCBI(Word op);
	void OCBP(Word op);
	void OCBWB(Word op);
	void OR(Word op);
	void ORI(Word op);
	void ORM(Word op);
	void PREF(Word op);
	void ROTCR(Word op);
	void ROTCL(Word op);
	void ROTR(Word op);
	void ROTL(Word op);
	void RTE(Word op);
	void RTS(Word op);
	void SETS(Word op);
	void SETT(Word op);
	void SHAD(Word op);
	void SHAR(Word op);
	void SHAL(Word op);
	void SHLD(Word op);
	void SHLL(Word op);
	void SHLL16(Word op);
	void SHLL8(Word op);
	void SHLL2(Word op);
	void SHLR(Word op);
	void SHLR16(Word op);
	void SHLR8(Word op);
	void SHLR2(Word op);
	void SLEEP(Word op);
	void STCSR(Word op);
	void STCGBR(Word op);
	void STCVBR(Word op);
	void STCSSR(Word op);
	void STCSPC(Word op);
	void STCDBR(Word op);
	void STCSGR(Word op);
	void STCRBANK(Word op);
	void STCMSR(Word op);
	void STCMGBR(Word op);
	void STCMVBR(Word op);
	void STCMSSR(Word op);
	void STCMSPC(Word op);
	void STCMDBR(Word op);
	void STCMSGR(Word op);
	void STCMRBANK(Word op);
	void STSMACH(Word op);
	void STSMACL(Word op);
	void STSPR(Word op);
	void STSFPSCR(Word op);
	void STSFPUL(Word op);
	void STSMMACH(Word op);
	void STSMMACL(Word op);
	void STSMPR(Word op);
	void STSMFPSCR(Word op);
	void STSMFPUL(Word op);
	void SUBC(Word op);
	void SUB(Word op);
	void SUBV(Word op);
	void SWAPW(Word op);
	void SWAPB(Word op);
	void TAS(Word op);
	void TRAPA(Word op);
	void TST(Word op);
	void TSTI(Word op);
	void TSTM(Word op);
	void XORM(Word op);
	void XORI(Word op);
	void XOR(Word op);
	void XTRCT(Word op);
};

#endif
