#include "Overlord.h"
#include "Debugger.h"
#include "SHMmu.h"
#include "Gpu.h"
#include "SHCpu.h"
#include "SHIntc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "dcdis.h"

Debugger::Debugger() : showStatusMessages(true), branchTraceFile(NULL)
{
	// create the breakpoint list
	breakpoints = new Breakpoint[DBG_MAXBREAKPOINTS];
	for(int i=0;i<DBG_MAXBREAKPOINTS;i++)
		breakpoints[i].valid = false;
	promptOn = false;
	execBpSet = false;
	memBpSet = false;
	maxExecBp = maxMemBp = 0;
	showFP = false;
}

Debugger::~Debugger()
{
	if(branchTraceFile)
		fclose(branchTraceFile);
	delete [] breakpoints;
}

// loads and runs a script of debugger commands
// returns false on error
bool Debugger::runScript(char *fname)
{

    FILE *fp = fopen(fname, "r");
    if(fp == NULL)
	return false;
    
    printf("Executing %s...\n", fname);
    
    char tmp[1024], tmp2[1024];
    while(!feof(fp))
    {
	if(fgets(tmp, 1024, fp) == NULL)
	    break;

	// % is a comment character - everything to the right of it is ignored
	for(Dword i=0; i<strlen(tmp); i++)
        {
	    if(tmp[i] == '%')
	    {
		tmp[i] = 0;
		break;
	    }
	}
	// throw out blank lines
	getToken(tmp, 1, tmp2);
	if(strlen(tmp2) > 0)
	    dispatchCommand(tmp);
    }
   
    fclose(fp);

    printf("\n");

    return true;
}

void Debugger::print(char *fmt, ...)
{
	if(showStatusMessages)
	{
		va_list mrk;
		va_start(mrk, fmt);
		vprintf(fmt, mrk);
		va_end(mrk);
	}
}

// Reports a branch in execution so it can be logged to the
// branch trace if needed.
void Debugger::reportBranch(char *tag, Dword src, Dword dest)
{
	static Dword last_src = 0, last_dest = 0;
	static int count = 0;

	if(branchTraceFile) 
        {
		if((last_src == src) && (last_dest == dest))
			count++;
		else 
                {
			if(count > 0)
				fprintf(branchTraceFile, "\trepeated %d times\n", count);
			else
				fprintf(branchTraceFile, "%08x: %s %08x\n", src, tag, dest);
			count = 0;
		}
		last_src = src;
		last_dest = dest;
	}
}

