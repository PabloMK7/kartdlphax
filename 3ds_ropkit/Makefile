HAXNAME := payload

all:	payload.bin miniapp.bin

clean:
	rm -f $(HAXNAME).elf payload.bin ropbin.h miniapp.elf miniapp.bin miniapp.h

payload.bin: $(HAXNAME).elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary $(HAXNAME).elf payload.bin
	./bin2c -o ropbin.h payload.bin
	cp ropbin.h ../plugin/Includes/ropbin.h

$(HAXNAME).elf: $(HAXNAME).s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x14250000 $< -o $(HAXNAME).elf

miniapp.bin: miniapp.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary miniapp.elf miniapp.bin
	./bin2c -o miniapp.h miniapp.bin
	cp miniapp.h ../plugin/Includes/miniapp.h
	
miniapp.elf: miniapp.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 $< -o miniapp.elf