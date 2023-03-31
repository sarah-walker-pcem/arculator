# configure.ac for Arculator
#

AC_PREREQ([2.71])
AC_INIT([Arculator],[v2.0],[Sarah Walker <b-em@bbcmicro.com>],[arculator])

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
LT_INIT([disable-static], [shared])


AC_MSG_CHECKING([whether to enable debugging])
AC_ARG_ENABLE(debug,
          AS_HELP_STRING([--enable-debug],[build debug executable]))
if test "$enable_debug" = "yes"; then
   CFLAGS_BUILD="-Wall -O0 -g -D_DEBUG -DDEBUG_LOG"
   AC_MSG_RESULT([yes])
else
   CFLAGS_BUILD="-O3"
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to build for release])
AC_ARG_ENABLE(release_build,
          AS_HELP_STRING([--enable-release-build],[build for release]))
if test "$enable_release_build" = "yes"; then
   AC_MSG_RESULT([yes])
   CFLAGS_RELEASE=" -DRELEASE_BUILD"
else
   AC_MSG_RESULT([no])
fi
AM_CONDITIONAL(RELEASE_BUILD, [test "$enable_release_build" = "yes"])

AC_MSG_CHECKING([whether to build podules])
AC_ARG_ENABLE(podules,
          AS_HELP_STRING([--disable-podules],[don't build podules]))
if test "$enable_podules" != "no"; then
   AC_MSG_RESULT([yes])
else
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
       build_macosx="yes"
       ;;
    *-*-cygwin* | *-*-mingw32*)
       build_win="yes"
       AC_SUBST(SET_MAKE, 'MAKE=mingw32-make')
       ;;
    *-*-linux*)
       AC_MSG_CHECKING([whether to use PCAP for networking])
       AC_ARG_ENABLE(pcap,
           AS_HELP_STRING([--enable-pcap],[build using PCAP]))
       if test "$enable_pcap" = "yes"; then
          AC_MSG_RESULT([yes])
          CFLAGS_PCAP=" -DENABLE_PCAP"
       else
          AC_MSG_RESULT([no])
       fi

       if test "$enable_podules" != "no"; then
          AC_CHECK_LIB(asound, snd_pcm_open,
               [],
               [echo "You need to install the ALSA library."
                exit -1])
          if test "$enable_pcap" = "yes"; then
             AC_CHECK_LIB(pcap, pcap_open_live,
                 [],
                 [echo "You need to install the PCAP library."
                  exit -1])
          fi
       fi
       build_linux="yes"
       ;;
    *)
       build_other="yes"
       ;;
esac

CFLAGS="$CFLAGS_BUILD $CFLAGS_RELEASE $CFLAGS_PCAP"
CXXFLAGS="$CFLAGS_BUILD $CFLAGS_RELEASE $CFLAGS_PCAP"
LIBS="$LIBS $WX_LIBS $SDL_LIBS"

AM_CONDITIONAL(OS_LINUX, [test "$build_linux" = "yes"])
AM_CONDITIONAL(OS_WINDOWS, [test "$build_win" = "yes"])
AM_CONDITIONAL(OS_OTHER, [test "$build_other" = "yes"])
AM_CONDITIONAL(OS_MACOSX, [test "$build_macosx" = "yes"])

AM_CONDITIONAL(BUILD_PODULES, [test "$enable_podules" != "no"])
AM_CONDITIONAL(ENABLE_PCAP, [test "$enable_pcap" = "yes"])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

AC_CONFIG_FILES([podules/aeh50/src/Makefile podules/aeh54/src/Makefile podules/aka05/src/Makefile podules/aka10/src/Makefile podules/aka12/src/Makefile podules/aka16/src/Makefile podules/aka31/src/Makefile podules/designit_e200/src/Makefile podules/lark/src/Makefile podules/midimax/src/Makefile podules/morley_uap/src/Makefile podules/oak_scsi/src/Makefile podules/pccard/src/Makefile podules/ultimatecdrom/src/Makefile])
AC_OUTPUT
