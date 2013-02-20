# Builds the webapp that speaks to 
WEBCLOCK_SRC := $(wildcard ./webclock/*.c)
WEBCLOCK_HDR := $(wildcard ./webclock/*.h)
WEBCLOCK_OBJ := $(WEBCLOCK_SRC:%.c=%.o)
WEBCLOCK_OBJ := $(WEBCLOCK_OBJ:./webclock/%=./.obj/webclock.d/%)
WEBCLOCK_DEP := $(WEBCLOCK_OBJ:%.o:%.d)
WEBCLOCK_FLAGS := -pthread -DMINUTE_CLOCK

-include $(WEBCLOCK_DEP) 

all: .obj/webclock
.obj/webclock: $(WEBCLOCK_OBJ)
	gcc -static -g $(WEBCLOCK_FLAGS) $(CFLAGS) -o "$@" $^

.obj/webclock.d/%.o : webclock/%.c $(WEBCLOCK_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(WEBCLOCK_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<

# Builds a second webapp, which might be useful for testing some edge
# cases of your cache
HOURCLOCK_SRC := $(wildcard ./webclock/*.c)
HOURCLOCK_HDR := $(wildcard ./webclock/*.h)
HOURCLOCK_OBJ := $(HOURCLOCK_SRC:%.c=%.o)
HOURCLOCK_OBJ := $(HOURCLOCK_OBJ:./webclock/%=./.obj/hourclock.d/%)
HOURCLOCK_DEP := $(HOURCLOCK_OBJ:%.o:%.d)
HOURCLOCK_FLAGS := -pthread -DHOUR_CLOCK

-include $(HOURCLOCK_DEP) 

all: .obj/hourclock
.obj/hourclock: $(HOURCLOCK_OBJ)
	gcc -static -g $(HOURCLOCK_FLAGS) $(CFLAGS) -o "$@" $^

.obj/hourclock.d/%.o : webclock/%.c $(HOURCLOCK_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(HOURCLOCK_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