// dumps registers
void Debugger::dumpRegs()
{
	int i;
	for(i=0;i<16;i+=2)
	{
		printf("R%-2d : %08X  R%-2d : %08X  ", i, cpu->R[i], i+1, cpu->R[i+1]);

		if(i==0) // print cpu->SR
		{
			printf("SR:    [");
			if(cpu->SR&F_SR_MD) printf("MD "); else printf("   ");
			if(cpu->SR&F_SR_RB) printf("RB "); else printf("   ");
			if(cpu->SR&F_SR_BL) printf("BL "); else printf("   ");
			if(cpu->SR&F_SR_FD) printf("FD "); else printf("   ");
			if(cpu->SR&F_SR_M) printf("M "); else printf("  ");
			if(cpu->SR&F_SR_Q) printf("Q "); else printf("  ");
			if(cpu->SR&F_SR_S) printf("S "); else printf("  ");
			if(cpu->SR&F_SR_T) printf("T"); else printf(" ");
			printf("]");
		}
		if(i==2)
		{
			int fpscr = cpu->FPSCR;
			printf("FPSCR: [%s %s %s %s %d]",
				fpscr & F_FPSCR_FR ? "FR" : "  ",
				fpscr & F_FPSCR_SZ ? "SZ" : "  ",
				fpscr & F_FPSCR_PR ? "PR" : "  ",
				fpscr & F_FPSCR_DN ? "DN" : "  ",
				bits(fpscr, 1, 0));
		}
		if(i==4)
		{
			//printf("MMUCR: %08X", CCNREG(MMUCR));
			int mmucr = CCNREG(MMUCR);
			printf("MMUCR: [LRUI=%02X URB=%02X URC=%02X %s %s %s %s]",
				bits(mmucr, 31, 26),
				bits(mmucr, 23, 18),
				bits(mmucr, 15, 10),
				bits(mmucr, 9, 9) ? "SQMD" : "    ",
				bits(mmucr, 8, 8) ? "SV" : "  ",
				bits(mmucr, 2, 2) ? "TI" : "  ",
				bits(mmucr, 0, 0) ? "AT" : "  ");
		}
		if(i==6)
		{
			printf("PTEH: %08X  PTEL: %08X PTEA: %02X",
				*((Dword*)(cpu->ccnRegs+PTEH)),
				*((Dword*)(cpu->ccnRegs+PTEL)),
				*((Dword*)(cpu->ccnRegs+PTEA)) );
		}
		if(i==8)
			printf("FPUL: %08X  (%.4ff)", cpu->FPUL, *((float*)&cpu->FPUL));
		if(i==10)
			printf("MACH: %08X  MACL: %08X", cpu->MACH, cpu->MACL);
		if(i==12)
			printf("GBR : %08X  VBR : %08X", cpu->GBR, cpu->VBR);
		if(i==14)
			printf("SPC : %08X  PR  : %08X", cpu->SPC, cpu->PR);
		printf("\n");
	}

	if(!showFP) return;

	for(i=0;i<16;i+=2)
	{
		printf("FR%-2d: %08X  ", i,  *(Dword*)&cpu->FR[i]);
		printf("FR%-2d: %08X  ", i+1, *(Dword*)&cpu->FR[i+1]);
		printf("XF%-2d: %08X  ", i,  *(Dword*)&cpu->XF[i]);
		printf("XF%-2d: %08X  ", i+1, *(Dword*)&cpu->XF[i+1]);

		printf("\n");
	}
}

void Debugger::flamingDeath(char *fmt, ...)
{
    printf("************ FLAMING DEATH ************\n");
    dumpRegs();
    // if an exception is pending we don't want to cause another one
    if(intc->exceptionsPending <= 1)
    {
	Word d = mmu->fetchInstruction(cpu->PC);
	printf("%08X: %04x: %s", cpu->PC, d, disasmInstr(d, cpu->PC));
    }
    else
	printf("PC: %08X", cpu->PC);
    
    printf("\n");
    va_list mrk;
    va_start(mrk, fmt);
    vprintf(fmt, mrk);
    va_end(mrk);
    printf("\n");
    exit(1);
}

// decides whether we've hit an execution breakpoint, and if so, turns on the
// debugging prompt
inline void Debugger::checkExecBp()
{
	if(execBpSet == false)
		return;

	for(int i=0;i<=maxExecBp;i++)
	{
		if(breakpoints[i].valid && breakpoints[i].type == DBG_BP_EXECUTION &&
			(cpu->PC == breakpoints[i].addr))
		{
			printf("Breakpoint %d (execution at %08x) hit.\n", i, breakpoints[i].addr);
			promptOn = true;
		}
	}
}

char *textureFormat[] = { "ARGB1555",
			  "RGB565",
			  "ARGB4444",
			  "YUV422",
			  "BUMP",
			  "4BPP PALETTE",
			  "8BPP PALETTE" };

void Debugger::printTexInfo(int num)
{   
    printf("texture #%d data: %s, %dx%d, address %08x\n", 
	   num, textureFormat[gpu->textures[num].format], 
	   gpu->textures[num].width, gpu->textures[num].height, 
	   gpu->textures[num].address);
    
    printf("texture #%d options: %s%s%s%s\n", num, 
	   gpu->textures[num].options & TEX_TWIDDLED ? "twiddled" : "",
	   gpu->textures[num].options & TEX_VQ ? "VQ-compressed " : "",
	   gpu->textures[num].options & TEX_STRIDE ? "stride " : "",
	   gpu->textures[num].options & TEX_MIPMAP ? "mipmap" : "");
    
    gpu->textures[num].options |= TEX_LOADED;
}

