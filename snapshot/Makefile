# (C) 2026, Cornell University
# All rights reserved.
# Modified for QEMU-only support by OSI 26spring staff

# [lab2]: compile with ult
ENABLE_ULT ?= 1
# [lab3]: TODO: change "NAIVE" to "MLFQ"
SCHEDULER=MLFQ
# [lab4]: TODO: change "SOFTINT" to "ECALL"
SYSCALLFUNC=ECALL
# [lab5]: TODO: change  "VMOFF" to "VMON"
IFVM=VMON
# [lab6]: TODO: change  "FLASH" to "SDCARD"
DISK=SDCARD
# [lab7]: TODO: change  "RWFSOFF" to "RWFSON"
RWFS=RWFSON

QEMU        = qemu-system-riscv32

ifeq ($(TOOLCHAIN), GNU)
# Use the official GNU toolchain.
# https://github.com/riscv-collab/riscv-gnu-toolchain
RISCV_CC    = riscv32-unknown-elf-gcc
OBJDUMP     = riscv32-unknown-elf-objdump
OBJCOPY     = riscv32-unknown-elf-objcopy
else
# Use the pre-compiled GNU toolchain binaries from xPack.
# https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases
RISCV_CC    = riscv-none-elf-gcc
OBJDUMP     = riscv-none-elf-objdump
OBJCOPY     = riscv-none-elf-objcopy
endif

DEBUG       = build/debug
RELEASE     = build/release

APPS_DEPS   = apps/*.* library/egos.h library/*/* Makefile
EGOS_DEPS   = earth/* grass/* library/egos.h library/*/* Makefile

FILESYS     = 1
LDFLAGS     = -nostdlib -lc -lgcc
INCLUDE     = -Ilibrary -Ilibrary/elf -Ilibrary/file -Ilibrary/libc -Ilibrary/syscall
CFLAGS      = -march=rv32ima_zicsr -mabi=ilp32 -Wl,--gc-sections -ffunction-sections -fdata-sections -fdiagnostics-show-option -ggdb -g
DEBUG_FLAGS = --source --all-headers --demangle --line-numbers --wide

