CC=gcc
LD=gcc

CFLAGS=-c -std=gnu99 -Wall -Werror -I .

OBJECTS += gallium.o
OBJECTS += ds/dict.o
OBJECTS += ds/list.o
OBJECTS += ds/stringbuf.o
OBJECTS += compiler/compiler.o
OBJECTS += compiler/ast.o
OBJECTS += compiler/lexer.o
OBJECTS += compiler/parser.o
OBJECTS += runtime/object.o
OBJECTS += runtime/vm.o
OBJECTS += runtime/builtins/bool.o
OBJECTS += runtime/builtins/builtin.o
OBJECTS += runtime/builtins/builtin_funcs.o
OBJECTS += runtime/builtins/class.o
OBJECTS += runtime/builtins/dict.o
OBJECTS += runtime/builtins/exception.o
OBJECTS += runtime/builtins/file.o
OBJECTS += runtime/builtins/function.o
OBJECTS += runtime/builtins/integer.o
OBJECTS += runtime/builtins/list.o
OBJECTS += runtime/builtins/method.o
OBJECTS += runtime/builtins/module.o
OBJECTS += runtime/builtins/null.o
OBJECTS += runtime/builtins/range.o
OBJECTS += runtime/builtins/string.o
OBJECTS += runtime/builtins/tuple.o
OBJECTS += runtime/builtins/weakref.o

GALLIUM=bin/gallium

all: $(GALLIUM)

$(GALLIUM): $(OBJECTS)
	mkdir -p bin
	$(LD) -o $@ $^ -lgcc
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(OBJECTS) $(GALLIUM)