bool Debugger::cmdTex(char *cmd)
{
    printf("\nTextures loaded:\n");

    for(int i=0; i<gpu->currTexNum; i++)
    {
	printTexInfo(i);
    }
    printf("\n");

    return false;
}

// Does the debugger prompt stuff.
bool Debugger::prompt()
{
	char userInput[512];

	if(execBpSet != false)
	checkExecBp();

	if(promptOn)
	{
		dumpRegs();
		printf("%08x: %04x: %s -> ", cpu->PC, cpu->currInstruction, 
		       disasmInstr(cpu->currInstruction, cpu->PC));
		fgets(userInput, 511, stdin);
		return dispatchCommand(userInput);
	} else
		return true;
}

// Decides which command should be executed, based on the user's input.
bool Debugger::dispatchCommand(char *cmd)
{
	char firstToken[256];

	getToken(cmd, 0, firstToken);

	if(strlen(firstToken) == 0)
		return 1;

#define DO_CMD(a, b) if(strcmp(a, firstToken)==0) return b(cmd)

	DO_CMD("path", cmdPath);
	DO_CMD("d", cmdD);
	DO_CMD("s", cmdS);
	DO_CMD("l", cmdL);
	DO_CMD("bx", cmdBx);
	DO_CMD("bl", cmdBl);
	DO_CMD("g", cmdG);
	DO_CMD("q", cmdQ);
	DO_CMD("bc", cmdBc);
	DO_CMD("fr", cmdFr);
	DO_CMD("frd", cmdFrd);
	DO_CMD("dr", cmdDr);
	DO_CMD("bm", cmdBm);
	DO_CMD("r", cmdR);
	DO_CMD("u", cmdU);
	DO_CMD("df", cmdDf);
	DO_CMD("uf", cmdUf);
	DO_CMD("trb", cmdTrb);
	DO_CMD("stat", cmdStat);
	DO_CMD("f", cmdF);
	DO_CMD("fp", cmdFP);
	DO_CMD("i", cmdI);
	DO_CMD("tex", cmdTex);

#undef DO_CMD

	printf("%s: What does that mean?\n", firstToken);
	return 0;
}

// Some crappy string parsing stuff - as you can see, string parsing is one of my
// least favorite things to write...
void Debugger::getToken(char *s, int tokennum, char *putwhere)
{
	int i, k=0;

	if(strlen(s) == 0)
	{
		*putwhere = '\0';
		return;
	}

	for(i=0;i<tokennum+1;i++)
	{
		s+=k;
		s+=scanTillBlackspace(s);
		k=scanTillWhitespace(s);
	}

	for(i=0;i<k;i++)
		putwhere[i]=s[i];

	putwhere[i]='\0';

}

int Debugger::scanTillWhitespace(char *s)
{
	int i=0;
	while(!isWhitespace(s[i]))
	{
		if(s[i] == '\0')
			return 0;
		i++;
	}
	return i;
}

int Debugger::scanTillBlackspace(char *s)
{
	int i=0;
	while(isWhitespace(s[i]))
	{
		if(s[i] == '\0')
			return 0;
		i++;
	}
	return i;
}

bool Debugger::isWhitespace(char c)
{
	switch(c)
	{
	case ' ':
	case '\t':
	case '\n':
		return true;
		break;
	default:
		return false;
	}
}

bool Debugger::cmdPath(char *cmd) 
{
	getToken(cmd, 1, path);
	printf("Path environment has been set to %s\n", path);
	return false;
}

bool Debugger::cmdFP(char *cmd)
{
    showFP = !showFP;
    return false;
}

// debugger command to dump memory
bool Debugger::cmdD(char *cmd)
{
	char addx[128];
	int addr, i, j, rowlen=16, numrows = 10;
	char c;

	getToken(cmd, 1, addx);
	if(strlen(addx) == 0)
		addr = 0;
	else
		sscanf(addx, "%x", &addr);

	for(i=addr;i<(addr+rowlen*numrows);i+=rowlen)
	{
		printf("%08X: ", i);
		for(j=i;j<i+rowlen;j+=2)
		{
			printf("%02X%02X ", mmu->readByte(j), mmu->readByte(j+1));
		}
		printf(" ");

		for(j=i;j<i+rowlen;j++)
		{
			c = mmu->readByte(j);
			printf("%c", c<32 ? '.' : c);
		}
		printf("\n");
	}

	return false;
}

