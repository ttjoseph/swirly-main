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

class SHCpu
{
public:
	void go();
	void exception(Dword type, Dword addr, Dword data, char *datadesc = 0);
	void executeInstruction(Word d);
	void reset();
	void delaySlot();
	SHCpu();
	virtual ~SHCpu();
	void setSR(Dword d);

	Dword *R, *RBANK, RBANK0[16], RBANK1[16], SR, GBR, VBR, MACH, MACL, PR, PC,
	SPC, DBR, SSR, SGR, *FR_Dwords;
	// FPU regs
	Dword FPSCR;
	Dword FPUL;
	float FPR_BANK0[16], FPR_BANK1[16], *FR, *XF;
	double *DR, *XD;

	Byte ccnRegs[1024]; // XXX: is this the best way to do this?
	int exceptionsPending;

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

private:
	void dispatchSwirlyHook();
	void unknownOpcode();
	// XXX: I don't trust these FP instructions yet.
	void FSCA(int n);
	void FSRRA(int n);
	void FTRC(int n);
	void FSUB(int m, int n);
	void FSTS(int n);
	void FSQRT(int n);
	void FSCHG();
	void FRCHG();
	void FNEG(int n);
	void FMOV_DRXD(int m, int n);
	void FMOV_XDDR(int m, int n);
	void FMOV_XDXD(int m, int n);
	void FMOV_INDEX_STORE_XD(int m, int n);
	void FMOV_INDEX_LOAD_XD(int m, int n);
	void FMOV_SAVE_XD(int m, int n);
	void FMOV_RESTORE_XD(int m, int n);
	void FMOV_LOAD_XD(int m, int n);
	void FMOV_STORE_XD(int m, int n);
	void FMAC(int m, int n);
	void FLOAT(int n);
	void FLDS(int n);
	void FLDI1(int n);
	void FLDI0(int n);
	void FDIV(int m, int n);
	void FCNVSD(int n);
	void FCNVDS(int n);
	void FCMPGT(int m, int n);
	void FCMPEQ(int m, int n);
	void FMOV_INDEX_STORE(int m, int n);
	void FMOV_INDEX_LOAD(int m, int n);
	void FMOV_SAVE(int m, int n);
	void FMOV_RESTORE(int m, int n);
	void FMOV_LOAD(int m, int n);
	void FMOV_STORE(int m, int n);
	void FMOV(int m, int n);
	void FMUL(int m, int n);
	void setFPSCR(Dword d);
	void FADD(int m, int n);
	void FABS(int n);
	void LDSFPUL(int n);
  void LDSMFPSCR(int n);
  void LDSMFPUL(int n);

