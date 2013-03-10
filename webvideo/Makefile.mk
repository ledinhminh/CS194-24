# A slightly more complicated soft real-time process
WEBVIDEO_SRC := $(wildcard ./webvideo/*.c)
WEBVIDEO_HDR := $(wildcard ./webvideo/*.h)
WEBVIDEO_OBJ := $(WEBVIDEO_SRC:%.c=%.o)
WEBVIDEO_OBJ := $(WEBVIDEO_OBJ:./webvideo/%=./.obj/webvideo.d/%)
WEBVIDEO_DEP := $(WEBVIDEO_OBJ:%.o:%.d)
BOGO_MIPS    := $(shell cat /proc/cpuinfo  | grep bogomips | head -n1 | cut -d ':' -f2 | sed s/\ //g)
WEBVIDEO_FLAGS := -pthread -DBOGO_MIPS=$(BOGO_MIPS)

-include $(WEBVIDEO_DEP) 

all: .obj/webvideo
.obj/webvideo: $(WEBVIDEO_OBJ)
	gcc -static -g $(WEBVIDEO_FLAGS) $(CFLAGS) -o "$@" $^ -lrt

.obj/webvideo.d/%.o : webvideo/%.c $(WEBVIDEO_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(WEBVIDEO_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
