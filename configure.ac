# configure.ac for Arculator
#

AC_PREREQ([2.61])
AC_INIT(Arculator, v1.0, Sarah Walker <pcem@pcem-emulator.co.uk>, arculator)

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O


AC_MSG_CHECKING([whether to enable debugging])
AC_ARG_ENABLE(debug,
          AC_HELP_STRING([--enable-debug], [build debug executable]))
if test "$enable_debug" = "yes"; then
   CFLAGS="-Wall -O0 -g -D_DEBUG"
   CXXFLAGS="-Wall -O0 -g -D_DEBUG"
   AC_MSG_RESULT([yes])
else
   CFLAGS="-O3"
   CXXFLAGS="-O3"
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([for libz])
AC_CHECK_LIB(z, inflateEnd, [], \
          [echo "You need to install the Zlib library."
           exit -1])

SDL_VERSION=2.0.1
AM_PATH_SDL2($SDL_VERSION,
   :,
   AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

AM_OPTIONS_WXCONFIG
reqwx=3.0.0
AM_PATH_WXCONFIG($reqwx, wxWin=1)
if test "$wxWin" != 1; then
   AC_MSG_ERROR([
      wxWidgets must be installed on your system.
 
      Please check that wx-config is in path, the directory
      where wxWidgets libraries are installed (returned by
      'wx-config --libs' or 'wx-config --static --libs' command)
      is in LD_LIBRARY_PATH or equivalent variable and
      wxWidgets version is $reqwx or above.
      ])
fi

AC_CHECK_LIB([pthread], [pthread_create])

build_macosx="no"
build_linux="no"
build_win="no"
build_other="no"

case "$host" in
    *-*-darwin*)
       LIBS="$LIBS -framework OpenAL"
       build_macosx="yes"
       ;;
    *-*-cygwin* | *-*-mingw32*)
       AC_CHECK_LIB([openal], [alGetError], [], \
          [echo "You need to install the OpenAL library."
           exit -1])
       build_win="yes"
       AC_SUBST(SET_MAKE, 'MAKE=mingw32-make')
       ;;
    *-*-linux*)
       AC_CHECK_LIB([openal], [alGetError], [], \
           [echo "You need to install the OpenAL library."
           exit -1])
       build_linux="yes"
       ;;
    *)
       AC_CHECK_LIB([openal], [alGetError], [], \
           [echo "You need to install the OpenAL library."
           exit -1])
       build_other="yes"
       ;;
esac

LIBS="$LIBS $WX_LIBS $SDL_LIBS"

AM_CONDITIONAL(OS_LINUX, [test "$build_linux" = "yes"])
AM_CONDITIONAL(OS_WINDOWS, [test "$build_win" = "yes"])
AM_CONDITIONAL(OS_OTHER, [test "$build_other" = "yes"])
AM_CONDITIONAL(OS_MACOSX, [test "$build_macosx" = "yes"])

AC_OUTPUT([Makefile src/Makefile])