-include params.mk

export # export all variable to applications sub-Makefile

CC = mipsel-unknown-elf-gcc
AS = mipsel-unknown-elf-as
LD = mipsel-unknown-elf-ld
DU = mipsel-unknown-elf-objdump
AR = mipsel-unknown-elf-ar

# Defaults values for hardware parameters and applications
# These parameters should be defined in the params.mk file
ARCH      ?= pathname
X_SIZE    ?= 1
Y_SIZE    ?= 1
NB_PROCS  ?= 1
NB_TTYS   ?= 1
FBF_WIDTH ?= 256
IOC_TYPE  ?= BDV
APPLIS    ?= shell

# build the list of applications used as argument by genmap
GENMAP_APPLIS := $(addprefix --,$(APPLIS))

# build the list of application.py (used as dependencies by genmap)
APPLIS_PY      = applications/classif/classif.py        \
                 applications/convol/convol.py          \
                 applications/coproc/coproc.py          \
                 applications/display/display.py        \
                 applications/dhrystone/dhrystone.py    \
                 applications/gameoflife/gameoflife.py  \
                 applications/ocean/ocean.py            \
                 applications/raycast/raycast.py        \
                 applications/router/router.py          \
                 applications/shell/shell.py            \
                 applications/sort/sort.py              \
                 applications/transpose/transpose.py 

# build the list of applications to be executed (used in the all rule)
APPLIS_ELF    := $(addsuffix /appli.elf,$(addprefix applications/,$(APPLIS)))

# Build PYTHONPATH
PYTHONPATH := $(shell find . -name *.py | grep -o "\(.*\)/" | sort -u | tr '\n' :)

# check hardware platform definition
ifeq ($(wildcard $(ARCH)),)
$(error please define in ARCH parameter the path to the platform)
endif 

### Rules that don't build a target file
.PHONY: all dirs list extract clean clean-disk install-disk $(APPLIS_ELF)

### Objects to be linked for the drivers library
DRIVERS_OBJS = build/drivers/dma_driver.o      \
               build/drivers/cma_driver.o      \
               build/drivers/xcu_driver.o      \
               build/drivers/bdv_driver.o      \
               build/drivers/hba_driver.o      \
               build/drivers/sdc_driver.o      \
               build/drivers/spi_driver.o      \
               build/drivers/rdk_driver.o      \
               build/drivers/iob_driver.o      \
               build/drivers/mmc_driver.o      \
               build/drivers/mwr_driver.o      \
               build/drivers/nic_driver.o      \
               build/drivers/tim_driver.o      \
               build/drivers/tty_driver.o      \
               build/drivers/pic_driver.o

### Objects to be linked for kernel.elf
KERNEL_OBJS  = build/common/utils.o            \
               build/common/kernel_locks.o     \
               build/common/kernel_barriers.o  \
               build/common/tty0.o             \
               build/common/vmem.o             \
               build/common/kernel_malloc.o    \
               build/fat32/fat32.o             \
               build/kernel/giet.o             \
               build/kernel/switch.o           \
               build/kernel/ctx_handler.o      \
               build/kernel/exc_handler.o      \
               build/kernel/sys_handler.o      \
               build/kernel/irq_handler.o      \
               build/kernel/kernel_init.o

### Objects to be linked for boot.elf
BOOT_OBJS    = build/common/utils.o            \
               build/common/kernel_locks.o     \
               build/common/kernel_barriers.o  \
               build/common/tty0.o             \
               build/common/pmem.o             \
               build/common/vmem.o             \
               build/common/kernel_malloc.o    \
               build/fat32/fat32.o             \
               build/kernel/ctx_handler.o      \
               build/kernel/irq_handler.o      \
               build/kernel/sys_handler.o      \
               build/kernel/switch.o           \
               build/boot/boot.o               \
               build/boot/boot_entry.o