bool Debugger::cmdI(char *cmd)
{
    intc->printInterrupts();

    return false;
}

// toggle reporting of status messages on and off
bool Debugger::cmdStat(char *cmd)
{
	showStatusMessages = !showStatusMessages;
	// yeah this is kinda pointless - so sue me
	printf("Status messages turned o");
	if(showStatusMessages)
		printf("n.\n");
	else
		printf("ff.\n");
	return false;
}

// toggle branch trace logging on and off
// defaults to ./branch_trace.log
bool Debugger::cmdTrb(char *cmd)
{
	if(branchTraceFile)
	{
		fprintf(branchTraceFile,
			"--> PC=%08X: Branch trace logging turned off <--\n", cpu->PC);
		fclose(branchTraceFile);
		branchTraceFile = NULL;
		printf("Branch trace logging is off.\n");
	} else
	{
		char fname[255];
		getToken(cmd, 1, fname);
		if(strlen(fname) == 0)
			strcpy(fname, "branch_trace.log");
		branchTraceFile = fopen(fname, "a");
		if(branchTraceFile == NULL)
		{
			printf("Error opening %s to use as the branch trace log.\n", fname);
			return false;
		}
		fprintf(branchTraceFile,
			"--> PC=%08X: Branch trace logging turned on <--\n", cpu->PC);
		printf("Branch trace logging is on.\n");
	}
	return false;
}

// debugger command to run a script
bool Debugger::cmdS(char *cmd)
{
	char fname[255];
	getToken(cmd, 1, fname);
	if(strlen(fname) == 0)
	{
		printf("Usage: s <filename>\n");
		return false;
	}
	if(runScript(fname) == false) 
        {
		printf("Couldn't execute script %s.\n", fname);
		return false;
	}
	return true;
}

// debugger command to load file to an arbitrary address
bool Debugger::cmdL(char *cmd) 
{

	char fname[255], addr[255];
	Dword theaddr;
	getToken(cmd, 1, fname);
	getToken(cmd, 2, addr);
	
	if(strlen(fname) == 0) 
        {
		printf("Usage: l <filename> [addr-in-hex]\n");
		return false;
	}
	
	char fullPath[255];
	memset(fullPath, 0, 255);
	strcpy(fullPath, path);
	strcat(fullPath, fname);
	
	char *ext = strrchr(fname, '.');
	if(ext==NULL) ext = ".bin";
	
	if(strcmp(ext, ".srec")==0) 
        {
		printf("Loading %s as SREC\n", fname);
		overlord->loadSrec(fullPath);
	} 
	else if(strcmp(ext, ".elf")==0) 
        {
		printf("Loading %s as ELF\n", fname);
		overlord->loadElf(fullPath);
	} 
	else 
        {
		sscanf(addr, "%x", &theaddr);
		if(strlen(addr) == 0) theaddr = 0x8c010000; // default
		
		printf("Loading %s as flat binary to %08x\n", fname, theaddr);
		overlord->load(fullPath, theaddr);
	}
	
	return true;
}

// debugger command to set an execution breakpoint
bool Debugger::cmdBx(char *cmd)
{
	Dword addr;
	int i;
	char tmp[255];

	getToken(cmd, 1, tmp);
	if(strlen(tmp) == 0)
	{
		printf("Usage: bx <address>\n");
		return false;
	}
	sscanf(tmp, "%x", &addr);
	for(i=0;i<DBG_MAXBREAKPOINTS;i++)
	{
		if(breakpoints[i].valid == 0)
			break;
	}
	
	breakpoints[i].valid = 1;
	breakpoints[i].type = DBG_BP_EXECUTION;
	breakpoints[i].addr = addr;
	execBpSet = true;

	updateMaxBp(DBG_BP_EXECUTION, &maxExecBp);
	printf("Breakpoint %d at PC=%08X has been set.\n", i, addr);
	return false;
}

