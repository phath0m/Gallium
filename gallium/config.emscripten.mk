###
### Configuration for emscripten (Web-assembly target)
###
GALLIUM=bin/gallium.js
GALLIUM_USE_READLINE=no

MY_CC=emcc
MY_LD=emcc

# Append anything to CFLAGS or LDFLAGS
MY_CFLAGS=-DGALLIUM_USE_EMSCRIPTEN
MY_LDFLAGS=-s EXPORTED_RUNTIME_METHODS=["ccall"]