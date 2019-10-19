AC_INIT(ocp, 0.1.22, stian@nixia.no)
AC_DEFINE(DLLVERSION, [0x00000114])

AC_CANONICAL_HOST

AC_MSG_CHECKING([GIT submodules are checked out])
if ! test -f playgmi/timidity-git/Makefile.am; then
  AC_MSG_ERROR([playgmi/timidity-git is missing - please execute 'git submodule update --init --recursive'])
else
  AC_MSG_RESULT([ok])
fi

AC_ARG_WITH(debug, [  --with-debug            compile with debug-flags and without SIGALRM],
if test "x$with_debug" = "xyes"; then
for i in $CFLAGS; do
  new_CFLAGS="$new_CFLAGS `echo $i|grep -- -L`";
  new_CFLAGS="$new_CFLAGS `echo $i|grep -- -I`";
done;
CFLAGS="$new_CFLAGS -g"
for i in $CXXFLAGS; do
  new_CXXFLAGS="$new_CFLAGS `echo $i|grep -- -L`";
  new_CXXFLAGS="$new_CFLAGS `echo $i|grep -- -I`";
done;
CXXFLAGS="$new_CXXFLAGS -g"
AC_DEFINE(KICKSTART_GDB, 1) AC_DEFINE(DISABLE_SIGALRM)
fi
)

AC_HEADER_TIME

AC_PROG_CC
AX_PROG_CC_FOR_BUILD
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_CXXCPP
AC_PROG_MAKE_SET
AC_SUBST(DESKTOP_FILE_INSTALL)

AC_C_BIGENDIAN

AC_SUBST(SHARED_FLAGS)
AC_SUBST(LIB_SUFFIX)
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(HAIKU)
target=`$CC -dumpmachine`
case "$target" in
	*-darwin*)
		SHARED_FLAGS="-dynamiclib -flat_namespace -undefined suppress"
		CFLAGS="$CFLAGS -fno-common"
		CXXFLAGS="$CXXFLAGS -fno-common"
		LIB_SUFFIX=.dylib
		PTHREAD_LIBS=-pthread
		;;
	*-haiku*)
		SHARED_FLAGS=-shared
		LIB_SUFFIX=.so
		PTHREAD_LIBS=
		with_desktop_file_install=no
		HAIKU=1
		;;
	*)
		SHARED_FLAGS=-shared
		LIB_SUFFIX=.so
		PTHREAD_LIBS=-pthread
		;;
esac
AC_DEFINE_UNQUOTED(LIB_SUFFIX, "$LIB_SUFFIX")

AC_SUBST(DIR_SUFFIX)
AC_ARG_WITH(dir_suffix, [  --with-dir-suffix=      append this on all paths. Defaults to -$PACKAGE_VERSION], DIR_SUFFIX=$with_dir_suffix, DIR_SUFFIX=-$PACKAGE_VERSION)
AC_DEFINE_UNQUOTED(DIR_SUFFIX, "$DIR_SUFFIX")

AC_ARG_WITH(x11, [  --with-x11              force/disable x11 support],, with_x11=auto)

AC_ARG_WITH(adplug, [  --with-adplug           force/disable adplug support],, with_adplug=auto)

AC_ARG_WITH(mad, [  --with-mad              force/disable mad mpeg audio support],, with_mad=auto)

AC_ARG_WITH(sidplay, [  --with-sidplay          force/disable sidplay support],, with_sidplay=auto)

AC_ARG_WITH(alsa, [  --with-alsa             force/disable alsa support],, with_alsa=auto)
AC_ARG_WITH(oss,  [  --with-oss              force/disable oss support],, with_oss=auto)

AC_ARG_WITH(lzw, [  --with-lzw              force/disable lzw support (enabled by default)],, with_lzw=yes)

AC_ARG_WITH(lzh, [  --with-lzh              force/disable lzh support in .ym files(enabled by default)],, with_lzh=yes)

AC_ARG_WITH(coreaudio, [  --with-coreaudio        force/disable osx coreaudio support (autodetect by default)],, with_coreaudio=auto)
AC_ARG_WITH(flac, [  --with-flac             force/disable FLAC support (autodetect by default)],, with_flac=auto)

AC_ARG_WITH(sdl,  [  --with-sdl              force/disable SDL video support (autodetect by default)],, with_sdl=auto)
AC_ARG_WITH(sdl2, [  --with-sdl2             force/disable SDL2 video support (autodetect by default)],, with_sdl2=auto)
AC_ARG_WITH(desktop_file_install, [  --without-desktop_file_install Do not install .desktop file (SDL and X11 normally does this)],, with_desktop_file_install=auto)


# normally, this is done automatically first when running AC_OUTPUT, but we need them expanded now
test "x$prefix" = xNONE && prefix=$ac_default_prefix
# Let make expand exec_prefix.
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

AC_DEFUN([AS_AC_EXPAND],
[
  EXP_VAR=[$1]
  FROM_VAR=[$2]
dnl first expand prefix and exec_prefix if necessary
  prefix_save=$prefix
  exec_prefix_save=$exec_prefix
dnl if no prefix given, then use /usr/local, the default prefix
  full_var="$FROM_VAR"
dnl loop until it doesn"t change anymore
  while true; do
    new_full_var="`eval echo $full_var`"
    if test "x$new_full_var" = "x$full_var"; then break; fi
    full_var=$new_full_var
  done
dnl clean up
  full_var=$new_full_var
  AC_SUBST([$1], "$full_var")
dnl restore prefix and exec_prefix
  prefix=$prefix_save
  exec_prefix=$exec_prefix_save
])