// debugger command to exit the program
bool Debugger::cmdQ(char *cmd) 
{
	exit(0);
	return false;
}

// debugger command to let the program run uninterrupted (at least till it hits a breakpoint)
bool Debugger::cmdG(char *cmd)
{
	promptOn = false;
	return true;
}

// Draw Frame - tells Gpu to refresh the screen
bool Debugger::cmdDf(char *cmd)
{
	gpu->draw2DFrame();
	printf("2D screen refreshed with Gpu::draw2DFrame.\n");
	return false;
}

// lists breakpoints
bool Debugger::cmdBl(char *cmd)
{
	int i, count=0;;
	printf("Breakpoints:\n");
	for(i=0;i<DBG_MAXBREAKPOINTS;i++)
	{
		if(breakpoints[i].valid)
		{
			switch(breakpoints[i].type)
			{
			case DBG_BP_EXECUTION:
				printf("%02d: PC=%08X\n", i, breakpoints[i].addr);
				break;
			case DBG_BP_MEMACCESS:
				printf("%02d: Mem access at %08X\n", i, breakpoints[i].addr);
				break;
			}
			count++;
		}
	}
	if(count == 0)
		printf("No breakpoints are set.\n");
	return false;
}

// clears a breakpoint
bool Debugger::cmdBc(char *cmd)
{
	char tmp[255];
	int bpnum;

	getToken(cmd, 1, tmp);
	if(strlen(tmp) == 0)
	{
		printf("Usage: bc <address>\n");
		return false;
	}
	sscanf(tmp, "%d", &bpnum);

	if(bpnum>(DBG_MAXBREAKPOINTS-1))
	{
		printf("The breakpoint number you gave is out of range.\n");
		return false;
	}
	breakpoints[bpnum].valid = false;
	printf("Breakpoint %d cleared.\n", bpnum);
	return false;

}

// Dump floating-point regs
bool Debugger::cmdFr(char *cmd)
{
	for(int i=0; i<16;i+=2)
		printf("FR%02d: %.9f %08X  FR%02d: %.9f %08X\n",
			i,
			cpu->FR[i],
			*((Dword*)(cpu->FR+i)),
			i+1,
			cpu->FR[i+1],
			*((Dword*)(cpu->FR+i+1)) );
	return false;
}

// dump double-prec floating point regs explicitly
bool Debugger::cmdFrd(char *cmd)
{
	for(int i=0; i<8;i+=2)
		printf("DR%02d: %f  DR%02d: %f\n", i, cpu->DR[i], i+1, cpu->DR[i+1]);
	return false;
}

// dump regular regs
bool Debugger::cmdDr(char *cmd)
{
	dumpRegs();
	return false;
}

// set breakpoint on memory access
bool Debugger::cmdBm(char *cmd)
{
	int i;
	char tmp[255];

	getToken(cmd, 1, tmp);

	for(i=0;i<DBG_MAXBREAKPOINTS;i++)
	{
		if(breakpoints[i].valid == 0)
			break;
	}
	breakpoints[i].type = DBG_BP_MEMACCESS;
	sscanf(tmp, "%x", &(breakpoints[i].addr));
	breakpoints[i].valid = true;
	memBpSet = true;

	updateMaxBp(DBG_BP_MEMACCESS, &maxMemBp);

	printf("Breakpoint %d on mem access at %08X set.\n", i, breakpoints[i].addr);
	return false;
}

// called for every memory access to see if we've hit a mem access breakpoint
void Debugger::checkMemBp(Dword addr)
{
	// if no memory breakpoints have been set, then don't bother
	// checking
	if(memBpSet == false)
		return;

	for(int i=0;i<(maxMemBp+1);i++)
	{
		if(breakpoints[i].valid && breakpoints[i].type == DBG_BP_MEMACCESS &&
			(addr == breakpoints[i].addr))
		{
			printf("Breakpoint %d (mem access at %08X) hit at PC=%08X.\n", i, addr, cpu->PC);
			promptOn = true;
		}
	}
}

