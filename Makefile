obj-m := parblo-coast10.o
KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

