# Builds the HTTP server
HTTPD_SRC := $(wildcard ./httpd/*.c)
HTTPD_HDR := $(wildcard ./httpd/*.h)
HTTPD_OBJ := $(HTTPD_SRC:%.c=%.o)
HTTPD_OBJ := $(HTTPD_OBJ:./httpd/%=./.obj/httpd.d/%)
HTTPD_DEP := $(HTTPD_OBJ:%.o:%.d)
HTTPD_FLAGS := -fms-extensions

-include $(HTTPD_DEP) 

all: .obj/httpd
.obj/httpd: $(HTTPD_OBJ)
	gcc -g -static $(HTTPD_FLAGS) $(CFLAGS) -o "$@" $^

.obj/httpd.d/%.o : httpd/%.c $(HTTPD_HDR)
	mkdir -p `dirname $@`
	gcc -g -c -o $@ $(HTTPD_FLAGS) $(CFLAGS) -MD -MP -MF ${@:.o=.d} $<
