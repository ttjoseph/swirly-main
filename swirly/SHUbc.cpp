#include "SHUbc.h"
#include "Debugger.h"

SHUbc::SHUbc() 
{
    bbra = 0x0000;
    bbrb = 0x0000;
    brcr = 0x0000;
}

SHUbc::~SHUbc()
{

}

Dword SHUbc::hook(int event, Dword addr, Dword data)
{
    // the user break controller is not implemented at all ... 
    addr &= 0xff;

    switch(addr)
    {
    case 0x20:
    case 0x08:
    case 0x14:
	/**/
	break;
    default:
	debugger->print("UBC: warning: unknown address %02x accessed\n", addr);
    }

    return 0;
}
