###
### Configuration for emscripten (Web-assembly target)
###
GALLIUM=bin/gallium.js
GALLIUM_USE_READLINE=no

MY_CC=emcc
MY_LD=emcc

# Append anything to CFLAGS or LDFLAGS
MY_CFLAGS=-DGALLIUM_USE_EMSCRIPTEN -DGALLIUM_TARGET_LIBRARY
MY_LDFLAGS=-s EXPORTED_RUNTIME_METHODS=["ccall","print","FS"] -s EXPORT_ES6=1 -s USE_ES6_IMPORT_META=0 -s ENVIRONMENT='web' -sFORCE_FILESYSTEM -sSINGLE_FILE 