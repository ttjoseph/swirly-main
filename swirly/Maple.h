#ifndef _MAPLE_H_
#define _MAPLE_H_
// Maple bus header file

#include "swirly.h"
#include "SHCpu.h"

// function codes
#define MAPLE_CONTROLLER	0x001
#define MAPLE_MEMCARD		0x002
#define MAPLE_LCD			0x004
#define MAPLE_CLOCK			0x008
#define MAPLE_MICROPHONE	0x010
#define MAPLE_ARGUN			0x020 // I wonder what an AR gun is
#define MAPLE_KEYBOARD		0x040
#define MAPLE_LIGHTGUN		0x080
#define MAPLE_PURUPURU		0x100
#define MAPLE_MOUSE			0x200

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

#define BUTTON_BASE 0
#define BUTTON_LEFT (BUTTON_BASE+1)
#define BUTTON_RIGHT (BUTTON_BASE+2)
#define BUTTON_UP (BUTTON_BASE+3)
#define BUTTON_DOWN (BUTTON_BASE+4)
#define BUTTON_START (BUTTON_BASE+5)
#define BUTTON_A (BUTTON_BASE+6)

class Maple
{
public:
	// This struct copied almost verbatim from http://mc.pp.se/dc/maplebus.html
	// Thank you Marcus Comstedt!
	struct DeviceInfo
	{
		Dword suppFuncs; // func codes supported - big endian
		Dword funcData[3]; // info about func codes - big endian
		Byte region; // peripheral's region code
		Byte connectorDirection; // ?? that's odd
		char productName[30];
		char productLicense[60];
		Word standbyPower; // standby power consumption - little endian
		Word maxPower; // maximum power consumption - little endian
	};

	struct FrameHeader
	{
		Byte command;
		Byte recipient;
		Byte sender;
		Byte numWordsFollowing;
	};

	Maple(class SHCpu *cpu);
	virtual ~Maple();

	Dword hook(int eventType, Dword addr, Dword data);

	class SHCpu *cpu;
	Byte buttonState[256];

	Dword asic9a;
	Dword asicAckA;
	Dword g2Fifo;
	
private:
	Maple() {}
	Dword dmaAddr;
	DeviceInfo gamepad;
};

#endif
