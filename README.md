# Gallium Programming Language

Gallium is a full-fledged dynamically typed, objected oriented toy programming
language written in C. My goals for this project are to develop a dynamic
scripting language that can compile with zero dependencies other than the C
standard library.

Gallium is inspired by Python, JavaScript and probably several other languages
as well. Unlike Python though, it incorporates a C-style syntax.


### Trying Gallium

In order to compile Gallium, simply run `make`. Gallium can be installed
locally with `make install`. The `DESTDIR` environmental variable can be
utilized to specify where to install Gallium to.

```
# make
# DESTDIR=/usr/local make install
# gallium
>>> 2 + 2
4
```

Alternatively, you can also try Gallium online using the
[Gallium Playground](https://galliumlang.dev).

### Language Syntax
Several Gallium programs can be found inside the examples directory.