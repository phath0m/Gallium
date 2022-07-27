###
### User Settings. These are the default settings! For further configuration
### edit config.mk!!!
###
MY_CC:=gcc
MY_LD:=gcc
GALLIUM=src/gallium

ifndef GALLIUM_CONFIG
    include config.mk
else
    include config.${GALLIUM_CONFIG}.mk
endif

CFLAGS=-c -std=gnu99 -O3 -Wall -Werror -I include $(MY_CFLAGS)
LDFLAGS= $(MY_LDFLAGS)
CC=$(MY_CC)
LD=$(MY_LD)

ifeq ($(CC), gcc)
    CFLAGS += -fno-gcse -fno-crossjumping
    LDFLAGS += -lgcc
endif

ifeq ($(GALLIUM_USE_READLINE), yes)
    CFLAGS += -DGALLIUM_USE_READLINE
    LDFLAGS += -lreadline
endif

OBJECTS += src/gallium.o
OBJECTS += src/ds/dict.o
OBJECTS += src/ds/list.o
OBJECTS += src/ds/stringbuf.o
OBJECTS += src/ds/vec.o
OBJECTS += src/compiler.o
OBJECTS += src/ast.o
OBJECTS += src/lexer.o
OBJECTS += src/parser.o
OBJECTS += src/object.o
OBJECTS += src/vm.o
OBJECTS += src/builtins/bool.o
OBJECTS += src/builtins/builtin.o
OBJECTS += src/builtins/class.o
OBJECTS += src/builtins/code.o
OBJECTS += src/builtins/dict.o
OBJECTS += src/builtins/exception.o
OBJECTS += src/builtins/enum.o
OBJECTS += src/builtins/enumerable.o
OBJECTS += src/builtins/file.o
OBJECTS += src/builtins/function.o
OBJECTS += src/builtins/integer.o
OBJECTS += src/builtins/list.o
OBJECTS += src/builtins/method.o
OBJECTS += src/builtins/mixin.o
OBJECTS += src/builtins/module.o
OBJECTS += src/builtins/mutable_string.o
OBJECTS += src/builtins/null.o
OBJECTS += src/builtins/range.o
OBJECTS += src/builtins/string.o
OBJECTS += src/builtins/tuple.o
OBJECTS += src/builtins/weakref.o
OBJECTS += src/modules/ast.o
OBJECTS += src/modules/builtins.o
OBJECTS += src/modules/os.o
OBJECTS += src/modules/parser.o

all: $(GALLIUM)

$(GALLIUM): $(OBJECTS)
	mkdir -p bin
	$(LD) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

install:
	mkdir -p "$(DESTDIR)/$(PREFIX)/bin"
	mkdir -p "$(DESTDIR)/$(PREFIX)/share/gallium/examples"
	cp $(GALLIUM) "$(DESTDIR)/$(PREFIX)/bin/gallium"
	cp -rp ../examples/*.ga "$(DESTDIR)/$(PREFIX)/share/gallium/examples"

clean:
	rm -f $(OBJECTS) $(GALLIUM)