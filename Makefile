KERNELDIR ?= /home/studentas/Downloads/buildroot-rpi3/output/build/linux-v7.1

# Build a single module from multiple source files
obj-m += lcd_screen.o
lcd_screen-y += src/main.o src/lcd_i2c.o src/lcd_hw.o src/lcd_chardev.o
CROSS := /home/studentas/Downloads/buildroot-rpi3/output/host/bin/aarch64-buildroot-linux-gnu-

all:
	make ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KERNELDIR) M=$(PWD) modules

clean:
	make ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KERNELDIR) M=$(PWD) clean