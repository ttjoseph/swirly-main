// Debugger.h: interface for the Debugger class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#include "swirly.h"

#define DBG_MAXBREAKPOINTS 32
#define DBG_BP_EXECUTION 101
#define DBG_BP_MEMACCESS 102

class Debugger  
{
public:
	void checkExecBp();
	void dumpToFile(char *addr, int length, char *fname);
	void checkMemBp(Dword addr);
	bool prompt();
	void dumpRegs();
	void flamingDeath(char *fmt, ...);
	Debugger(class SHCpu *cpu);
	virtual ~Debugger();
	bool dispatchCommand(char *cmd);
  char* getExceptionName(int exception);
  char* disasmInstr(Word d, Dword pc);

	bool promptOn;

private:
	Debugger() {}

	void updateMaxBp(int type, int *max);
	bool memBpSet;
	bool execBpSet;
	int maxExecBp; // number of the exec breakpoint with the highest number
	int maxMemBp;  // same as above, except with mem breakpoints
	struct Breakpoint
	{
		Dword addr;
		int type;

		bool valid;
	};

	class SHCpu *cpu;
	Breakpoint *breakpoints;

	bool isWhitespace(char c);
	int scanTillWhitespace(char *s);
	void getToken(char *s, int tokennum, char *putwhere);
	int scanTillBlackspace(char *s);

	bool cmdU(char *cmd);
	bool cmdR(char *cmd);
	bool cmdBm(char *cmd);
	bool cmdDr(char *cmd);
	bool cmdDf(char *cmd);
	bool cmdFrd(char *cmd);
	bool cmdFr(char *cmd);
	bool cmdBc(char *cmd);
	bool cmdBl(char *cmd);
	bool cmdG(char *cmd);
	bool cmdQ(char *cmd);
	bool cmdBx(char *cmd);
	bool cmdD(char *cmd);
  bool cmdUf(char *cmd);


};

#endif
