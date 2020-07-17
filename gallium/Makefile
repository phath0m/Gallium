CC=gcc
LD=gcc

CFLAGS=-c -std=gnu99 -O3 -fno-gcse -fno-crossjumping -Wall -Werror -I .
#CFLAGS=-c -std=gnu99 -DDEBUG_EMIT -Wall -Werror -I . -g

OBJECTS += gallium.o
OBJECTS += ds/dict.o
OBJECTS += ds/list.o
OBJECTS += ds/stringbuf.o
OBJECTS += ds/vec.o
OBJECTS += compiler.o
OBJECTS += ast.o
OBJECTS += lexer.o
OBJECTS += parser.o
OBJECTS += object.o
OBJECTS += vm.o
OBJECTS += builtins/bool.o
OBJECTS += builtins/builtin.o
OBJECTS += builtins/class.o
OBJECTS += builtins/code.o
OBJECTS += builtins/dict.o
OBJECTS += builtins/exception.o
OBJECTS += builtins/file.o
OBJECTS += builtins/function.o
OBJECTS += builtins/integer.o
OBJECTS += builtins/list.o
OBJECTS += builtins/method.o
OBJECTS += builtins/module.o
OBJECTS += builtins/null.o
OBJECTS += builtins/range.o
OBJECTS += builtins/string.o
OBJECTS += builtins/tuple.o
OBJECTS += builtins/weakref.o
OBJECTS += modules/ast.o
OBJECTS += modules/builtins.o
OBJECTS += modules/parser.o

GALLIUM=bin/gallium

all: $(GALLIUM)

$(GALLIUM): $(OBJECTS)
	mkdir -p bin
	$(LD) -o $@ $^ -lgcc
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(OBJECTS) $(GALLIUM)
