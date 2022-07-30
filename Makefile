###
### User Settings. These are the default settings! For further configuration
### edit config.mk!!!
###
MY_CC:=gcc
MY_LD:=gcc
GALLIUM=src/gallium
GALLIUM_CORE=src/libgallium.a

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

# Interpreter binary
GALLIUM_OBJECTS	= src/gallium.o

# Core library
CORE_OBJECTS =	src/lib.o src/hashmap_impl.o src/linkedlist_impl.o src/stringbuilder.o \
			   	src/arraylist_impl.o src/compiler.o src/ast.o src/lexer.o src/vm.o \
				src/parser.o src/object.o src/builtins/bool.o \
				src/builtins/builtin.o src/builtins/class.o \
				src/builtins/code.o src/builtins/dict.o \
				src/builtins/exception.o src/builtins/enum.o \
				src/builtins/enumerable.o src/builtins/file.o \
				src/builtins/function.o src/builtins/integer.o \
				src/builtins/list.o src/builtins/method.o \
				src/builtins/mixin.o src/builtins/module.o \
				src/builtins/mutable_string.o src/builtins/null.o \
				src/builtins/range.o src/builtins/string.o \
				src/builtins/tuple.o src/builtins/weakref.o \
				src/modules/ast.o src/modules/builtins.o \
				src/modules/os.o src/modules/parser.o

all: $(GALLIUM_CORE) $(GALLIUM)

$(GALLIUM): $(GALLIUM_CORE) $(GALLIUM_OBJECTS)
	$(LD) -o $@ $(GALLIUM_OBJECTS) $(GALLIUM_CORE) $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

$(GALLIUM_CORE): $(CORE_OBJECTS)
	$(AR) -rcs $@ $^
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

install:
	mkdir -p "$(DESTDIR)/$(PREFIX)/bin"
	mkdir -p "$(DESTDIR)/$(PREFIX)/share/gallium/examples"
	cp $(GALLIUM_CORE) "$(DESTDIR)/$(PREFIX)/lib"
	cp -rp ./include/* "$(DESTDIR)/$(PREFIX)/include"
	cp -rp ./examples/*.ga "$(DESTDIR)/$(PREFIX)/share/gallium/examples"

clean:
	rm -f $(CORE_OBJECTS) $(GALLIUM_CORE) $(GALLIUM) $(GALLIUM_OBJECTS)