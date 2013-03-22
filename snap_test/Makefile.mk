# Builds the HTTP server
SNAP_TEST_SRC := $(wildcard ./snap_test/*.c)
SNAP_TEST_HDR := $(wildcard ./snap_test/*.h)
SNAP_TEST_OBJ := $(SNAP_TEST_SRC:%.c=%.o)
SNAP_TEST_OBJ := $(SNAP_TEST_OBJ:./snap_test/%=./.obj/snap_test.d/%)
SNAP_TEST_DEP := $(SNAP_TEST_OBJ:%.o:%.d)
SNAP_TEST_FLAGS := -fms-extensions -isystem linux/include

-include $(SNAP_TEST_DEP)

all: .obj/snap_test
.obj/snap_test: $(SNAP_TEST_OBJ)
	gcc -g -static $(SNAP_TEST_FLAGS) $(CFLAGS) -o "$@" $^

.obj/snap_test.d/%.o : snap_test/%.c $(SNAP_TEST_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(SNAP_TEST_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
