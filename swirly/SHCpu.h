#ifndef _SHCPU_H_
#define _SHCPU_H_

#include "swirly.h"

// utility macros
#define getM(a) (((Word)a >> 4) & 0xf)
#define getN(a) (((Word)a >> 8) & 0xf)
#define getI(a) ((Word)a & 0xff)

// these next two only work on byte values!!
#define BCD(n) ((n % 10) | ((n/10) << 4))
#define PACKED(n) ((n&0xf) + (10*(n>>4)))

// flags: F_[regname]_[flagname]
#define F_SR_MD 0x40000000
#define F_SR_RB 0x20000000
#define F_SR_BL 0x10000000
#define F_SR_FD 0x00008000
#define F_SR_M  0x00000200
#define F_SR_Q  0x00000100
#define F_SR_S  0x00000002
#define F_SR_T  0x00000001

// makes it easier to access bits in SR
#define M (((struct SR_t *)(&SR))->M0)
#define Q (((struct SR_t *)(&SR))->Q0)
#define S (((struct SR_t *)(&SR))->S0)
#define T (((struct SR_t *)(&SR))->T0)

#define FPU_DP_FIX_M() if(FPSCR & F_FPSCR_PR) m>>=1
#define FPU_DP_FIX_N() if(FPSCR & F_FPSCR_PR) n>>=1
#define FPU_DP_FIX_MN() if(FPSCR & F_FPSCR_PR) { m >>= 1; n>>=1; }
#define FPU_DP() (FPSCR & F_FPSCR_PR)
#define FPU_SZ_FIX_M() if(FPSCR & F_FPSCR_SZ) m>>=1
#define FPU_SZ_FIX_N() if(FPSCR & F_FPSCR_SZ) n>>=1
#define FPU_SZ_FIX_MN() if(FPSCR & F_FPSCR_SZ) { m >>= 1; n>>=1; }
#define FPU_SZ() (FPSCR & F_FPSCR_SZ)

#define F_FPSCR_PR 0x00080000
#define F_FPSCR_DN 0x00040000
#define F_FPSCR_SZ 0x00100000
#define F_FPSCR_FR 0x00200000

#define F_FPSCR_E  0x00020000
#define F_FPSCR_V  0x00010040
#define F_FPSCR_Z  0x00008020
#define F_FPSCR_O  0x00004010
#define F_FPSCR_U  0x00002008
#define F_FPSCR_I  0x00001004

#define F_CAUSE    0x0003f000

#define F_ENABLE_V 0x00000800

#define CLEAR_CAUSE() FPSCR &= ~F_CAUSE

typedef union {
	double d;
	unsigned int i[2];
} cnv_dbl;

typedef unsigned long long Qword;

// functions in shfpu.s
extern "C"
{
void shfpu_setContext(float *FR, float *XF, float *FPUL, Dword *FPSCR);
void shcpu_DO_MACL(Sdword rn, Sdword rm, Dword *mach, Dword *macl);
void shfpu_sFLOAT();
void shfpu_sFMUL();
}

class SHCpu
{
public:
        void go();
	void go_rec();
	void reset();
	void delaySlot();
	SHCpu();
	virtual ~SHCpu();
	void setSR(Dword d);
	void executeInstruction(Word d);

        Word currInstruction;

	Dword *R, *RBANK, RBANK0[16], RBANK1[16], SR, GBR, VBR, MACH, MACL, PR, PC,
	SPC, DBR, SSR, SGR, *FR_Dwords, *XF_Dwords;
	// FPU regs
	Dword FPSCR;
	Dword FPUL;
	float FPR_BANK0[16], FPR_BANK1[16], *FR, *XF;
	double *DR, *XD;

	Byte ccnRegs[1024]; // XXX: is this the best way to do this?
	Dword numIterations;

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

    
	void (SHCpu::*sh_instruction_jump_table[0x10000])(Word);
	void (SHCpu::*sh_recompile_jump_table[0x10000])();

#if 0
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
#endif

public:	// Needs to be public
	void dispatchSwirlyHook(Word op);
	void unknownOpcode(Word op);

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

	// fp instructions
	int test_pinf(int n);
	int test_ninf(int n);
	int test_snan(int n);
	int test_qnan(int n);
	int test_denorm(int n);
	void invalid(int n);
	void qnan(int n);

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
};

#endif