AC_MSG_CHECKING([gcc version])
cc_version=`( $CC -dumpversion ) 2>&1`
if test "$?" -gt 0 || test x$cc_version = x; then
	AC_MSG_ERROR([not found])
else
	case $cc_version in
		'')
			AC_MSG_ERROR([not found]);
			;;
		2.95.[[2-9]]|2.95.[[2-9]][[-]].*|3.[[0-9]]|3.[[0-9]].[[0-9]]|3.[[0-9]]|3.[[0-9]].[[0-9]]-*|4.*|5.*|6.*|7*|8*|9*|10*)
			_cc_major=`echo $cc_version | cut -d '.' -f 1`
			_cc_minor=`echo $cc_version | cut -d '.' -f 2`
			_cc_mini=`echo $cc_version | cut -d '.' -f 3`
			if test $_cc_major -gt 2; then
				AC_MSG_RESULT([$cc_version, ok])
			else
				AC_MSG_ERROR([$cc_version, bad])
			fi
			;;
		*)
			AC_MSG_ERROR([$cc_version, bad])
			;;
       esac
fi

AC_MSG_CHECKING([for broken signed char to signed int in loops (gcc version 4.1.1)])
AC_RUN_IFELSE(
[AC_LANG_SOURCE([
#include <stdio.h>
#include <unistd.h>
void failcheck(signed int source, signed int filter)
{
	if ((source>128)&&(filter>0))
		_exit(1);
}

int main(int arg, char *argv@<:@@:>@)
{
	int j;
	for (j=0; j<256; j++)
	{
		signed char j2=(signed char)j;
		signed int j3=j2;
		failcheck(j, j3);
	}
	return 0;
}
])],
	AC_MSG_RESULT([ok]),
	AC_MSG_ERROR([failed.
Try to remove any -O flag or to add -fwrapv to CFLAGS and CXXFLAGS]),
AC_DEFINE(GCC_411_RUNTIMECHECK, 1)
)

AC_MSG_CHECKING([for PIC])
org_cflags="$CFLAGS"
CFLAGS="$CFLAGS -fPIC"
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[]],[[]])],
	[AC_MSG_RESULT(-fPIC)
	CXXFLAGS="$CXXFLAGS -fPIC"]
	,
	[CFLAGS="$org_cflags -fpic"
	AC_LINK_IFELSE(
		[AC_LANG_PROGRAM([[]],[[]])],
		[AC_MSG_RESULT(-fpic)
		CXXFLAGS="$CXXFLAGS -fpic"]
		,
		[CFLAGS="$org_cflags"
		AC_MSG_RESULT(not supported)]
	])
)

AC_CHECK_FUNCS(memmem)
AC_CHECK_FUNCS(getopt)
AC_CHECK_FUNCS(popen)
AC_CHECK_FUNCS(qsort)
AC_CHECK_FUNCS(gettimeofday)
AC_CHECK_FUNCS(sleep)
AC_CHECK_FUNCS(sysconf)
AC_CHECK_FUNCS(strerror)
AC_CHECK_FUNCS(strdup)
AC_CHECK_FUNCS(strstr)
AC_CHECK_FUNCS(strupr)
AC_CHECK_FUNCS(getcwd)
AC_CHECK_FUNCS(snprintf)
AC_CHECK_FUNCS(usleep)
AC_CHECK_FUNCS(vsnprintf)
AC_CHECK_FUNCS(strncasecmp)
AC_CHECK_FUNCS(mkstemp)
AC_CHECK_FUNCS(strlcpy)
AC_CHECK_FUNCS(strlcat)

AC_SUBST(MATH_LIBS)
AC_CHECK_FUNCS(sqrt,
               MATH_LIBS=,
               AC_CHECK_LIB(m,
                            sqrt,
                            MATH_LIBS=-lm,
                            AC_MSG_ERROR("libm not found")
                           )
              )

AC_STRUCT_DIRENT_D_TYPE

AC_CONFIG_HEADERS(config.h)

AC_HEADER_STDC

AC_SUBST(HAVE_DIRENT_H)
AC_CHECK_HEADER(dirent.h,
                HAVE_DIRENT_H=1
                AC_DEFINE(HAVE_DIRENT_H, 1),
               )
AC_SUBST(HAVE_FCNTL_H)
AC_CHECK_HEADER(fcntl.h,
                HAVE_FCNTL_H=1
                AC_DEFINE(HAVE_FCNTL_H, 1),
               )
AC_SUBST(HAVE_GETOPT_H)
AC_CHECK_HEADER(getopt.h,
                HAVE_GETOPT_H=1
                AC_DEFINE(HAVE_GETOPT_H, 1),
               )
AC_SUBST(HAVE_NDIR_H)
AC_CHECK_HEADER(ndir.h,
                HAVE_NDIR_H=1
                AC_DEFINE(HAVE_NDIR_H, 1),
               )
AC_SUBST(HAVE_SYS_NDIR_H)
AC_CHECK_HEADER(sys/ndir.h,
                HAVE_SYS_NDIR_H=1
                AC_DEFINE(HAVE_SYS_NDIR_H, 1),
               )
AC_SUBST(HAVE_SYS_DIR_H)
AC_CHECK_HEADER(sys/dir.h,
                HAVE_SYS_DIR_H=1
                AC_DEFINE(HAVE_SYS_DIR_H, 1),
               )
AC_SUBST(HAVE_STDINT_H)
AC_CHECK_HEADER(stdint.h,
                HAVE_STDINT_H=1
                AC_DEFINE(HAVE_STDINT_H, 1),
               )
AC_SUBST(HAVE_INTTYPES_H)
AC_CHECK_HEADER(inttypes.h,
                HAVE_INTTYPES_H=1
                AC_DEFINE(HAVE_INTTYPES_H, 1),
               )
