KVERS = $(shell uname -r)

# kernel modules
obj-m += globalmem.o

# Specify flags for the module compilation.
# EXTRA_FLAGS= -g -O0

build: kernel_modules

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	
