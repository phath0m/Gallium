# Gallium Programming Language

Gallium is a dynamically typed object oriented toy programming language with an emphasis on simpicity and minimalism. The interpreter is intended to be portable and requires no external dependencies.

As of right now, this is a hobby project written purely for fun. Gallium is inspired by several languages, most notably Python with its runtime and standard library.

### Compiling

It should be possible to compile the interpreter by invoking its Makefile

```
cd gallium
make
```

The interpreter binary itself will be created in `./gallium/bin/gallium`

### Examples

#### Hello, World
```go
print("Hello, World!")
```

More examples can be found inside the `examples` directory
