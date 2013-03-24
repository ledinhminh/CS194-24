# Builds the HTTP server
PRCTL_TEST_SRC := $(wildcard ./streaming_test/*.c)
PRCTL_TEST_HDR := $(wildcard ./streaming_test/*.h)
PRCTL_TEST_OBJ := $(PRCTL_TEST_SRC:%.c=%.o)
PRCTL_TEST_OBJ := $(PRCTL_TEST_OBJ:./streaming_test/%=./.obj/streaming_test.d/%)
PRCTL_TEST_DEP := $(PRCTL_TEST_OBJ:%.o:%.d)
PRCTL_TEST_FLAGS := -fms-extensions

-include $(PRCTL_TEST_DEP)

all: .obj/streaming_test
.obj/streaming_test: $(PRCTL_TEST_OBJ)
	gcc -g -static $(PRCTL_TEST_FLAGS) $(CFLAGS) -o "$@" $^

.obj/streaming_test.d/%.o : streaming_test/%.c $(PRCTL_TEST_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(PRCTL_TEST_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
