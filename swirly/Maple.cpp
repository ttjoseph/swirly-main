#include "Maple.h"
#include "Overlord.h"
#include "Debugger.h"
#include <stdio.h>
#include <string.h>

Maple::Maple(class SHCpu *cpu)
{
	this->cpu = cpu;
	// initialize a DeviceInfo for a single gamepad
	strcpy(gamepad.productName, "Swirly Fake Gamepad");
	strcpy(gamepad.productLicense, "I like cheese");
	gamepad.suppFuncs = Overlord::switchEndian(MAPLE_CONTROLLER);
	gamepad.standbyPower = 1; // this is completely made up
	gamepad.maxPower = 2; // so is this

	for(int i=0; i<sizeof(buttonState); i++)
		buttonState[i] = 0;
}

Maple::~Maple()
{

}

Dword Maple::hook(int eventType, Dword addr, Dword data)
{
	switch(eventType)
	{
	case MMU_READ_DWORD:
		switch(addr)
		{
			// XXX: is this Right?
		case 0xa05f6900: // TA's VBL reg
			return 0x88; // hack

		case 0xa05f688c: // XXX: some SPU-related reg?
			return 1; // hack

		case 0xa05f6c18: // Maple DMA
			cpu->debugger->print("Maple: check DMA status, PC=%08X\n", cpu->PC);
			return 0; // hack, pretend DMA is done

		default:
			return 0;
		}
	case MMU_WRITE_DWORD:
		switch(addr)
		{
			case 0xa05f6c04: // DMA address set
				// we are given a physical address here, NOT a virtual address
				dmaAddr = data;
				return 0;
			case 0xa05f6c14: // Maple enable
				if(data & 1)
					cpu->debugger->print("Maple: enabled\n");
				else
					cpu->debugger->print("Maple: disabled\n");
				return 0;
			case 0xa05f6c18: // DMA start
				if(data & 1) // if bit 0 is set, begin transfer
				{
					int cmd, recipient, sender, addtlwords, last, port, length;
					// XXX: finish me!
					Dword transferDesc1 = cpu->mmu->access(dmaAddr, MMU_READ_DWORD);
					Dword transferDesc2 = cpu->mmu->access(dmaAddr+4, MMU_READ_DWORD);
					Dword frameHeader = cpu->mmu->access(dmaAddr+8, MMU_READ_DWORD);
					frameHeader = Overlord::switchEndian(frameHeader);
					last = Overlord::bits(transferDesc1, 31, 31);
					port = Overlord::bits(transferDesc1, 17, 16);
					length = Overlord::bits(transferDesc1, 7, 0);
					cmd = Overlord::bits(frameHeader, 31, 24);
					recipient = Overlord::bits(frameHeader, 23, 16);
					sender = Overlord::bits(frameHeader, 15, 8);
					addtlwords = Overlord::bits(frameHeader, 7, 0);
					Dword resultAddr = Overlord::bits(transferDesc2, 28, 5) << 5;

					/*
					cpu->debugger->print("Maple: DMA transfer from %08X: ", dmaAddr);
					cpu->debugger->print("last %d, port %d, length %d ", last, port, length);
					cpu->debugger->print("Result addr: %08X\n", transferDesc2);
					cpu->debugger->print("Maple: command %08X: ", frameHeader);
					cpu->debugger->print("cmd: %d ", cmd);
					cpu->debugger->print("recipient: %d ", recipient);
					cpu->debugger->print("sender: %d ", sender);
					cpu->debugger->print("addtl words: %d\n", addtlwords);
					*/

					switch(cmd)
					{
					case MAPLE_REQ_DEVICE_INFO:
						cpu->debugger->print("Maple: request device info of %02x (on port %d)\n", recipient, Overlord::bits(recipient, 7, 6));
						// if we are being asked about A0
						if(recipient == (D5))
						{ // ...then say there's a gamepad connected
							// printf("resultaddr %08x\n", resultAddr);
							FrameHeader fh;
							fh.command = MAPLE_DEVICE_INFO_RESP;
							fh.recipient = sender;
							fh.sender = recipient;
							fh.numWordsFollowing = sizeof(DeviceInfo) / 4;
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
							for(int i=0; i<(sizeof(DeviceInfo) / 4); i++)
								cpu->mmu->writeDwordToExternal(resultAddr+i*4+4, ((Dword*)&gamepad)[i]);
						}	else
						{
							FrameHeader fh;
							fh.command = MAPLE_NORESP_ERR;
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
						}
						break;

					case MAPLE_GET_CONDITION:
						cpu->debugger->print("Maple: get condition of %02x (on port %d)\n", recipient, Overlord::bits(recipient, 7, 6));
						cpu->overlord->handleEvents();
						//printf("%08x params[0]=%08x\n", dmaAddr, cpu->mmu->access(dmaAddr+4, MMU_READ_DWORD));
						// are we asking about our lone gamepad?
						// then say what buttons are down
						if(recipient == D5)
						{
							FrameHeader fh;
							fh.command = MAPLE_DATA_TRANSFER_RESP;
							fh.recipient = sender;
							fh.sender = recipient;
							fh.numWordsFollowing = 2;
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
							Dword cond = 0xffffffff;
							if(buttonState[BUTTON_START] == 1)
							{
								printf("Maple: Start button pressed - resultAddr = %08x\n", resultAddr);
								cond &= ~D3;
							}
							cpu->mmu->writeDwordToExternal(resultAddr+4, Overlord::switchEndian(MAPLE_CONTROLLER));
							//cpu->mmu->writeDwordToExternal(resultAddr+8, Overlord::switchEndian(cond));
							cpu->mmu->writeDwordToExternal(resultAddr+8, cond);
						} else
						{
							FrameHeader fh;
							fh.command = MAPLE_NORESP_ERR;
							cpu->mmu->writeDwordToExternal(resultAddr, *((Dword*)&fh));
						}
						break;
					}
				}
				return 0;
			case 0xa05f6c80: // timeout / speed
				cpu->debugger->print("Maple: timeout set to %d\n", data >> 16);
				return 0;
			case 0xa05f6c8c: // unknown - a magic value is usually written to this
				cpu->debugger->print("Maple: register 8c written with %08x\n", data);
				return 0;
		}
	}
	return 0xbeefbeef;
}
