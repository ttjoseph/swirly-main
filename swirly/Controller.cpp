#include "Controller.h"
#include <string.h>

Controller::Controller(Maple *maple, Dword setting) 
{
	// key repeat enable
	SDL_EnableKeyRepeat(250, 30);
	
	if(setting) 
	{
		// configure for keyboard/mouse

		// XXX: standbyPower and maxPower for a real keyboard?
		strcpy(devices[0].productName, "Swirly Fake Keyboard");
		strcpy(devices[0].productLicense, "Unknown");
		devices[0].suppFuncs = switchEndian(MAPLE_KEYBOARD);
		devices[0].region = 0xff;
		devices[0].connectorDirection = 0;
		devices[0].standbyPower = 0x01ae; 
		devices[0].maxPower = 0x01f4;
		// maple-specific information
		deviceSize[0] = 4;
		deviceData[0] = keyboard;
		
		// XXX: standbyPower and maxPower for a real mouse?
		strcpy(devices[1].productName, "Swirly Fake Mouse");
		strcpy(devices[1].productLicense, "Unknown");
		devices[1].suppFuncs = switchEndian(MAPLE_MOUSE);
		devices[1].region = 0xff;
		devices[1].connectorDirection = 0;
		devices[1].standbyPower = 0x01ae; 
		devices[1].maxPower = 0x01f4;
		// maple-specific information
		deviceSize[1] = 6;
		deviceData[1] = mouse;
		
		devices[2].suppFuncs = 0;
		devices[3].suppFuncs = 0;
		
		updateInput = &Controller::updateKeyboardMouse;
		
	} 
	else 
	{

		// configure for a controller + VMU
		
		// XXX: should be:
		// A0: Dreamcast Controller (01000000: Controller)
		// A1: Visual Memory        (0e000000: Clock, LCD, MemoryCard)
		
		strcpy(devices[0].productName, "Swirly Dreamcast Controller");
		strcpy(devices[0].productLicense, "Unknown");
		devices[0].suppFuncs = switchEndian(MAPLE_CONTROLLER);
		// parameters taken from a real DC controller
		devices[0].region = 0xff;
		devices[0].connectorDirection = 0;
		devices[0].standbyPower = 0x01ae; 
		devices[0].maxPower = 0x01f4; 
		
		// maple-specific information
		deviceSize[0] = 3;
		deviceData[0] = controller;
		
#if 0
		strcpy(devices[1].productName, "Swirly Visual Memory");
		strcpy(devices[1].productLicense, "Unknown");
		devices[1].suppFuncs = switchEndian(MAPLE_MEMCARD | MAPLE_LCD | MAPLE_CLOCK);
		// parameters taken from a real VMU
		devices[1].region = 0xff;
		devices[1].connectorDirection = 0;
		devices[1].standbyPower = 0x007c; 
		devices[1].maxPower = 0x0082; 
		
		deviceSize[1] = 0; 
#else
		devices[1].suppFuncs = 0;
#endif
		
		devices[2].suppFuncs = 0;
		devices[3].suppFuncs = 0;
		
		updateInput = &Controller::updateJoystick;
		
		// ignore mouse since we only emulate a controller
		SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	}
}

Controller::~Controller() {}

struct Controller::DeviceInfo *Controller::returnInfo(Dword port) 
{
	return devices+((port>>6)&3);
}

int Controller::returnSize(Dword port) 
{
	return deviceSize[(port>>6)&3];
}

Dword *Controller::returnData(Dword port) {
	return deviceData[(port>>6)&3];
}

void Controller::checkInput() 
{
	(this->*updateInput)();
}

