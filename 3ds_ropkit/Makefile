
all:	payload_eur.bin miniapp_eur.bin payload_usa.bin miniapp_usa.bin payload_jap.bin miniapp_jap.bin

clean:
	rm -f payload_eur.elf payload_eur.bin ropbin_eur.h miniapp_eur.elf miniapp_eur.bin miniapp_eur.h
	rm -f payload_usa.elf payload_usa.bin ropbin_usa.h miniapp_usa.elf miniapp_usa.bin miniapp_usa.h
	rm -f payload_jap.elf payload_jap.bin ropbin_jap.h miniapp_jap.elf miniapp_jap.bin miniapp_jap.h

payload_eur.bin: payload_eur.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary payload_eur.elf payload_eur.bin
	./bin2c -o ropbin_eur.h payload_eur.bin
	cp ropbin_eur.h ../plugin/Includes/ropbin_eur.h

payload_usa.bin: payload_usa.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary payload_usa.elf payload_usa.bin
	./bin2c -o ropbin_usa.h payload_usa.bin
	cp ropbin_usa.h ../plugin/Includes/ropbin_usa.h

payload_jap.bin: payload_jap.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary payload_jap.elf payload_jap.bin
	./bin2c -o ropbin_jap.h payload_jap.bin
	cp ropbin_jap.h ../plugin/Includes/ropbin_jap.h

payload_eur.elf: payload.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 -DEUR_BUILD $< -o payload_eur.elf

payload_usa.elf: payload.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 -DUSA_BUILD $< -o payload_usa.elf

payload_jap.elf: payload.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 -DJAP_BUILD $< -o payload_jap.elf

miniapp_eur.bin: miniapp_eur.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary miniapp_eur.elf miniapp_eur.bin
	./bin2c -o miniapp_eur.h miniapp_eur.bin
	cp miniapp_eur.h ../plugin/Includes/miniapp_eur.h

miniapp_usa.bin: miniapp_usa.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary miniapp_usa.elf miniapp_usa.bin
	./bin2c -o miniapp_usa.h miniapp_usa.bin
	cp miniapp_usa.h ../plugin/Includes/miniapp_usa.h

miniapp_jap.bin: miniapp_jap.elf
	C:\devkitPro\devkitARM\bin\arm-none-eabi-objcopy -O binary miniapp_jap.elf miniapp_jap.bin
	./bin2c -o miniapp_jap.h miniapp_jap.bin
	cp miniapp_jap.h ../plugin/Includes/miniapp_jap.h
	
miniapp_eur.elf: miniapp.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 -DEUR_BUILD $< -o miniapp_eur.elf

miniapp_usa.elf: miniapp.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 -DUSA_BUILD $< -o miniapp_usa.elf

miniapp_jap.elf: miniapp.s
	C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib -Ttext=0x100000 -DJAP_BUILD $< -o miniapp_jap.elf