### Objects to be linked for the user library
USER_OBJS     = build/libs/malloc.o            \
                build/libs/mwmr_channel.o      \
                build/libs/stdio.o             \
                build/libs/stdlib.o            \
                build/libs/string.o            \
                build/libs/user_barrier.o      \
                build/libs/user_lock.o         \
                build/libs/user_sqt_lock.o     

### Objects to be linked for the math library
MATH_OBJS     = build/libs/math/e_pow.o        \
                build/libs/math/e_rem_pio2.o   \
                build/libs/math/k_cos.o        \
                build/libs/math/k_rem_pio2.o   \
                build/libs/math/k_sin.o        \
                build/libs/math/s_copysign.o   \
                build/libs/math/s_fabs.o       \
                build/libs/math/s_finite.o     \
                build/libs/math/s_floor.o      \
                build/libs/math/s_isnan.o      \
                build/libs/math/sqrt.o         \
                build/libs/math/s_rint.o       \
                build/libs/math/s_scalbn.o     \
                build/libs/math/s_sin.o        \
                build/libs/math/s_cos.o        \
                build/libs/math/e_sqrt.o       


CFLAGS = -Wall -ffreestanding -mno-gpopt -mips32 -g -O2 \
		 -fno-delete-null-pointer-checks

GIET_INCLUDE = -Igiet_boot    \
               -Igiet_kernel  \
               -Igiet_xml     \
               -Igiet_fat32   \
               -Igiet_drivers \
               -Igiet_common  \
               -Igiet_libs    \
               -I.

DISK_IMAGE  := hdd/virt_hdd.dmg

### The Mtools used to build the FAT32 disk image perform a few sanity checks,
### to make sure that the disk is indeed an MS-DOS disk. However, the size
### of the disk image used by the Giet-VM is not MS-DOS compliant.
### Setting this variable prevents these checks.
MTOOLS_SKIP_CHECK := 1

##################################
### first rule executed (make all)
all: dirs                            \
     map.bin                         \
     hard_config.h                   \
     giet_vsegs.ld                   \
     install-disk
	mdir -/ -b -i $(DISK_IMAGE) ::/

#####################################################
### create build directories
dirs:
	@mkdir -p build/boot
	@mkdir -p build/common
	@mkdir -p build/drivers
	@mkdir -p build/fat32
	@mkdir -p build/kernel
	@mkdir -p build/libs/math
	@mkdir -p hdd

#####################################################
### make a recursive list of the virtual disk content
list:
	 mdir -/ -w -i $(DISK_IMAGE) ::/

########################################################
### copy the files generated by the virtual prototype on
### the virtual disk "home" directory to the giet_vm home directory
extract:
	mcopy -o -i $(DISK_IMAGE) ::/home .

########################################
### clean all binary files
clean:
	rm -f *.o *.elf *.bin *.txt core
	rm -f hard_config.h giet_vsegs.ld map.bin map.xml
	rm -rf build/
	cd applications/classif      && $(MAKE) clean && cd ../..
	cd applications/convol       && $(MAKE) clean && cd ../..
	cd applications/coproc       && $(MAKE) clean && cd ../..
	cd applications/display      && $(MAKE) clean && cd ../..
	cd applications/dhrystone    && $(MAKE) clean && cd ../..
	cd applications/gameoflife   && $(MAKE) clean && cd ../..
	cd applications/ocean        && $(MAKE) clean && cd ../..
	cd applications/raycast      && $(MAKE) clean && cd ../..
	cd applications/router       && $(MAKE) clean && cd ../..
	cd applications/shell        && $(MAKE) clean && cd ../..
	cd applications/sort         && $(MAKE) clean && cd ../..
	cd applications/transpose    && $(MAKE) clean && cd ../..

########################################
### delete disk image
clean-disk:
	rm -rf hdd/