AC_SUBST(HAVE_BYTESWAP_H)
AC_CHECK_HEADER(byteswap.h,
                HAVE_BYTESWAP_H=1
                AC_DEFINE(HAVE_BYTESWAP_H, 1),
               )
AC_SUBST(HAVE_UNISTD_H)
AC_CHECK_HEADER(unistd.h,
                HAVE_UNISTD_H=1
                AC_DEFINE(HAVE_UNISTD_H, 1),
               )
AC_SUBST(HAVE_LIMITS_H)
AC_CHECK_HEADER(limits.h,
                HAVE_LIMITS_H=1
                AC_DEFINE(HAVE_LIMITS_H, 1),
               )
AC_SUBST(NO_STRING_H)
AC_SUBST(HAVE_STRING_H)
AC_CHECK_HEADER(string.h,
                HAVE_STRING_H=1
                AC_DEFINE(HAVE_STRING_H, 1),
                [ AC_DEFINE(NO_STRING_H,1,Define to 1 if you do not have <string.h>.)]
               )
AC_SUBST(HAVE_STRINGS_H)
AC_CHECK_HEADER(strings.h,
                HAVE_STRINGS_H=1
                AC_DEFINE(HAVE_STRINGS_H, 1),
               )

AC_SUBST(HAVE_ERRNO_H)
AC_CHECK_HEADER(errno.h,
                HAVE_ERRNO_H=1
                AC_DEFINE(HAVE_ERRNO_H, 1),
               )
AC_SUBST(HAVE_SYS_STAT_H)
AC_CHECK_HEADER(sys/stat.h,
                HAVE_SYS_STAT_H=1
                AC_DEFINE(HAVE_SYS_STAT_H, 1),
               )
AC_SUBST(HAVE_SYS_SYSCTL_H)
AC_CHECK_HEADER(sys/sysctl.h,
                HAVE_SYS_SYSCTL_H=1
                AC_DEFINE(HAVE_SYS_SYSCTL_H, 1),
               )
AC_SUBST(HAVE_SYS_TIME_H)
AC_CHECK_HEADER(sys/time.h,
                HAVE_SYS_TIME_H=1
                AC_DEFINE(HAVE_SYS_TIME_H, 1),
               )
AC_SUBST(HAVE_SYS_TYPES_H)
AC_CHECK_HEADER(sys/types.h,
                HAVE_SYS_TYPES_H=1
                AC_DEFINE(HAVE_SYS_TYPES_H, 1),
               )
AC_SUBST(HAVE_SYS_PARAM_H)
AC_CHECK_HEADER(sys/param.h,
                HAVE_SYS_PARAM_H=1
                AC_DEFINE(HAVE_SYS_PARAM_H, 1),
               )
AC_SUBST(HAVE_MACHINE_ENDIAN_H)
AC_CHECK_HEADER(machine/endian.h,
                HAVE_MACHINE_ENDIAN_H=1
                AC_DEFINE(HAVE_MACHINE_ENDIAN_H, 1),
               )

dnl timidity patch dir
AC_ARG_WITH(timidity-default-path,
  AS_HELP_STRING([--with-timidity-default-path=DIR], [Where timidity.cfg is  (PREFIX/share/timidity)]),
  [timiditypkgdatadir=$withval],
  [
   if test "x$prefix" != "xNONE"; then
      timiditypkgdatadir='${prefix}/share/timidity'
   else
      timiditypkgdatadir='/usr/local/share/timidity'
   fi
  ])
tmpdefpath="`eval \"echo ${timiditypkgdatadir}\"`"
AC_DEFINE_UNQUOTED(TIMIDITY_DEFAULT_PATH,"$tmpdefpath",place to install patches)
AC_DEFINE(TIMIDITYPKGDATADIR,TIMIDITY_DEFAULT_PATH,a compatibility matter. ignore it.)

dnl We first check for ncursesw, if that fails we revert back to ncurses
AX_WITH_CURSES
if test "x$ax_cv_ncursesw" != xyes && test "x$ax_cv_ncurses" != xyes; then
	AC_MSG_ERROR([requires either NcursesW or Ncurses library])
fi
if test "x$ax_cv_curses_color" != xyes; then
	AC_MSG_ERROR([requires an X/Open-compatible Curses library with color])
fi
push_LIBS=$LIBS
LIBS=$CURSES_LIB
AC_CHECK_FUNCS(resize_term)
LIBS=$push_LIBS

AC_CHECK_LIB(z, zlibVersion, , AC_MSG_ERROR("zlib not found"))
AC_SUBST(DL_LIBS)
dnl -lc is dirty hack here
push_LIBS=$LIBS
LIBS=
dnl refresh LIB list
AC_LANG_PUSH(C)
AC_CHECK_FUNC(dlopen, DL_LIBS="",AC_CHECK_LIB(dl, dlopen, DL_LIBS="-ldl", AC_MSG_ERROR("dlopen not found")))
LIBS=$push_LIBS
AC_LANG_POP

