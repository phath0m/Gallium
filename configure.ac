AC_INIT([gallium], [1.0], [bug-automake@gnu.org])

# Check for programs

AC_PROG_CC
if test -z "$LD" ; then
	LD=$CC
fi
AC_SUBST(LD)

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([
 Makefile
])

# AC_DEFINE_SUBST(NAME, VALUE, DESCRIPTION)
# -----------------------------------------
AC_DEFUN([AC_DEFINE_SUBST], [
  AC_DEFINE_UNQUOTED([$1], [$2], [$3])
  AC_SUBST([$1], ['$2'])
])

if test "$prefix" = "NONE" ; then
  prefix="/usr/local"
fi

AC_DEFINE_SUBST([MODULE_PATH], [["$prefix/gallium/modules"]], [Gallium module location])

AX_CHECK_COMPILE_FLAG([-fno-gcse -fno-crossjumping],
  [AX_APPEND_FLAG([-fno-gcse -fno-crossjumping])],
  [AC_MSG_WARN([Compiler will not support computed gotos.])]
)

AC_ARG_ENABLE(emit-debug,
  [  --enable-emit-debug.     Enable additional debugging for bytecode compilation. Not recommended.],
  [
    if test "$enableval" = "yes"; then
      AC_DEFINE([ENABLE_EMIT_DEBUG], 1, [Enable debug printing regarding instructions emitted.])
      AC_MSG_NOTICE([Enabling emit debugging.])
    fi
  ],
  []
)

AC_ARG_ENABLE(memory-debug,
  [  --enable-memory-debug.     Enable additional debugging for memory leaks. Not recommended.],
  [
    if test "$enableval" = "yes"; then
      AC_DEFINE([ENABLE_MEMORY_DEBUG], 1, [Enable debug printing regarding memory usage.])
      AC_MSG_NOTICE([Enabling memory leak debugging.])
    fi
  ],
  []
)

# Optional libraries
AC_CHECK_LIB([readline], [readline])

AC_OUTPUT