########################################
### Copy content in the disk image
### create the three build / misc / home directories
### store the images files into misc
install-disk: $(DISK_IMAGE) build/kernel/kernel.elf $(APPLIS_ELF)
	mmd -o -i $< ::/bin               || true
	mmd -o -i $< ::/bin/kernel        || true
	mmd -o -i $< ::/bin/classif       || true
	mmd -o -i $< ::/bin/convol        || true
	mmd -o -i $< ::/bin/coproc        || true
	mmd -o -i $< ::/bin/dhrystone     || true
	mmd -o -i $< ::/bin/display       || true
	mmd -o -i $< ::/bin/gameoflife    || true
	mmd -o -i $< ::/bin/ocean         || true
	mmd -o -i $< ::/bin/raycast       || true
	mmd -o -i $< ::/bin/router        || true
	mmd -o -i $< ::/bin/shell         || true
	mmd -o -i $< ::/bin/sort          || true
	mmd -o -i $< ::/bin/transpose     || true
	mmd -o -i $< ::/misc              || true
	mmd -o -i $< ::/home              || true
	mcopy -o -i $< map.bin ::/
	mcopy -o -i $< build/kernel/kernel.elf ::/bin/kernel
	mcopy -o -i $< applications/classif/appli.elf ::/bin/classif          || true
	mcopy -o -i $< applications/convol/appli.elf ::/bin/convol            || true
	mcopy -o -i $< applications/coproc/appli.elf ::/bin/coproc            || true
	mcopy -o -i $< applications/dhrystone/appli.elf ::/bin/dhrystone      || true
	mcopy -o -i $< applications/display/appli.elf ::/bin/display          || true
	mcopy -o -i $< applications/gameoflife/appli.elf ::/bin/gameoflife    || true
	mcopy -o -i $< applications/ocean/appli.elf ::/bin/ocean              || true
	mcopy -o -i $< applications/raycast/appli.elf ::/bin/raycast          || true
	mcopy -o -i $< applications/router/appli.elf ::/bin/router            || true
	mcopy -o -i $< applications/shell/appli.elf ::/bin/shell              || true
	mcopy -o -i $< applications/sort/appli.elf ::/bin/sort                || true
	mcopy -o -i $< applications/transpose/appli.elf ::/bin/transpose      || true
	mcopy -o -i $< images/images_128.raw ::/misc
	mcopy -o -i $< images/philips_1024.raw ::/misc
	mcopy -o -i $< images/lena_256.raw ::/misc
	mcopy -o -i $< images/bridge_256.raw ::/misc
	mcopy -o -i $< images/couple_512.raw ::/misc
	mcopy -o -i $< images/door_32.raw ::/misc
	mcopy -o -i $< images/handle_32.raw ::/misc
	mcopy -o -i $< images/rock_32.raw ::/misc
	mcopy -o -i $< images/wood_32.raw ::/misc

#########################
### Disk image generation 
### This requires the generic LINUX/MacOS script "create_dmg" script
### written by C.Fuguet. (should be installed in GIET-VM root directory).
$(DISK_IMAGE): build/boot/boot.elf
	rm -f $@
	./create_dmg create $(basename $@)
	dd if=$@ of=temp.dmg count=65536
	mv temp.dmg $@
	dd if=build/boot/boot.elf of=$@ seek=2 conv=notrunc

#########################################################################
### mapping generation: map.bin / map.xml / hard_config.h / giet_vsegs.ld
### TODO add dépendancies on appli.py files : $(APPLIS_DEPS) 
map.bin hard_config.h giet_vsegs.ld: $(ARCH)/arch.py $(APPLIS_PY)
	giet_python/genmap --arch=$(ARCH)     \
                       --x=$(X_SIZE)      \
                       --y=$(Y_SIZE)      \
                       --p=$(NB_PROCS)    \
                       --tty=$(NB_TTYS)   \
                       --fbf=$(FBF_WIDTH) \
                       --ioc=$(IOC_TYPE)  \
                       --giet=.           \
                       $(GENMAP_APPLIS)   \
                       --xml=.