AC_SUBST(MAD_CFLAGS)
AC_SUBST(MAD_LIBS)
if test "x$with_mad" != "xno"; then
	AC_MSG_CHECKING([mad support])
	MAD_LIBS=`pkg-config --libs mad 2> /dev/null`
	MAD_CFLAGS=`pkg-config --cflags mad 2> /dev/null`
	if test "$?" = 0; then
		AC_MSG_RESULT("$MAD_CFLAGS $MAD_LIBS")
	else
		AC_MSG_RESULT([pkg-config failed]);
	dnl Fall back to non-pkg-config method
		AC_CHECK_LIB(mad, mad_stream_init, , if test "x$with_mad" = "xyes"; then AC_MSG_ERROR("libmad not found"); else with_mad="no"; fi)
		if test "x$with_mad" != "xno"; then
			AC_CHECK_HEADER(mad.h, , if test "x$with_mad" = "xyes"; then AC_MSG_ERROR("libmad header files was not found"); else with_mad="no"; fi)
		fi
		if test "x$with_mad" != "xno"; then
			MAD_LIBS="-lmad"
			MAD_CFLAGS=""
			LIBS=$push_LIBS
		fi
	fi
fi
AC_SUBST(HAVE_MAD)
if test "x$with_mad" = "xno"; then
	HAVE_MAD=0
else
	HAVE_MAD=1
	AC_DEFINE(HAVE_MAD)
fi

AC_SUBST(OGG_CFLAGS)
AC_SUBST(OGG_LIBS)
AC_MSG_CHECKING([ogg support])
OGG_LIBS=`pkg-config --libs ogg 2> /dev/null`
OGG_CFLAGS=`pkg-config --cflags ogg 2> /dev/null`
if test "$?" = 0; then
	AC_MSG_RESULT("$OGG_CFLAGS $OGG_LIBS")
else
	AC_MSG_RESULT([pkg-config failed]);
dnl Fall back to non-pkg-config method
	AC_CHECK_LIB(ogg, ogg_sync_init, , AC_MSG_ERROR("ogg libraries not found"))
	OGG_LIBS="-logg"
	OGG_CFLAGS=""
	LIBS=$push_LIBS
fi

AC_SUBST(VORBIS_CFLAGS)
AC_SUBST(VORBIS_LIBS)
AC_MSG_CHECKING([vorbis support])
VORBIS_LIBS=`pkg-config --libs vorbis 2> /dev/null`
VORBIS_CFLAGS=`pkg-config --cflags vorbis 2> /dev/null`
if test "$?" = 0; then
	AC_MSG_RESULT("$VORBIS_CFLAGS $VORBIS_LIBS")
else
	AC_MSG_RESULT([pkg-config failed]);
dnl Fall back to non-pkg-config method
	AC_CHECK_LIB(vorbis, vorbis_bitrate_init, , AC_MSG_ERROR("vorbis libraries not found"), -logg)
	AC_CHECK_HEADER(vorbis/codec.h, , AC_MSG_ERROR("libvorbis header files was not found"))
	VORBIS_LIBS="-lvorbis"
	VORBIS_CFLAGS=""
	LIBS=$push_LIBS
fi

AC_SUBST(VORBISFILE_CFLAGS)
AC_SUBST(VORBISFILE_LIBS)
AC_MSG_CHECKING([vorbisfile support])
VORBISFILE_LIBS=`pkg-config --libs vorbisfile 2> /dev/null`
VORBISFILE_CFLAGS=`pkg-config --cflags vorbisfile 2> /dev/null`
if test "$?" = 0; then
	AC_MSG_RESULT("$VORBISFILE_CFLAGS $VORBISFILE_LIBS")
else
	AC_MSG_RESULT([pkg-config failed]);
dnl Fall back to non-pkg-config method

	AC_CHECK_LIB(vorbisfile, ov_pcm_seek_lap, AC_DEFINE(HAVE_OV_PCM_SEEK_LAP, 1),
		AC_CHECK_LIB(vorbisfile, ov_pcm_seek, AC_DEFINE(HAVE_OV_PCM_SEEK, 1), AC_MSG_ERROR("vorbis libraries not found"), -logg -lvorbis)
	)
	AC_CHECK_HEADER(vorbis/vorbisfile.h,, AC_MSG_ERROR("libvorbis header files were not found"))
	VORBISFILE_LIBS="-lvorbisfile"
	VORBISFILE_CFLAGS=""
	LIBS=$push_LIBS
fi

AC_SUBST(FLAC_CFLAGS)
AC_SUBST(FLAC_LIBS)
if test "x$with_flac" != "xno"; then
	AC_CHECK_LIB(FLAC, FLAC__seekable_stream_decoder_process_until_end_of_metadata, FLAC_LIBS="-lFLAC",
		AC_CHECK_LIB(FLAC, FLAC__stream_decoder_process_until_end_of_metadata, FLAC_LIBS="-lFLAC",
			if test "x$with_flac" = "xyes"; then AC_MSG_ERROR("(lib)flac not found"); else with_flac="no"; fi,
		-lz),
	-lz)
	LIBS=$push_LIBS
fi
if test "x$with_flac" != "xno"; then
	AC_CHECK_HEADER(FLAC/all.h, FLAC_CFLAGS="", if test "x$with_flac" = "xyes"; then AC_MSG_ERROR("(lib)flac header files were not found"); else with_flac="no"; fi)
fi
AC_SUBST(HAVE_FLAC)
AC_SUBST(FLAC_INI)
if test "x$with_flac" = "xno"; then
	HAVE_FLAC=
	FLAC_INI=
else
	HAVE_FLAC=1
	FLAC_INI=flacptype
fi

AC_CHECK_HEADER(zlib.h, , AC_MSG_ERROR("zlib header files was not found"))

