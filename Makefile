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
include lab*/Makefile.mk

# These are recursive builds
.PHONY: busybox
busybox::
	$(MAKE) -C busybox
.PHONY: linux
linux:
	$(MAKE) -C linux

# We can't track dependencies through those recursive builds, but we
# can kind of do it -- this always rebuilds the subprojects, but only
# rebuilds these projects when something actually changes in a
# subproject
busybox/busybox: busybox
linux/usr/gen_init_cpio: linux

# Adds a trivial clean target -- this gets most things
.PHONY: clean
clean::
	make -C linux clean
	make -C busybox clean
	rm -f .lab*/*
