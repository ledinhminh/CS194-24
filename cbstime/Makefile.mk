# A simple real-time application that communicates over shared memory
# to a controlling process.
CBSTIME_SRC := ./cbstime/cbs.c ./cbstime/rt.c ./cbstime/drf.c ./cbstime/drfq.c
CBSTIME_HDR := $(wildcard ./cbstime/*.h)
CBSTIME_OBJ := $(CBSTIME_SRC:%.c=%.o)
CBSTIME_OBJ := $(CBSTIME_OBJ:./cbstime/%=./.obj/cbstime.d/%)
CBSTIME_DEP := $(CBSTIME_OBJ:%.o:%.d)
BOGO_MIPS    := $(shell cat /proc/cpuinfo  | grep bogomips | head -n1 | cut -d ':' -f2 | sed s/\ //g)
CBSTIME_FLAGS := -pthread -DBOGO_MIPS=$(BOGO_MIPS)

-include $(CBSTIME_DEP) 

all: .obj/cbstime
.obj/cbstime: $(CBSTIME_OBJ)
	gcc -static -g $(CBSTIME_FLAGS) $(CFLAGS) -o "$@" $^ -lrt

.obj/cbstime.d/%.o : cbstime/%.c $(CBSTIME_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(CBSTIME_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<

# A simple real-time application that communicates over shared memory
# to a controlling process.
CBSTIMECTL_SRC := ./cbstime/ctl.c
CBSTIMECTL_HDR := $(wildcard ./cbstimectl/*.h)
CBSTIMECTL_OBJ := $(CBSTIMECTL_SRC:%.c=%.o)
CBSTIMECTL_OBJ := $(CBSTIMECTL_OBJ:./cbstime/%=./.obj/cbstimectl.d/%)
CBSTIMECTL_DEP := $(CBSTIMECTL_OBJ:%.o:%.d)
CBSTIMECTL_FLAGS := -pthread

-include $(CBSTIMECTL_DEP) 

all: .obj/cbstimectl
.obj/cbstimectl: $(CBSTIMECTL_OBJ)
	gcc -static -g $(CBSTIMECTL_FLAGS) $(CFLAGS) -o "$@" $^

.obj/cbstimectl.d/%.o : cbstime/%.c $(CBSTIMECTL_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(CBSTIMECTL_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
