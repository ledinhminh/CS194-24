# Do not:
# o  use make's built-in rules and variables
#    (this increases performance and avoids hard-to-debug behaviour);
# o  print "Entering directory ...";
.SUFFIXES:
MAKEFLAGS += -rR --no-print-directory

# Avoid funny character set dependencies
unexport LC_ALL
LC_COLLATE=C
LC_NUMERIC=C
export LC_COLLATE LC_NUMERIC

CFLAGS=-Wall -Werror

# Source each lab's Makefile seperately -- this way we can provide the
# labs to the students one at a time
-include */Makefile.mk

# These are recursive builds
.PHONY: busybox
busybox::
	$(MAKE) -C busybox
.PHONY: linux
linux:
	$(MAKE) -C linux
.PHONY: qemu
.qemu-configure.stamp:
	rm -rf qemu/build qemu/install
	mkdir -p qemu/build qemu/install
	cd qemu/build; ../configure --prefix=`pwd`/qemu/install --target-list=x86_64-softmmu
	touch .qemu-configure.stamp
qemu: .qemu-configure.stamp
	$(MAKE) -C qemu/build
all: qemu

# We can't track dependencies through those recursive builds, but we
# can kind of do it -- this always rebuilds the subprojects, but only
# rebuilds these projects when something actually changes in a
# subproject
busybox/busybox: busybox
linux/usr/gen_init_cpio: linux

# Adds a trivial clean target -- this gets most things
.PHONY: clean
clean::
	$(MAKE) -C linux clean
	$(MAKE) -C busybox clean
	rm -rf .lab0/* .obj/*
	rm -rf qemu/build qemu/install .qemu-configure.stamp

