#ifndef _MAPLE_H_
#define _MAPLE_H_

#include "swirly.h"
#include "SHCpu.h"
#include "Controller.h"

// commands
#define MAPLE_REQ_DEVICE_INFO 1
#define MAPLE_REQ_EXT_DEVICE_INFO 2
#define MAPLE_RESET_DEVICE 3
#define MAPLE_SHUTDOWN_DEVICE 4
#define MAPLE_DEVICE_INFO_RESP 5
#define MAPLE_EXT_DEVICE_INFO_RESP 6
#define MAPLE_CMD_ACK_RESP 7
#define MAPLE_DATA_TRANSFER_RESP 8
#define MAPLE_GET_CONDITION 9
#define MAPLE_GET_MEMORY_INFO 10
#define MAPLE_BLOCK_READ 11
#define MAPLE_BLOCK_WRITE 12
#define MAPLE_SET_CONDITION 14
#define MAPLE_NORESP_ERR 0xff
#define MAPLE_BAD_FUNC_CODE_ERR 0xfe
#define MAPLE_UNKNOWN_CMD_ERR 0xfd
#define MAPLE_SEND_CMD_AGAIN_ERR 0xfc
#define MAPLE_FILE_ERR 0xfb

class Maple 
{

 public:
	
	struct FrameHeader 
	{
		Byte command;
		Byte recipient;
		Byte sender;
		Byte numWordsFollowing;
	};
    
	Maple(class SHCpu *cpu, Dword setting);
	virtual ~Maple();
	
	Dword hook(int eventType, Dword addr, Dword data);
	
	class SHCpu *cpu;
	class Controller *controller;
	
	Dword asic9a;
	Dword asicAckA;
	Dword g2Fifo;
	
 private:
	Maple() {}
	Dword dmaAddr;
};

#endif