void Controller::updateKeyboardMouse() 
{

	// XXX: not all keys are 'emulated' and we only process a key at a time

	keyboard[0] = keyboard[1] = 0;
	mouse[0] = mouse [1] = mouse[2] = mouse [3] = mouse[4] = mouse [5] = 0;
	
	SDL_Event e;
	SDL_PollEvent(&e);
	
	if(e.type==SDL_KEYDOWN) 
        {
		
		int test = e.key.keysym.sym;
		
		switch(test) 
		{
		case SDLK_LCTRL:  keyboard[0] |= 0x01; break;
		case SDLK_LSHIFT: keyboard[0] |= 0x02; break;
		case SDLK_LALT:   keyboard[0] |= 0x04; break;
		case SDLK_RCTRL:  keyboard[0] |= 0x10; break;
		case SDLK_RSHIFT: keyboard[0] |= 0x20; break;
		case SDLK_RALT:   keyboard[0] |= 0x40; break;
			
		case SDLK_BACKSPACE: keyboard[0] = 0x2a<<24; break;
		case SDLK_RETURN:    keyboard[0] = 0x28<<24; break;
		case SDLK_ESCAPE:    keyboard[0] = 0x27<<24; break;
		case SDLK_TAB:       keyboard[0] = 0x09<<24; break;
		case SDLK_SPACE:     keyboard[0] = 0x2c<<24; break;
			
		case SDLK_RIGHT: keyboard[0] = 0x4f<<24; break;
		case SDLK_LEFT:  keyboard[0] = 0x50<<24; break;
		case SDLK_DOWN:  keyboard[0] = 0x51<<24; break;
		case SDLK_UP:    keyboard[0] = 0x52<<24; break;

		case SDLK_0: keyboard[0] = 0x27<<24;
		default: break;
		}

		if(test>=SDLK_a && test <= SDLK_z) keyboard[0] = (test-93)<<24;
		if(test>=SDLK_1 && test <= SDLK_9) keyboard[0] = (test-19)<<24;
	}

	// XXX: for mouse, use SDL_GetRelativeMouseState(...)
}

void Controller::updateJoystick() 
{

	static bool lctrlPressed = false;
	static Byte up, down, left, right; 

	// XXX: improve analog button code. (not just as a linear function)
	// XXX: add right/left trigger and second joystick code

	SDL_Event e;
	SDL_PollEvent(&e);
	
	controller[0] = ~0;         
	controller[1] = 0x80808080; 
	
	switch(e.type) 
	{
	case SDL_KEYDOWN:

		switch(e.key.keysym.sym) 
                {
		case SDLK_a:      controller[0] &= ~CONT_A; break;
		case SDLK_s:      controller[0] &= ~CONT_B; break;
		case SDLK_d:      controller[0] &= ~CONT_X; break;
		case SDLK_f:      controller[0] &= ~CONT_Y; break;
		case SDLK_SPACE:  controller[0] &= ~CONT_Z; break;

		case SDLK_LEFT:   
			if(lctrlPressed) 
                        {
				if(left!=0x00) left -= 1;
				controller[1] |= left;
				controller[1] &= 0x8080807f;
			}
			else    controller[0] &= ~CONT_LEFT; 
			break;
		case SDLK_RIGHT:
			if(lctrlPressed) 
                        {
				if(right!=0xff) right += 1;
				controller[1] |= right;
			}
			else    controller[0] &= ~CONT_RIGHT; 
			break;
		case SDLK_UP:   
			if(lctrlPressed) 
                        {
				if(up!=0x00) up -= 1; 
				controller[1] |= (up << 8);
				controller[1] &= 0x80807f80;
			} 
			else    controller[0] &= ~CONT_UP; 
			break;
		case SDLK_DOWN:   
			if(lctrlPressed) 
                        {
				if(down!=0xff) down += 1;
				controller[1] |= (down << 8);
			}
			else    controller[0] &= ~CONT_DOWN; 
			break;

		case SDLK_RETURN: controller[0] &= ~CONT_START; break;

		case SDLK_LCTRL:  lctrlPressed = true; break;
		default: break;
		}
		break;
	case SDL_KEYUP:

		// reset analog buttons if "released"
		switch(e.key.keysym.sym) 
		{
		case SDLK_LCTRL:  
			lctrlPressed = false; 
			up = 0x80; down = 0x80; left = 0x80; right = 0x80;
			break;
		case SDLK_LEFT:  left  = 0x80; break;
		case SDLK_RIGHT: right = 0x80; break;
		case SDLK_UP:    up    = 0x80; break;
		case SDLK_DOWN:  down  = 0x80; break;
		default: break;
		}
		break;
	}
}
