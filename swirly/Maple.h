#ifndef _MAPLE_H_
#define _MAPLE_H_

#include "swirly.h"
#include "Controller.h"

// commands
#define MAPLE_REQ_DEVICE_INFO      1
#define MAPLE_REQ_EXT_DEVICE_INFO  2
#define MAPLE_RESET_DEVICE         3
#define MAPLE_SHUTDOWN_DEVICE      4
#define MAPLE_DEVICE_INFO_RESP     5
#define MAPLE_EXT_DEVICE_INFO_RESP 6
#define MAPLE_CMD_ACK_RESP         7
#define MAPLE_DATA_TRANSFER_RESP   8
#define MAPLE_GET_CONDITION        9
#define MAPLE_GET_MEMORY_INFO      10
#define MAPLE_BLOCK_READ           11
#define MAPLE_BLOCK_WRITE          12
#define MAPLE_SET_CONDITION        14

// error codes
#define MAPLE_FILE_ERR           0xfb
#define MAPLE_SEND_CMD_AGAIN_ERR 0xfc
#define MAPLE_UNKNOWN_CMD_ERR    0xfd
#define MAPLE_BAD_FUNC_CODE_ERR  0xfe
#define MAPLE_NORESP_ERR         0xff

// registers (from 0xa06f6cxx)
#define MAPLE_DMA_ADDR    0x04
#define MAPLE_RESET1_ADDR 0x8c
#define MAPLE_RESET2_ADDR 0x10
#define MAPLE_STATE_ADDR  0x18
#define MAPLE_SPEED_ADDR  0x80
#define MAPLE_ENABLE_ADDR 0x14

#define DMA_STATE 0x00
#define DMA_LEN   0x04
#define DMA_DST   0x08 // has to be cleared after dma transfer

#define LMMODE0   0x84
#define LMMODE1   0x88

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
    

	Maple(Dword setting);
	virtual ~Maple();
	
	Dword mapleHook(int eventType, Dword addr, Dword data);
	Dword asicHook(int eventType, Dword addr, Dword data);
	Dword g2Hook(int eventType, Dword addr, Dword data);
	Dword unknownHook(int eventType, Dword addr, Dword data);
	
	void setAsicA(int bit);

	class Controller *controller;
	
	Dword asic9a, asic9b, asic9c;
	Dword asicBa, asicBb, asicBc;
	Dword asicDa, asicDb, asicDc;
	Dword asicAckA, asicAckB, asicAckC;


	Dword g2Fifo;

	Dword dmaDestAddr;
	Dword dmaCount;
	Dword dmaRepeat;
	
 private:
	Maple() {}
	Dword dmaAddr;
	bool enabled;
};

#endif
