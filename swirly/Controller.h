#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "swirly.h"
#include "Maple.h"

// peripherals
#define MAPLE_CONTROLLER	0x001
#define MAPLE_MEMCARD		0x002
#define MAPLE_LCD		0x004
#define MAPLE_CLOCK		0x008
#define MAPLE_MICROPHONE	0x010
#define MAPLE_ARGUN		0x020
#define MAPLE_KEYBOARD		0x040
#define MAPLE_LIGHTGUN		0x080
#define MAPLE_PURUPURU		0x100
#define MAPLE_MOUSE		0x200

// controller
#define CONT_C            0x0001 
#define CONT_B            0x0002
#define CONT_A            0x0004
#define CONT_START        0x0008
#define CONT_UP           0x0010
#define CONT_DOWN         0x0020
#define CONT_LEFT         0x0040
#define CONT_RIGHT        0x0080
#define CONT_Z            0x0100
#define CONT_Y            0x0200
#define CONT_X            0x0400
#define CONT_D            0x0800 
#define CONT_ANALOG_UP    0x1000
#define CONT_ANALOG_DOWN  0x2000
#define CONT_ANALOG_LEFT  0x4000
#define CONT_ANALOG_RIGHT 0x8000

class Controller 
{

 public:
	Controller(class Maple *maple, Dword setting);
	~Controller();    
	
	void Controller::checkInput();
	
	// This struct copied almost verbatim from http://mc.pp.se/dc/maplebus.html
	// Thank you Marcus Comstedt!
	struct DeviceInfo {
		Dword suppFuncs;         // func codes supported - big endian
		Dword funcData[3];       // info about func codes - big endian
		Byte region;             // peripheral's region code
		Byte connectorDirection; // ?? that's odd
		char productName[30];
		char productLicense[60];
		Word standbyPower;       // standby power consumption - little endian
		Word maxPower;           // maximum power consumption - little endian
	};
	
	struct DeviceInfo *returnInfo(Dword port);
	int returnSize(Dword port);
	Dword *returnData(Dword port);
	
 private:
	Controller();
	
	void (Controller::*updateInput)();
	void updateKeyboardMouse();
	void updateJoystick();
	
	class Maple *maple;
	
	struct DeviceInfo devices[4];
	int deviceSize[4];
	
	// peripheral-specific button data minus 2 dwords of maple header
	// information (e.g., frame header and identification)
	Dword keyboard[2];
	Dword mouse[6];
	Dword controller[1];
	
	Dword *deviceData[4];
};

#endif