SYSAPP_ELFS = $(patsubst %.c, $(RELEASE)/%.elf, $(notdir $(wildcard apps/system/*.c)))
USRAPP_ELFS = $(patsubst %.c, $(RELEASE)/user/%.elf, $(notdir $(wildcard apps/user/*.c)))
USRAPP_ELFS := $(filter-out $(RELEASE)/user/ult%, $(USRAPP_ELFS))  # remove all files with ult (ult, ult_test)

ULT_ELF :=
ifeq ($(ENABLE_ULT),1)  # add ult to compilation when enabled
ULT_ELF := $(RELEASE)/user/ult.elf
endif

egos: $(USRAPP_ELFS) $(SYSAPP_ELFS) $(ULT_ELF) $(RELEASE)/egos.elf

$(RELEASE)/egos.elf: $(EGOS_DEPS)
	@printf "$(YELLOW)-------- Compile EGOS --------$(END)\n"
	$(RISCV_CC) $(CFLAGS) $(INCLUDE) -DKERNEL -D$(SCHEDULER) -D$(SYSCALLFUNC) -D$(IFVM) -D$(DISK) -D$(RWFS) \
		$(filter %.s, $(wildcard $^)) $(filter %.c, $(wildcard $^)) -Tlibrary/elf/egos.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(DEBUG)/egos.lst

$(SYSAPP_ELFS): $(RELEASE)/%.elf : apps/system/%.c $(APPS_DEPS)
	@printf "Compile app $(CYAN)%s$(END) => %s\n" $(patsubst %.c, %, $(notdir $<)) $@
	@$(RISCV_CC) $(CFLAGS) $(INCLUDE) -DFILESYS=$(FILESYS) -DKERNEL -D$(SYSCALLFUNC) -D$(RWFS) -Iapps apps/app.s $(filter %.c, $(wildcard $^)) -Tlibrary/elf/app.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(patsubst %.c, $(DEBUG)/%.lst, $(notdir $<))

$(USRAPP_ELFS): $(RELEASE)/user/%.elf : apps/user/%.c $(APPS_DEPS)
	@mkdir -p $(DEBUG) $(RELEASE) $(RELEASE)/user
	@printf "Compile app $(CYAN)%s$(END) => %s\n" $(patsubst %.c, %, $(notdir $<)) $@
	@$(RISCV_CC) $(CFLAGS) $(INCLUDE) -D$(SYSCALLFUNC) -D$(RWFS) -Iapps apps/app.s $(filter %.c, $(wildcard $^)) -Tlibrary/elf/app.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(patsubst %.c, $(DEBUG)/%.lst, $(notdir $<))

$(RELEASE)/user/ult.elf: apps/user/ult.c apps/user/ult_test.c apps/user/thread.s $(APPS_DEPS)
	@printf "Compile app $(CYAN)%s$(END) => %s\n" $(patsubst %.c, %, $(notdir $<)) $@
	$(RISCV_CC) $(CFLAGS) $(INCLUDE) -D$(SYSCALLFUNC) -Iapps apps/app.s apps/user/thread.s $(filter %.c, $(wildcard $^)) -Tlibrary/elf/app.lds $(LDFLAGS) -o $@
	@$(OBJDUMP) $(DEBUG_FLAGS) $@ > $(patsubst %.c, $(DEBUG)/%.lst, $(notdir $<))

install: egos
	@printf "$(GREEN)-------- Create the Disk & ROM Images --------$(END)\n"
	$(OBJCOPY) -O binary $(RELEASE)/egos.elf tools/egos.bin
	$(CC) tools/mkfs.c library/file/file$(FILESYS).c -DMKFS -DFILESYS=$(FILESYS) -D$(RWFS) $(INCLUDE) -o tools/mkfs
	cd tools; rm -f disk.img qemuROM.bin; ./mkfs

QEMU_MACHINE = -M virt -smp 1 -m 8M -bios tools/egos.bin
QEMU_GRAPHIC = -nographic # -device VGA,addr=0x2 -serial mon:stdio
QEMU_FLASH_1 = -drive if=pflash,format=raw,unit=1,file=tools/qemuROM.bin
QEMU_ETH_NET = -device e1000,netdev=E1000,addr=0x3,mac=52:54:00:00:00:01 -netdev socket,id=E1000,listen=:1234
QEMU_ETH_HTTP = -device e1000,netdev=E1000,addr=0x3,mac=52:54:00:00:00:01 -netdev user,id=E1000,hostfwd=tcp::8080-:80
QEMU_SD_CARD = -device sdhci-pci,addr=0x1 -device sd-card,drive=MMC -drive if=none,file=tools/disk.img,format=raw,id=MMC
QEMU_GDB     = -gdb tcp::6640 -S

qemu: install
	@printf "$(YELLOW)-------- Simulate on QEMU-RISCV --------$(END)\n"
	$(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD) $(QEMU_ETH_NET)

qemu-gdb: install
	@printf "$(YELLOW)-------- Simulate on QEMU-RISCV (with GDB) --------$(END)\n"
	@printf "$(YELLOW)-------- run 'gdb' in another window --------$(END)\n"
	$(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD) $(QEMU_ETH_NET) $(QEMU_GDB)

qemu-http: install
	@printf "$(YELLOW)-------- Simulate on QEMU-RISCV (HTTP) --------$(END)\n"
	$(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD) $(QEMU_ETH_HTTP)


clean:
	rm -rf build tools/egos.bin tools/mkfs tools/disk.img tools/qemuROM.bin hello.bin hello.lst hello.elf thread.bin thread.lst thread.elf

GREEN = \033[1;32m
YELLOW = \033[1;33m
CYAN = \033[1;36m
END = \033[0m