if test "x$with_x11" != "xno"; then
	AC_CHECK_HEADER(X11/extensions/xf86vmode.h, , if test "x$with_x11" = "xyes"; then AC_MSG_ERROR("xvidmode header files was not found"); else with_x11="no"; fi,
[#include <X11/Xlib.h>
])
fi
if test "x$with_x11" != "xno"; then
	AC_CHECK_HEADER(X11/extensions/XShm.h, , if test "x$with_x11" = "xyes"; then AC_MSG_ERROR("XShm header files was not found"); else with_x11="no"; fi,
[#include <X11/Xlib.h>
])
fi
if test "x$with_x11" != "xno"; then
	AC_CHECK_HEADER(X11/xpm.h, , if test "x$with_x11" = "xyes"; then AC_MSG_ERROR("xpm header files was not found"); else with_x11="no"; fi,
[#include <X11/Xlib.h>
])
fi
if test "x$with_x11" != "xno"; then
	AC_CHECK_LIB(Xpm, XpmCreatePixmapFromData, X11_LIBS="-lXpm -lX11 -L/usr/X11R6/lib", if test "x$with_x11" = "xyes"; then AC_MSG_ERROR("Xpm/X11 not found (expected to be found in /usr/X11R6/lib)"); else with_x11="no"; fi, -lXpm -lX11 -L/usr/X11R6/lib)
	LIBS=$push_LIBS
fi
if test "x$with_x11" != "xno"; then
	AC_CHECK_LIB(Xext, XShmQueryExtension, X11_LIBS="-lXext $X11_LIBS", if test "x$with_x11" = "xyes"; then AC_MSG_ERROR("SHM/X11 not found (expected to be found in /usr/X11R6/lib)"); else with_x11="no"; fi, -lX11 -lXext -L/usr/X11R6/lib)
	LIBS=$push_LIBS
fi
if test "x$with_x11" != "xno"; then
	AC_CHECK_LIB(Xxf86vm, XF86VidModeQueryExtension, X11_LIBS="-lXxf86vm $X11_LIBS", if test "x$with_x11" = "xyes"; then AC_MSG_ERROR("xvidmode was not found (expected to be found in /usr/X11R6/lib)"); else with_x11="no"; fi, -lX11 -lXext -L/usr/X11R6/lib)
	LIBS=$push_LIBS
fi
AC_SUBST(HAVE_X11)
if test "x$with_x11" = "xno"; then
	X11_LIBS=
	HAVE_X11=
else
	HAVE_X11=1
	AC_DEFINE(HAVE_X11)
fi

if test "x$with_sdl" = "xyes" && test "x$with_sdl2" = "xyes"; then
	AC_MSG_ERROR("Can not use both --with-sdl and --with-sdl2")
fi

if test "x$with_sdl" = "xyes"; then
	with_sdl2="no"
fi

if test "x$with_sdl2" = "xyes"; then
	with_sdl="no"
fi

AC_SUBST(SDL2_CFLAGS)
AC_SUBST(SDL2_LIBS)
if test "x$with_sdl2" != "xno"; then
	AM_PATH_SDL2(["2.0.0"], with_sdl2=yes;, if test "x$with_sdl2" = "xyes"; then AC_MSG_ERROR("libSDL2 was not found"); else with_sdl2="no"; fi)
fi
SDL2_CFLAGS=$SDL_CFLAGS
SDL2_LIBS=$SDL_LIBS
SDL_CFLAGS=
SDL_LIBS=
AC_SUBST(HAVE_SDL2)
if test "x$with_sdl2" = "xno"; then
	HAVE_SDL2=
else
	HAVE_SDL2=1
	AC_DEFINE(HAVE_SDL2)
	with_sdl="no"
fi

org_cflags="$CFLAGS"
org_cppflags="$CPPFLAGS"
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)
if test "x$with_sdl" != "xno"; then
	AC_MSG_CHECKING([SDL support])
	SDL_LIBS=`pkg-config --libs sdl 2> /dev/null`
	SDL_CFLAGS=`pkg-config --cflags sdl 2> /dev/null`
	if test "$?" = 0; then
		AC_MSG_RESULT("$SDL_CFLAGS $SDL_LIBS")
	else
		AC_MSG_RESULT([pkg-config failed])

		AC_CHECK_LIB(SDL, SDL_Init, SDL_LIBS="-lSDL", if test "x$with_sdl" = "xyes"; then AC_MSG_ERROR("libSDL was not found"); else with_sdl="no"; fi)

		if test "x$with_sdl" != "xno"; then
			CFLAGS="$CFLAGS -I/usr/include/SDL"
			CPPFLAGS="$CPPFLAGS -I/usr/include/SDL"
			AC_CHECK_HEADER(SDL.h, SDL_CFLAGS="-I/usr/include/SDL", if test "x$with_sdl" = "xyes"; then AC_MSG_ERROR("SDL header files not found"); else with_sdl="no"; fi)
		fi
	fi
fi
AC_SUBST(HAVE_SDL)
if test "x$with_sdl" = "xno"; then
	HAVE_SDL=
else
	HAVE_SDL=1
	AC_DEFINE(HAVE_SDL)
fi
CFLAGS="$org_cflags"
CPPFLAGS="$org_cflags"

AC_LANG_PUSH(C++)
org_cxxflags="$CXXFLAGS"
org_cppflags="$CPPFLAGS"
AC_SUBST(ADPLUG_CXXFLAGS)
AC_SUBST(ADPLUG_LIBS)
if test "x$with_adplug" != "xno"; then
	AC_MSG_CHECKING([Adplug support])
	ADPLUG_LIBS=`pkg-config --libs adplug 2> /dev/null`
	ADPLUG_CXXFLAGS=`pkg-config --cflags adplug 2> /dev/null`
	if test "$?" = 0; then
		AC_MSG_RESULT("$ADPLUG_CXXFLAGS $ADPLUG_LIBS")
	else
		AC_MSG_RESULT([pkg-config failed]);
