# Builds the webapp that speaks to 
WEBAPP_SRC := $(wildcard ./webclock/*.c)
WEBAPP_HDR := $(wildcard ./webclock/*.h)
WEBAPP_OBJ := $(WEBAPP_SRC:%.c=%.o)
WEBAPP_OBJ := $(WEBAPP_OBJ:./webclock/%=./.obj/webclock.d/%)
WEBAPP_DEP := $(WEBAPP_OBJ:%.o:%.d)
WEBAPP_FLAGS := -pthread

-include $(WEBAPP_DEP) 

all: .obj/webclock
.obj/webclock: $(WEBAPP_OBJ)
	gcc -g $(WEBAPP_FLAGS) $(CFLAGS) -o "$@" $^

.obj/webclock.d/%.o : webclock/%.c $(WEBAPP_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(WEBAPP_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
