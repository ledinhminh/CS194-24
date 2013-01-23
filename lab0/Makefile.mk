# Builds a small, interactive shell that you can play with
all: .lab0/interactive_initrd.gz

.lab0/interactive_initrd.gz: lab0/interactive_config busybox/busybox \
                             linux/usr/gen_init_cpio lab0/interactive_init
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

# This init just prints out the kernel version and then promptly exits
all: .lab0/version_initrd.gz

.lab0/version_initrd.gz: lab0/version_config busybox/busybox \
                         linux/usr/gen_init_cpio lab0/version_init
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

# Builds an initrd with a /init that links against a shared library
# that isn't availiable.  This will cause a kernel panic
all: .lab0/shared_initrd.gz

.lab0/shared_initrd.gz: lab0/shared_config .lab0/shared_init \
                        linux/usr/gen_init_cpio
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

.lab0/shared_init: lab0/init.c
	gcc $(CFLAGS) "$<" -o "$@"


# Builds an entirely static /init, this should shut down the system
# cleanly, not producing any kernel panic messages.
all: .lab0/static_initrd.gz

.lab0/static_initrd.gz: lab0/static_config .lab0/static_init \
                        linux/usr/gen_init_cpio
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

.lab0/static_init: lab0/init.c
	gcc $(CFLAGS) -static "$<" -o "$@"

# Builds an initrd without any /init -- this will kernel panic when it
# boots from this initramfs because there is no process for the kernel
# to run to startup the userspace
all: .lab0/noinit_initrd.gz

.lab0/noinit_initrd.gz: lab0/noinit_config \
                        linux/usr/gen_init_cpio
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

# Builds an initrd that runs a userspace version of the fish test --
# this still writes to VGA memory, it just doesn't run the fish system
# calls in kernel space
all: .lab0/user_initrd.gz
.lab0/user_initrd.gz: lab0/user_config .lab0/user_init \
                      linux/usr/gen_init_cpio lab0/frame*.txt
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

.lab0/user_init: lab0/test_harness.c lab0/fish_impl.h lab0/fish_impl.c \
                 lab0/fish_compat.h lab0/fish_syscall.h lab0/fish_syscall.c
	gcc $(CFLAGS) -static -DUSERSPACE_TEST $^ -o "$@"

# Builds an initrd that runs a kernelspace version of the fish test
all: .lab0/kernel_initrd.gz
.lab0/kernel_initrd.gz: lab0/kernel_config .lab0/kernel_init \
                        linux/usr/gen_init_cpio lab0/frame*.txt
	linux/usr/gen_init_cpio "$<" | gzip > "$@"

.lab0/kernel_init: lab0/test_harness.c lab0/fish_impl.h \
                   lab0/fish_syscall.h lab0/fish_syscall.c
	gcc $(CFLAGS) -static $^ -o "$@"

# Enforces that some of these files are copied into the kernel 
linux: linux/drivers/gpu/vga/fish_impl.c
linux/drivers/gpu/vga/fish_impl.c: lab0/fish_impl.c
	cp "$<" "$@"
linux: linux/drivers/gpu/vga/fish_impl.h
linux/drivers/gpu/vga/fish_impl.h: lab0/fish_impl.h
	cp "$<" "$@"
linux: linux/drivers/gpu/vga/sys_fish.c
linux/drivers/gpu/vga/sys_fish.c: lab0/sys_fish.c
	cp "$<" "$@"
linux: linux/drivers/gpu/vga/fish_compat.h
linux/drivers/gpu/vga/fish_compat.h: lab0/fish_compat.h
	cp "$<" "$@"