dnl Fall back to non-pkg-config method
		AC_CHECK_HEADER(adplug/adplug.h, ,with_adplug_failed=yes)
		if test "x$with_adplug_failed" = "xyes"; then
			CXXFLAGS="$CXXFLAGS -I/usr/include/libbinio"
			CPPFLAGS="$CPPFLAGS -I/usr/include/libbinio"
			unset ac_cv_header_adplug_adplug_h
			AC_MSG_NOTICE("Trying again with -I/usr/include/libbinio")
			AC_CHECK_HEADER(adplug/adplug.h,
				ADPLUG_CXXFLAGS="-I/usr/include/libbinio",
				if test "x$with_adplug" = "xyes"; then AC_MSG_ERROR("Adplug header files was not found"); else with_adplug="no"; fi
			)
		fi
		if test "x$with_adplug" != "xno"; then
			AC_CHECK_LIB(adplug, docell0, ADPLUG_LIBS="-ladplug", if test "x$with_adplug" = "xyes"; then AC_MSG_ERROR("Adplug was not found"); else with_adplug="no"; fi)
			LIBS=$push_LIBS
		fi
	fi
fi
CXXFLAGS="$org_cxxflags"
CPPFLAGS="$org_cppflags"

if test "x$with_adplug" != "xno"; then
AC_MSG_CHECKING([If AdPlug was compiled with another C++ ABI])
	CXXFLAGS="$CXXFLAGS $ADPLUG_CXXFLAGS"
	LIBS="$LIBS $ADPLUG_LIBS"

	AC_LINK_IFELSE(
		[AC_LANG_PROGRAM([[#include <adplug/emuopl.h>]],[[CEmuopl opl = CEmuopl(48000, 1, 1);]])],
		[],
		[AC_MSG_ERROR("Adplug was compiled with a different c++ compiler")]
	)

	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([[
#include <adplug/adplug.h>
#include <adplug/players.h>
#include <adplug/player.h>
                                 ]],[[
	int j;
	CPlayers::const_iterator i;

	for (i = CAdPlug::players.begin(); i != CAdPlug::players.end(); i++)
	{
		for (j = 0; (*i)->get_extension(j); j++)
		{
			if (CFileProvider::extension("OPL", (*i)->get_extension(j)))
			{
				return 0;
			}
		}
	}
	return 0;
                                 ]])],
		[AC_MSG_RESULT([ok])],
		[AC_MSG_ERROR("Adplug was compiled with a different c++ compiler")]
	)
	LIBS=$push_LIBS
	CXXFLAGS="$org_cxxflags"
	CPPFLAGS="$org_cppflags"
fi

if test "x$with_adplug" != "xno"; then
AC_MSG_CHECKING([If libbinio used by AdPlug contains bugs https://github.com/adplug/libbinio/issues/2])
	CXXFLAGS="$CXXFLAGS $ADPLUG_CXXFLAGS"
	LIBS="$LIBS $ADPLUG_LIBS"

	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([[
#include <binio.h>
#include <binstr.h>
#include <cstdlib>
#include <string.h>
#include <adplug/adplug.h>
#include <adplug/fprovide.h>
#include <string.h>
                                 ]],[[
	binisstream *f = new binisstream((void *)"foobar", 6);
	long size;
	char test[6] = {1,2,3,4,5,6};

	f->seek(0, binio::End);
	size = f->pos();
	f->seek(0);

	if (f->readString(test, size) != 6) return -1;

	/* test should contain "foobar" if all is correct */
	return memcmp(test, "foobar", 6);
                                 ]])],
		[AC_MSG_RESULT([ok])],
		[AC_MSG_ERROR("libbinio fails to use memoryobjects")]
	)
	LIBS=$push_LIBS
	CXXFLAGS="$org_cxxflags"
	CPPFLAGS="$org_cppflags"
fi

if test "x$with_sidplay" != "xno"; then
	AC_CHECK_LIB(sidplay, sidEmuFillFunc, , if test "x$with_sidplay" = "xyes"; then AC_MSG_ERROR("sidplay (1.x) libraries not found"); else with_sidplay="no"; fi)
	LIBS=$push_LIBS
fi
if test "x$with_sidplay" != "xno"; then
	AC_CHECK_HEADER(sidplay/version.h, , if test "x$with_sidplay" = "xyes"; then AC_MSG_ERROR("sidplay (1.x) header files was not found"); else with_sidplay="no"; fi)
fi
AC_LANG_POP()

AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
if test "x$with_alsa" != "xno"; then
	AC_MSG_CHECKING([Alsa support])
	ALSA_LIBS=`pkg-config --libs alsa 2> /dev/null`
	ALSA_CFLAGS=`pkg-config --cflags alsa 2> /dev/null`
	if test "$?" = 0; then
		AC_MSG_RESULT("$ALSA_CFLAGS $ALSA_LIBS")
	else
		if test "x$with_alsa" = "xyes"; then
			AC_MSG_ERROR([pkg-config failed]);
		else
			AC_MSG_RESULT([pkg-config failed]);
			with_alsa="no"
		fi
	fi
fi

AC_SUBST(HAVE_ALSA)
if test "x$with_alsa" = "xno"; then
	HAVE_ALSA=0
else
	HAVE_ALSA=1
fi

AC_SUBST(HAVE_ADPLUG)
AC_SUBST(ADPLUG_INI)
if test "x$with_adplug" = "xno"; then
	HAVE_ADPLUG=
	ADPLUG_INI=
else
	HAVE_ADPLUG=1
	ADPLUG_INI=opltype
fi
AC_SUBST(HAVE_SIDPLAY)
AC_SUBST(SIDPLAY_INI)
if test "x$with_sidplay" = "xno"; then
	HAVE_SIDPLAY=0
	SIDPLAY_INI=
else
	HAVE_SIDPLAY=1
	SIDPLAY_INI=sidtype
fi

AC_SUBST(LINUX)
AC_SUBST(CDROM_SUPPORT)
AC_SUBST(X11_LIBS)
AC_SUBST(CDROM_INI)
AC_SUBST(HAVE_FRAMEBUFFER)
HAVE_FRAMEBUFFER=1
LINUX=1
	AC_CHECK_HEADER(linux/limits.h, , AC_MSG_RESULT([linux kernel header files was not found]);LINUX=)
if test "$LINUX+set" != "+set"; then
	AC_CHECK_HEADER(linux/major.h, , AC_MSG_RESULT([linux kernel header files was not found]);LINUX=)
fi
if test "$LINUX+set" != "+set"; then
	AC_CHECK_HEADER(linux/fb.h, , AC_MSG_RESULT([linux kernel header files was not found]);LINUX=;HAVE_FRAMEBUFFER=)
fi
if test "$LINUX+set" != "+set"; then
	AC_CHECK_HEADER(linux/cdrom.h, CDROM_SUPPORT=1;CDROM_INI="cdrom", AC_MSG_RESULT([linux cdrom kernel header files was not found]))
fi

AC_SUBST(HAVE_OSS)
AC_SUBST(OSS_LIBS)
AC_SUBST(OSS_CFLAGS)
if test "x$with_oss" != "xno"; then
	AC_MSG_CHECKING([liboss])
	OSS_LIBS=`pkg-config --libs liboss 2> /dev/null`
	OSS_CFLAGS=`pkg-config --cflags liboss 2> /dev/null`
	if test "$?" = 0; then
		AC_MSG_RESULT("$OSS_CFLAGS $OSS_LIBS")
		HAVE_OSS=1
	else
		AC_MSG_RESULT([pkg-config failed]);
dnl Fall back to non-pkg-config method
		AC_CHECK_HEADER(sys/soundcard.h,
			HAVE_OSS=1
			AC_DEFINE(HAVE_SYS_SOUNDCARD_H),
			if test "x$with_oss" = "xyes"; then AC_MSG_ERROR([kernel OSS header file was not found], 1); else with_oss="no"; HAVE_OSS=0;fi)
	fi
fi

AC_SUBST(HAVE_COREAUDIO)
AC_SUBST(COREAUDIO_CFLAGS)
AC_SUBST(COREAUDIO_LIBS)
if test "x$with_coreaudio" != "xno"; then
	AC_CHECK_HEADER(CoreServices/CoreServices.h, HAVE_COREAUDIO=1, if test "x$with_coreaudio" = "xyes"; then AC_MSG_ERROR([CoreAudio not found], 1); else with_coreaudio="no"; HAVE_COREAUDIO=0; fi)
fi
if test "x$with_coreaudio" != "xno"; then
	AC_CHECK_HEADER(AudioUnit/AudioUnit.h, HAVE_COREAUDIO=1, if test "x$with_coreaudio" = "xyes"; then AC_MSG_ERROR([CoreAudio not found], 1); else with_coreaudio="no"; HAVE_COREAUDIO=0; fi)
fi
if test "x$with_coreaudio" != "xno"; then
	AC_CHECK_HEADER(AudioToolbox/AudioToolbox.h, HAVE_COREAUDIO=1, if test "x$with_coreaudio" = "xyes"; then AC_MSG_ERROR([CoreAudio not found], 1); else with_coreaudio="no"; HAVE_COREAUDIO=0; fi)
fi
if test "x$with_coreaudio" != "xno"; then
	AC_CHECK_HEADER(CoreAudio/CoreAudio.h, HAVE_COREAUDIO=1, if test "x$with_coreaudio" = "xyes"; then AC_MSG_ERROR([CoreAudio not found], 1); else with_coreaudio="no"; HAVE_COREAUDIO=0; fi)
fi
if test "x$with_coreaudio" != "xno"; then
	COREAUDIO_CFLAGS=""
	COREAUDIO_LIBS="-framework CoreAudio -framework Carbon -framework AudioUnit"
fi

AS_AC_EXPAND(LIBDIR, $libdir/$PACKAGE_NAME$DIR_SUFFIX)
AC_DEFINE_UNQUOTED(LIBDIR, "$LIBDIR")

AS_AC_EXPAND(BINDIR, $exec_prefix/bin)

AS_AC_EXPAND(PREFIX, $prefix)
AS_AC_EXPAND(DOCDIR, $docdir)
AS_AC_EXPAND(INFODIR, $infodir)
AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)
AS_AC_EXPAND(DATADIR, $datadir)
AS_AC_EXPAND(DATAROOTDIR, $datarootdir)
AC_DEFINE_UNQUOTED(DATADIR, "$DATADIR")
AC_DEFINE_UNQUOTED(PREFIX, "$PREFIX")

AC_ARG_WITH(efence, [  --with-efence           Enable usage of efence],
EFENCE_LIBS="-lefence"
AC_DEFINE(HAVE_EFENCE, 1)
)
AC_SUBST(EFENCE_LIBS)

AC_ARG_WITH(duma, [  --with-duma             Enable usage of duma],
DUMA_LIBS="-lduma"
AC_DEFINE(HAVE_DUMA, 1)
)
AC_SUBST(DUMA_LIBS)


AC_SUBST(HAVE_MAKEINFO)
AC_CHECK_PROG(HAVE_MAKEINFO, makeinfo, yes,)

AC_SUBST(HAVE_LZW)
AC_MSG_CHECKING([Include lzw support (used by GIF and arcZIP)])
if test "x$with_lzw" = "xyes"; then
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_LZW, 1)
	HAVE_LZW=1