// lets user modify the contents of a register
bool Debugger::cmdR(char *cmd)
{
	char reg[64], value[64];

	Dword val, *realaddr = 0;

	getToken(cmd, 1, reg);
	getToken(cmd, 2, value);

	if(reg[0]=='\0' || value[0]=='\0')
	    return false;

	printf("got %s %s\n", reg, value);

	sscanf(value, "%x", &val);

	printf("value is now %x\n", val);

#define DO_REG(a, b) if(strcmp(reg, a) == 0) realaddr = &(b)

	DO_REG("r0", cpu->R[0]);
	DO_REG("r1", cpu->R[1]);
	DO_REG("r2", cpu->R[2]);
	DO_REG("r3", cpu->R[3]);
	DO_REG("r4", cpu->R[4]);
	DO_REG("r5", cpu->R[5]);
	DO_REG("r6", cpu->R[6]);
	DO_REG("r7", cpu->R[7]);
	DO_REG("r8", cpu->R[8]);
	DO_REG("r9", cpu->R[9]);
	DO_REG("r10", cpu->R[10]);
	DO_REG("r11", cpu->R[11]);
	DO_REG("r12", cpu->R[12]);
	DO_REG("r13", cpu->R[13]);
	DO_REG("r14", cpu->R[14]);
	DO_REG("r15", cpu->R[15]);
	DO_REG("pc", cpu->PC);
	DO_REG("spc", cpu->SPC);
	DO_REG("pr", cpu->PR);
	DO_REG("sr", cpu->SR);
	DO_REG("gbr", cpu->GBR);
	DO_REG("vbr", cpu->VBR);

#undef DO_REG

	*realaddr = val;

	return false;
}

// shows the instruction right after PC - "unassemble following"
// usually you'll use this to show the delay slot instruction
bool Debugger::cmdUf(char *cmd)
{
    Word d = mmu->fetchInstruction(cpu->PC + 2);
    printf("%08x: %04x: %s\n", cpu->PC, d, disasmInstr(d, cpu->PC));
    return false;
}

// executes until an rte or rts
bool Debugger::cmdF(char *cmd)
{

    // disabled for now
#if 0
	Word d;

	promptOn = false;

	// XXX: this doesn't look right. what if an exception occurs? 
	while(promptOn == false)
	{
		d = mmu->fetchInstruction(cpu->PC);
		if((d == 0x002b) || (d == 0x000b))
			promptOn = true;
		else 
			cpu->executeInstruction(d);
	}
#endif
	return false;
}

// disassembles memory - XXX: doesn't work
bool Debugger::cmdU(char *cmd)
{
	char buf[64];
	Dword addr, numbytes, foo;

	getToken(cmd, 1, buf);
	sscanf(buf, "%x", &addr);

	getToken(cmd, 2, buf);
	numbytes = 128;
	foo = sscanf(buf, "%x", &numbytes);

	printf("This command doesn't work yet.\n");

	return false;
}

// dumps an arbitrary memory location (in host address space, not SH's)
// to a file.
void Debugger::dumpToFile(char *addr, int length, char *fname)
{
	static const int blockLen = 4096;
	int numBlocks = length/blockLen;
	int overflow = length%blockLen;

	FILE *fp = fopen(fname, "wb");

	int bytesWritten = fwrite((void*)addr, blockLen, numBlocks, fp);
	fwrite((void*)((Dword)addr+bytesWritten), overflow, 1, fp);
}

// updates the variable that holds the maximum valid breakpoint index
// this is used for optimization of breakpoint checking (so that we don't check
// the entire breakpoint list for validity every time)
void Debugger::updateMaxBp(int type, int *max)
{
	for(int i=0; i<DBG_MAXBREAKPOINTS; i++)
	{
		if((breakpoints[i].valid == true) && (breakpoints[i].type == type))
			*max = i;
	}
	promptOn=true;
}

char* Debugger::disasmInstr(Word d, Dword pc)
{
	return decode(d, pc, "There's a bug in Debugger.cpp", 1, pc);
}