################################
### drivers library compilation
build/drivers/%.o: giet_drivers/%.c \
                   giet_drivers/%.h \
                   hard_config.h    \
                   giet_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

build/drivers/libdrivers.a: $(DRIVERS_OBJS)
	$(AR) -rcs $@ $(DRIVERS_OBJS)

##########################
### fat32 compilation
build/fat32/fat32.o: giet_fat32/fat32.c \
                     giet_fat32/fat32.h \
                     hard_config.h      \
                     giet_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

##########################
### common compilation
build/common/%.o: giet_common/%.c   \
                  giet_common/%.h   \
                  hard_config.h     \
                  giet_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

########################
### boot compilation
### Copy bootloader into sector 2 of disk image
build/boot/boot.elf: $(BOOT_OBJS)            \
                     giet_boot/boot.ld       \
                     build/drivers/libdrivers.a
	$(LD) -o $@ -T giet_boot/boot.ld $(BOOT_OBJS) -Lbuild/drivers -ldrivers
	$(DU) -D $@ > $@.txt

build/boot/boot.o: giet_boot/boot.c          \
                   giet_common/utils.h       \
                   giet_fat32/fat32.h        \
                   giet_common/vmem.h        \
                   hard_config.h             \
                   giet_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

build/boot/boot_entry.o: giet_boot/boot_entry.S \
                         hard_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

#########################
### kernel compilation
build/kernel/kernel.elf: $(KERNEL_OBJS)        \
                         giet_kernel/kernel.ld \
                         build/drivers/libdrivers.a
	$(LD) -o $@ -T giet_kernel/kernel.ld $(KERNEL_OBJS) -Lbuild/drivers -ldrivers
	$(DU) -D $@ > $@.txt

build/kernel/%.o: giet_kernel/%.c    \
                  hard_config.h      \
                  giet_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

build/kernel/%.o: giet_kernel/%.s    \
                  hard_config.h      \
                  giet_config.h
	$(CC) $(GIET_INCLUDE) $(CFLAGS)  -c -o $@ $<

###########################
### user library compilation
build/libs/%.o: giet_libs/%.c        \
                     giet_libs/%.h   \
                     hard_config.h   \
                     giet_config.h
	$(CC) $(CFLAGS) $(GIET_INCLUDE) -c -o $@ $<

build/libs/libuser.a: $(USER_OBJS)
	$(AR) -rcs $@ $^

################################
### math library compilation
build/libs/math/%.o: giet_libs/math/%.c             \
                     giet_libs/math/math_private.h  \
                     giet_libs/math.h
	$(CC) $(CFLAGS) $(GIET_INCLUDE) -c -o $@ $<

build/libs/libmath.a: $(MATH_OBJS)
	$(AR) -rcs $@ $^

########################################
### classif   application compilation
applications/classif/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/classif

########################################
### convol   application compilation
applications/convol/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/convol

########################################
### coproc   application compilation
applications/coproc/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/coproc

########################################
### dhrystone   application compilation
applications/dhrystone/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/dhrystone

########################################
### display  application compilation
applications/display/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/display

########################################
### gameoflife  application compilation
applications/gameoflife/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/gameoflife

########################################
### ocean  application compilation
applications/ocean/appli.elf: build/libs/libmath.a  build/libs/libuser.a
	cd applications/ocean && $(MAKE) && cd ../..

########################################
### raycast application compilation
applications/raycast/appli.elf: build/libs/libmath.a build/libs/libuser.a
	$(MAKE) -C applications/raycast

########################################
### router  application compilation
applications/router/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/router

########################################
### shell  application compilation
applications/shell/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/shell

########################################
### sort  application compilation
applications/sort/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/sort

########################################
### transpose compilation
applications/transpose/appli.elf: build/libs/libuser.a
	$(MAKE) -C applications/transpose

