# Builds the HTTP server
PRCTL_TEST_SRC := $(wildcard ./prctl_test/*.c)
PRCTL_TEST_HDR := $(wildcard ./prctl_test/*.h)
PRCTL_TEST_OBJ := $(PRCTL_TEST_SRC:%.c=%.o)
PRCTL_TEST_OBJ := $(PRCTL_TEST_OBJ:./prctl_test/%=./.obj/prctl_test.d/%)
PRCTL_TEST_DEP := $(PRCTL_TEST_OBJ:%.o:%.d)
PRCTL_TEST_FLAGS := -fms-extensions

-include $(PRCTL_TEST_DEP)

all: .obj/prctl_test
.obj/prctl_test: $(PRCTL_TEST_OBJ)
	gcc -g -static $(PRCTL_TEST_FLAGS) $(CFLAGS) -o "$@" $^

.obj/prctl_test.d/%.o : prctl_test/%.c $(PRCTL_TEST_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(PRCTL_TEST_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