	// the non-FP instructions
	void ADD(int m, int n);
	void ADDI(Byte i, int n);
	void ADDC(int m, int n);
	void ADDV(int m, int n);
	void AND(int m, int n);
	void ANDI(Byte i);
	void ANDM(Byte i);
	void BF(Byte d);
	void BFS(Byte d);
	void BRA(Sword d);
	void BRAF(int n);
	void BSR(Sword d);
	void BSRF(int n);
	void BT(Byte d);
	void BTS(Byte d);
	void CLRMAC();
	void CLRS();
	void CLRT();
	void CMPEQ(int m, int n);
	void CMPGE(int m, int n);
	void CMPGT(int m, int n);
	void CMPHI(int m, int n);
	void CMPHS(int m, int n);
	void CMPPL(int n);
	void CMPPZ(int n);
	void CMPSTR(int m, int n);
	void CMPIM(Byte i);
	void DIV0S(int m, int n);
	void DIV0U();
	void DIV1(int m, int n);
	void DMULS(int m, int n);
	void DMULU(int m, int n);
	void DT(int n);
	void EXTSB(int m, int n);
	void EXTSW(int m, int n);
	void EXTUB(int m, int n);
	void EXTUW(int m, int n);
	void JMP(int n);
	void JSR(int n);
	void LDCSR(int n);
	void LDCGBR(int n);
	void LDCVBR(int n);
	void LDCSSR(int n);
	void LDCSPC(int n);
	void LDCDBR(int n);
	void LDCRBANK(int m, int n);
	void LDCMSR(int n);
	void LDCMGBR(int n);
	void LDCMVBR(int n);
	void LDCMSSR(int n);
	void LDCMSPC(int n);
	void LDCMDBR(int n);
	void LDCMRBANK(int m, int n);
	void LDSMPR(int n);
	void LDSMMACH(int n);
	void LDSMMACL(int n);
	void LDSPR(int n);
	void LDSFPSCR(int n);
	void LDSMACH(int n);
	void LDSMACL(int n);
	void DO_MACL(int m, int n);
	void MACW(int m, int n);
	void MOV(int m, int n);
	void MOVBS(int m, int n);
	void MOVWS(int m, int n);
	void MOVLS(int m, int n);
	void MOVBL(int m, int n);
	void MOVWL(int m, int n);
	void MOVLL(int m, int n);
	void MOVBM(int m, int n);
	void MOVWM(int m, int n);
	void MOVLM(int m, int n);
	void MOVBP(int m, int n);
	void MOVWP(int m, int n);
	void MOVLP(int m, int n);
	void MOVBS0(int m, int n);
	void MOVWS0(int m, int n);
	void MOVLS0(int m, int n);
	void MOVBL0(int m, int n);
	void MOVWL0(int m, int n);
	void MOVLL0(int m, int n);
	void MOVI(Byte i, int n);
	void MOVWI(Byte d, int n);
	void MOVLI(Byte d, int n);
	void MOVBLG(Byte d);
	void MOVWLG(Byte d);
	void MOVLLG(Byte d);
	void MOVBSG(Byte d);
	void MOVWSG(Byte d);
	void MOVLSG(Byte d);
	void MOVBS4(Byte d, int n);
	void MOVWS4(Byte d, int n);
	void MOVBL4(int n, Byte d);
	void MOVWL4(int n, Byte d);
	void MOVLS4(int m, Byte d, int n);
	void MOVLL4(int m, Byte d, int n);
	void MOVA(Byte d);
	void MOVCAL(int n);
	void MOVT(int n);
	void MULL(int m, int n);
	void MULS(int m, int n);
	void MULU(int m, int n);
	void NEG(int m, int n);
	void NEGC(int m, int n);
	void NOT(int m, int n);
	void NOP();
	void OCBI(int n);
	void OCBP(int n);
	void OCBWB(int n);
	void OR(int m, int n);
	void ORI(Byte i);
	void ORM(Byte i);
	void PREF(int n);
	void ROTCR(int n);
	void ROTCL(int n);
	void ROTR(int n);
	void ROTL(int n);
	void RTE();
	void RTS();
	void SETS();
	void SETT();
	void SHAD(int m, int n);
	void SHAR(int n);
	void SHAL(int n);
	void SHLD(int m, int n);
	void SHLL(int n);
	void SHLL16(int n);
	void SHLL8(int n);
	void SHLL2(int n);
	void SHLR(int n);
	void SHLR16(int n);
	void SHLR8(int n);
	void SHLR2(int n);
	void SLEEP();
	void STCSR(int n);
	void STCGBR(int n);
	void STCVBR(int n);
	void STCSSR(int n);
	void STCSPC(int n);
	void STCDBR(int n);
	void STCSGR(int n);
	void STCRBANK(int m, int n);
	void STCMSR(int n);
	void STCMGBR(int n);
	void STCMVBR(int n);
	void STCMSSR(int n);
	void STCMSPC(int n);
	void STCMDBR(int n);
	void STCMSGR(int n);
	void STCMRBANK(int m, int n);
	void STSMACH(int n);
	void STSMACL(int n);
	void STSPR(int n);
	void STSFPSCR(int n);
	void STSFPUL(int n);
	void STSMMACH(int n);
	void STSMMACL(int n);
	void STSMPR(int n);
	void STSMFPSCR(int n);
	void STSMFPUL(int n);
	void SUBC(int m, int n);
	void SUB(int m, int n);
	void SUBV(int m, int n);
	void SWAPW(int m, int n);
	void SWAPB(int m, int n);
	void TAS(int n);
	void TRAPA(Byte i);
	void TST(int m, int n);
	void TSTI(Byte i);
	void TSTM(Byte i);
	void XORM(Byte i);
	void XORI(Byte i);
	void XOR(int m, int n);
	void XTRCT(int m, int n);
};

#endif
