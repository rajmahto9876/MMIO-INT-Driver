obj-m := gpio_intr.o
gpio_intr-objs += main.o
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-

#set KERN_DIR to linux source location (Beaglebone)
KERN_DIR = /home/raj-pc/RajDev/Linux_Device_Driver/linux

#set KERN_DIR to linux source location (Linux Ubuntu)
HOST_KERN_DIR = /lib/modules/$(shell uname -r)/build/

ccflags-y := -DMODULE_DEBUG


all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) modules

clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) clean

clean_host:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean	

help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=$(PWD) help

host:
	make -C $(HOST_KERN_DIR) M=$(PWD)  modules

copy:
	scp *.ko debian@192.168.7.2:/home/debian/drivers/05_hw096_Disp/02_Basic_INTR

insert: 
	sudo insmod *.ko

remove:
	sudo rmmod *.ko

get_info:
	file *.ko
	modinfo *.ko

show_logs:
	sudo dmesg -xT | tail -20

perm:
	sudo chmod 777 /dev/my_device

preprocess:
	make -C $(HOST_KERN_DIR) M=$(PWD)  main.i

user_app:
	gcc user_app.c -o user_app.o

clean_user:
	rm user_app.o