else
	AC_MSG_RESULT([no])
fi

AC_SUBST(HAVE_LZH)
AC_MSG_CHECKING([Include lzh support (used by .ym file support)])
if test "x$with_lzh" = "xyes"; then
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_LZH, 1)
	HAVE_LZH=1
else
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING("cp -f implies --remove-destination")
cp -f --remove-destination Rules.make.in Rules.make.in.tmp > /dev/null 2>&1
if test "$?" -ne 0; then
	AC_MSG_RESULT([yes])
	CP="cp -f"
else
	AC_MSG_RESULT([no])
	CP="cp -f --remove-destination"
fi
rm -f Rules.make.in.tmp
AC_SUBST(CP)

AC_ARG_WITH(libiconv,
	[AS_HELP_STRING([--with-libiconv=], [@<:@auto/no/gnu/native@:>@ what kind of libiconv library to use])],
	[with_libiconv=$withval],
	[with_libiconv=auto])

found_iconv=no
case $with_libiconv in
	auto)
		# Check in the C library first
		AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
		# Check if we have GNU libiconv
		if test $found_iconv = "no"; then
			AC_CHECK_LIB(iconv, libiconv_open, [ICONV_LIBS=-liconv; with_libiconv=gnu; found_iconv=yes])
		fi
		# Check if we have a iconv in -liconv, possibly from vendor
		if test $found_iconv = "no"; then
			AC_CHECK_LIB(iconv, iconv_open, [ICONV_LIBS=-liconv; with_libiconv=native; found_iconv=yes])
		fi
		;;
	no)
		AC_CHECK_FUNC(iconv_open, [with_libiconv=no; found_iconv=yes])
		;;
	gnu|yes)
		AC_CHECK_LIB(iconv, libiconv_open, [ICONV_LIBS=-liconv; with_libiconv=gnu; found_iconv=yes])
		;;
	native)
		AC_CHECK_LIB(iconv, iconv_open, [ICONV_LIBS=-liconv; with_libiconv=native; found_iconv=yes])
		;;
esac
LIBS=$push_LIBS

if test "x$found_iconv" = "xno" ; then
	AC_MSG_ERROR([*** No iconv() implementation found in C library or libiconv])
fi
AC_SUBST(ICONV_LIBS)

case $with_libiconv in
	gnu)
		AC_DEFINE(USE_LIBICONV_GNU, 1, [Using GNU libiconv])
		;;
	native)
		AC_DEFINE(USE_LIBICONV_NATIVE, 1, [Using a native implementation of iconv in a separate library])
		;;
esac

if ! test "x$with_desktop_file_install" = "xno"; then
	if test "x$with_x11" != "xno" || test "x$with_sdl" != "xno" || test "x$with_sdl2" != "xno"; then
		AC_CHECK_PROG(DESKTOP_FILE_INSTALL,desktop-file-install,desktop-file-install,failed)
		if test "x$DESKTOP_FILE_INSTALL" = "xfailed"; then
			AC_MSG_ERROR(Cannot find desktop-file-install (required by x11 and SDL driver).)
		fi
	fi
fi

AC_CONFIG_FILES(Rules.make)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(goodies/helpc/Makefile)
AC_CONFIG_FILES(goodies/pack/Makefile)
AC_CONFIG_FILES(ocp.ini)
AC_CONFIG_FILES(desktop/opencubicplayer.desktop)

AC_CONFIG_FILES(doc/texi/faq.texi)
AC_CONFIG_FILES(doc/texi/install.texi)
AC_CONFIG_FILES(doc/texi/ocp.texi)

AC_OUTPUT

echo
if test "x$with_debug" = "xyes"; then
	echo "debug:             ON"
else
	echo "debug:             OFF"
fi

if test "x$with_adplug" = "xno"; then
	echo "adplug:            OFF"
else
	echo "adplug:            ON"
fi
if test "x$with_mad" = "xno"; then
	echo "mad:               OFF"
else
	echo "mad:               ON"
fi
if test "x$with_sidplay" = "xno"; then
	echo "sidplay (1.x):     OFF"
else
	echo "sidplay (1.x):     ON"
fi
if test "x$with_flac" = "xno"; then
	echo "FLAC:              OFF"
else
	echo "FLAC:              ON"
fi
if test "x$with_x11" = "xno"; then
	echo "x11:               OFF"
else
	echo "x11:               ON"
fi
if test "x$with_sdl" = "xno"; then
	echo "SDL:               OFF"
else
	echo "SDL:               ON"
fi
if test "x$with_sdl2" = "xno"; then
	echo "SDL2:              OFF"
else
	echo "SDL2:              ON"
fi
if test "x$with_alsa" = "xno"; then
	echo "ALSA:              OFF"
else
	echo "ALSA:              ON"
fi
if test "x$with_oss" = "xno"; then
	echo "OSS:               OFF"
else
	echo "OSS:               ON"
fi
if test "x$with_coreaudio" = "xno"; then
	echo "CoreAudio:         OFF"
else
	echo "CoreAudio:         ON"
fi
if test "$LINUX+set" != "+set"; then
	echo "linux cdrom:       ON"
	echo "linux framebuffer: ON"
else
	echo "linux cdrom:       OFF"
	echo "linux framebuffer: OFF"
fi
if test "x$with_lzw" = "xno"; then
	echo "include LZW:       OFF"
else
	echo "include LZW:       ON"
fi
if test "x$with_lzh" = "xno"; then
	echo "include LZH:       OFF"
else
	echo "include LZH:       ON"
fi
