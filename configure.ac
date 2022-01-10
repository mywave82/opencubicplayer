AC_INIT(ocp, 0.2.91, stian.skjelstad@gmail.com)
AC_DEFINE(DLLVERSION, [0x0000025b])

AC_CANONICAL_HOST


AC_MSG_CHECKING([GIT submodules are checked out])
AS_IF([test -f playtimidity/timidity-git/Makefile.am], [AC_MSG_RESULT([ok])],
      [AC_MSG_ERROR([playtimidity/timidity-git is missing - please execute 'git submodule update --init --recursive'])]
)

AC_ARG_WITH([debug], [AS_HELP_STRING([--with-debug],[compile with debug-flags and without SIGALRM])])
AS_IF([test "x$with_debug" = "xyes"], [
  for i in $CFLAGS; do
    new_CFLAGS="$new_CFLAGS `echo $i|grep -- -L`";
    new_CFLAGS="$new_CFLAGS `echo $i|grep -- -I`";
  done;
  CFLAGS="$new_CFLAGS -g"
  for i in $CXXFLAGS; do
    new_CXXFLAGS="$new_CFLAGS `echo $i|grep -- -L`";
    new_CXXFLAGS="$new_CFLAGS `echo $i|grep -- -I`";
  done;
  CXXFLAGS="$new_CXXFLAGS -g -DDEBUG=1"
  AC_DEFINE(KICKSTART_GDB, 1)
  AC_DEFINE(DISABLE_SIGALRM)
])

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
AS_CASE([$target],
	[*-darwin*],[
		SHARED_FLAGS="-dynamiclib -flat_namespace -undefined suppress"
		CFLAGS="$CFLAGS -fno-common"
		CXXFLAGS="$CXXFLAGS -fno-common"
		LIB_SUFFIX=.dylib
		PTHREAD_LIBS=-pthread
	],
	[-haiku*],[
		SHARED_FLAGS=-shared
		LIB_SUFFIX=.so
		PTHREAD_LIBS=
		with_desktop_file_install=no
		HAIKU=1
	],[
		SHARED_FLAGS=-shared
		LIB_SUFFIX=.so
		PTHREAD_LIBS=-pthread
	]
)
AC_DEFINE_UNQUOTED(LIB_SUFFIX, "$LIB_SUFFIX")

AC_SUBST(DIR_SUFFIX)
AC_ARG_WITH([dir_suffix], [AS_HELP_STRING([--with-dir-suffix=],[append this on all paths. Defaults to -$PACKAGE_VERSION])], [DIR_SUFFIX=$with_dir_suffix], [DIR_SUFFIX=-$PACKAGE_VERSION])
AC_DEFINE_UNQUOTED(DIR_SUFFIX, "$DIR_SUFFIX")

AC_ARG_WITH([x11],                  [AS_HELP_STRING([--with-x11],                    [force/disable x11 support])],                                               [], [with_x11=auto])
AC_ARG_WITH([adplug],               [AS_HELP_STRING([--with-adplug],                 [force/disable adplug support])],                                            [], [with_adplug=auto])
AC_ARG_WITH([mad],                  [AS_HELP_STRING([--with-mad],                    [force/disable mad mpeg audio support])],                                    [], [with_mad=auto])
AC_ARG_WITH([alsa],                 [AS_HELP_STRING([--with-alsa],                   [force/disable alsa support])],                                              [], [with_alsa=auto])
AC_ARG_WITH([oss],                  [AS_HELP_STRING([--with-oss],                    [force/disable oss support])],                                               [], [with_oss=auto])
AC_ARG_WITH([lzw],                  [AS_HELP_STRING([--with-lzw],                    [force/disable lzw support (enabled by default)])],                          [], [with_lzw=yes])
AC_ARG_WITH([lzh],                  [AS_HELP_STRING([--with-lzh],                    [force/disable lzh support in .ym files(enabled by default)])],              [], [with_lzh=yes])
AC_ARG_WITH([coreaudio],            [AS_HELP_STRING([--with-coreaudio],              [force/disable osx coreaudio support (autodetect by default)])],             [], [with_coreaudio=auto])
AC_ARG_WITH([flac],                 [AS_HELP_STRING([--with-flac],                   [force/disable FLAC support (autodetect by default)])],                      [], [with_flac=auto])
AC_ARG_WITH([sdl],                  [AS_HELP_STRING([--with-sdl],                    [force/disable SDL video support (autodetect by default)])],                 [], [with_sdl=auto])
AC_ARG_WITH([sdl2],                 [AS_HELP_STRING([--with-sdl2],                   [force/disable SDL2 video support (autodetect by default)])],                [], [with_sdl2=auto])
AC_ARG_WITH([desktop_file_install], [AS_HELP_STRING([--without-desktop_file_install],[Do not install .desktop file (SDL and X11 normally does this)])],           [], [with_desktop_file_install=auto])
AC_ARG_WITH([dumptools],            [AS_HELP_STRING([--with-dumptools],              [enable building the different fileformat dump utilities])],                 [], [with_dumptools=no])
AC_ARG_WITH([unifontdir],           [AS_HELP_STRING([--with-unifontdir=/path],       [the path to where ttf-unifont in located (used by X11, SDL1.x and SDL2)])], [], [with_unifontdir="${datarootdir}/fonts/truetype/unifont"])

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
    AS_IF([test "x$new_full_var" = "x$full_var"],[break])
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
AS_IF([test "$?" -gt 0 || test x$cc_version = x], [AC_MSG_ERROR([not found])], [
	AS_CASE([$cc_version],
		[''],                              [AC_MSG_ERROR([not found])],
# old GCC versions in the 2.95.xx family are known to produce bad crashing binaries (it is still out there on some old systems. In general the all versions before 3.0 is risky
		[2.95.[[2-9]]|2.95.[[2-9]][[-]].*],[AC_MSG_ERROR([$cc_version, bad])],
		                                   [AC_MSG_RESULT([$cc_version, ok])]
	)
])

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

AC_CHECK_FUNCS([memmem])
AC_CHECK_FUNCS([getopt])
AC_CHECK_FUNCS([popen])
AC_CHECK_FUNCS([qsort])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([sleep])
AC_CHECK_FUNCS([sysconf])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strupr])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([snprintf])
AC_CHECK_FUNCS([usleep])
AC_CHECK_FUNCS([vsnprintf])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_FUNCS([mkstemp])
AC_CHECK_FUNCS([strlcpy])
AC_CHECK_FUNCS([strlcat])

AC_SUBST(MATH_LIBS)
AC_CHECK_FUNCS([sqrt], [MATH_LIBS=], [
	AC_CHECK_LIB([m], [sqrt], [MATH_LIBS=-lm], [AC_MSG_ERROR("libm not found")])
])

AC_STRUCT_DIRENT_D_TYPE

AC_CONFIG_HEADERS([config.h] [playsid/sidplayfp-config/config.h])

AC_HEADER_STDC

AC_CHECK_HEADER([dirent.h],        [AC_DEFINE(HAVE_DIRENT_H,          1)], [])
AC_CHECK_HEADER([fcntl.h],         [AC_DEFINE(HAVE_FCNTL_H,           1)], [])
AC_CHECK_HEADER([getopt.h],        [AC_DEFINE(HAVE_GETOPT_H,          1)], [])
AC_CHECK_HEADER([ndir.h],          [AC_DEFINE(HAVE_NDIR_H,            1)], [])
AC_CHECK_HEADER([sys/ndir.h],      [AC_DEFINE(HAVE_SYS_NDIR_H,        1)], [])
AC_CHECK_HEADER([sys/dir.h],       [AC_DEFINE(HAVE_SYS_DIR_H,         1)], [])
AC_CHECK_HEADER([stdint.h],        [AC_DEFINE(HAVE_STDINT_H,          1)], [])
AC_CHECK_HEADER([inttypes.h],      [AC_DEFINE(HAVE_INTTYPES_H,        1)], [])
AC_CHECK_HEADER([byteswap.h],      [AC_DEFINE(HAVE_BYTESWAP_H,        1)], [])
AC_CHECK_HEADER([unistd.h],        [AC_DEFINE(HAVE_UNISTD_H,          1)], [])
AC_CHECK_HEADER([limits.h],        [AC_DEFINE(HAVE_LIMITS_H,          1)], [])
AC_CHECK_HEADER([string.h],        [AC_DEFINE(HAVE_STRING_H,          1)], [AC_DEFINE(NO_STRING_H,1,"Define to 1 if you do not have <string.h>.")])
AC_CHECK_HEADER([strings.h],       [AC_DEFINE(HAVE_STRINGS_H,         1)], [])
AC_CHECK_HEADER([errno.h],         [AC_DEFINE(HAVE_ERRNO_H,           1)], [])
AC_CHECK_HEADER([sys/cdefs.h],     [AC_DEFINE(HAVE_SYS_CDEFS_H,       1)], [])
AC_CHECK_HEADER([sys/stat.h],      [AC_DEFINE(HAVE_SYS_STAT_H,        1)], [])
AC_CHECK_HEADER([sys/time.h],      [AC_DEFINE(HAVE_SYS_TIME_H,        1)], [])
AC_CHECK_HEADER([sys/types.h],     [AC_DEFINE(HAVE_SYS_TYPES_H,       1)], [])
AC_CHECK_HEADER([sys/param.h],     [AC_DEFINE(HAVE_SYS_PARAM_H,       1)], [])
AC_CHECK_HEADER([machine/endian.h],[AC_DEFINE(HAVE_MACHINE_ENDIAN_H,  1)], [])
AC_CHECK_HEADER([pwd.h],           [AC_DEFINE(HAVE_PWD_H,             1)], [])

AC_CHECK_FUNC([getpwnam], [HAVE_GETPWNAM=1; AC_DEFINE(HAVE_GETPWNAM)])

dnl timidity patch dir
AC_ARG_WITH([timidity-default-path], [AS_HELP_STRING([--with-timidity-default-path=DIR], [Where timidity.cfg is  (PREFIX/share/timidity)])],
  [timiditypkgdatadir=$withval],
  [
    AS_IF([test "x$prefix" != "xNONE"], [timiditypkgdatadir='${prefix}/share/timidity'],
          [timiditypkgdatadir='/usr/local/share/timidity'])
  ]
)
tmpdefpath="`eval \"echo ${timiditypkgdatadir}\"`"
AC_DEFINE_UNQUOTED(TIMIDITY_DEFAULT_PATH,"$tmpdefpath",place to install patches)
AC_DEFINE(TIMIDITYPKGDATADIR,TIMIDITY_DEFAULT_PATH,a compatibility matter. ignore it.)

dnl We first check for ncursesw, if that fails we revert back to ncurses
AX_WITH_CURSES
AS_IF([test "x$ax_cv_ncursesw" != xyes && test "x$ax_cv_ncurses" != xyes], [AC_MSG_ERROR([requires either NcursesW or Ncurses library])])
AS_IF([test "x$ax_cv_curses_color" != xyes], [AC_MSG_ERROR([requires an X/Open-compatible Curses library with color])])
push_LIBS=$LIBS
LIBS=$CURSES_LIB
AC_CHECK_FUNCS([resize_term])
LIBS=$push_LIBS

AC_CHECK_LIB([z], [zlibVersion], [], [AC_MSG_ERROR("zlib not found")])
AC_CHECK_HEADER([zlib.h], [], [AC_MSG_ERROR("zlib header files was not found")])

AC_CHECK_LIB([bz2], [BZ2_bzDecompress], [], [AC_MSG_ERROR([bzip2 library not found (libbz2-dev)])])
AC_CHECK_HEADER([bzlib.h], [], [AC_MSG_ERROR("bzip2 header file not found (libbz2-dev)")])

AC_SUBST(DL_LIBS)
dnl -lc is dirty hack here
push_LIBS=$LIBS
LIBS=
dnl refresh LIB list
AC_LANG_PUSH(C)
AC_CHECK_FUNC([dlopen], [DL_LIBS=""],[AC_CHECK_LIB([dl], [dlopen], [DL_LIBS="-ldl"], [AC_MSG_ERROR("dlopen not found")])])
LIBS=$push_LIBS
AC_LANG_POP

AC_SUBST(MAD_CFLAGS)
AC_SUBST(MAD_LIBS)
AC_SUBST(HAVE_MAD)
AS_IF([test "x$with_mad" != "xno"],[
	PKG_CHECK_MODULES([MAD],[mad],[],[
	dnl Fall back to non-pkg-config method
		AC_CHECK_LIB([mad], [mad_stream_init], [], [
			AS_IF([test "x$with_mad" = "xyes"], [AC_MSG_ERROR("libmad not found")],
			      [with_mad="no"])
		])
		AS_IF([test "x$with_mad" != "xno"], [
			AC_CHECK_HEADER([mad.h], [], [
				AS_IF([test "x$with_mad" = "xyes"], [AC_MSG_ERROR("libmad header files was not found")],
				      [with_mad="no"])
			])
		])
		AS_IF([test "x$with_mad" != "xno"], [
			MAD_LIBS="-lmad"
			MAD_CFLAGS=""
			LIBS=$push_LIBS
		])
	])
])
AS_IF([test "x$with_mad" = "xno"], [
	HAVE_MAD=
],[
	HAVE_MAD=1
	AC_DEFINE(HAVE_MAD)
])


PKG_CHECK_MODULES([LIBJPEG],[libturbojpeg],[AS_ECHO("libturbojpeg flags ... $LIBJPEG_CFLAGS $LIBJPEG_LIBS")],[
	PKG_CHECK_MODULES([LIBJPEG],[libjpeg],[AS_ECHO("libjpeg flags ... $LIBJPEG_CFLAGS $LIBJPEG_LIBS")],[AC_MSG_ERROR([failed to find either libturbojpeg or libjpeg])])
])

PKG_CHECK_MODULES([LIBPNG],[libpng],[],[AC_MSG_ERROR([failed to find libpng])])

AC_SUBST(OGG_CFLAGS)
AC_SUBST(OGG_LIBS)
PKG_CHECK_MODULES([OGG],[ogg],[],[
dnl Fall back to non-pkg-config method
	AC_CHECK_LIB([ogg], [ogg_sync_init], [], [AC_MSG_ERROR("ogg libraries not found")])
	OGG_LIBS="-logg"
	OGG_CFLAGS=""
	LIBS=$push_LIBS
])

AC_SUBST(VORBIS_CFLAGS)
AC_SUBST(VORBIS_LIBS)
PKG_CHECK_MODULES([VORBIS],[vorbis],[],[
dnl Fall back to non-pkg-config method
	AC_CHECK_LIB([vorbis], [vorbis_bitrate_init], [], [AC_MSG_ERROR("vorbis libraries not found")], [$OGG_LIBS])
	AC_CHECK_HEADER([vorbis/codec.h], [], [AC_MSG_ERROR("libvorbis header files was not found")])
	VORBIS_LIBS="-lvorbis"
	VORBIS_CFLAGS=""
	LIBS=$push_LIBS
])

AC_SUBST(VORBISFILE_CFLAGS)
AC_SUBST(VORBISFILE_LIBS)
PKG_CHECK_MODULES([VORBISFILE],[vorbisfile],[],[
dnl Fall back to non-pkg-config method
	AC_CHECK_LIB([vorbisfile], [ov_pcm_seek_lap], [AC_DEFINE(HAVE_OV_PCM_SEEK_LAP, 1)],
		AC_CHECK_LIB([vorbisfile], [ov_pcm_seek], [AC_DEFINE(HAVE_OV_PCM_SEEK, 1)], [AC_MSG_ERROR("vorbis libraries not found")], [$OGG_LIBS $VORBIS_LIBS])
	)
	AC_CHECK_HEADER([vorbis/vorbisfile.h], [], [AC_MSG_ERROR("libvorbis header files were not found")])
	VORBISFILE_LIBS="-lvorbisfile"
	VORBISFILE_CFLAGS=""
	LIBS=$push_LIBS
])

AC_SUBST(FLAC_CFLAGS)
AC_SUBST(FLAC_LIBS)
AC_SUBST(HAVE_FLAC)
AS_IF([test "x$with_flac" != "xno"],[
	PKG_CHECK_MODULES([FLAC],[flac],[],[
		AC_CHECK_LIB([FLAC], [FLAC__seekable_stream_decoder_process_until_end_of_metadata], [FLAC_LIBS="-lFLAC"],
			AC_CHECK_LIB([FLAC], [FLAC__stream_decoder_process_until_end_of_metadata], [FLAC_LIBS="-lFLAC"],
				[AS_IF([test "x$with_flac" = "xyes"], [AC_MSG_ERROR("libFLAC not found")], [with_flac="no"])],
				[-lz]),
		[-lz])
		AC_CHECK_HEADER([FLAC/all.h], [FLAC_CFLAGS=""],
			[  AS_IF([test "x$with_flac" = "xyes"], [AC_MSG_ERROR("libFLAC header files were not found")], [with_flac="no"]) ])
		LIBS=$push_LIBS
	])
])
AC_SUBST(HAVE_FLAC)
AS_IF([test "x$with_flac" = "xno"], [HAVE_FLAC=], [HAVE_FLAC=1])

AS_IF([test "x$with_x11" != "xno"], [
	AC_CHECK_HEADER([X11/extensions/xf86vmode.h], [], [ AS_IF([test "x$with_x11" = "xyes"], [AC_MSG_ERROR("xvidmode header files was not found")], [with_x11="no"]) ],
[#include <X11/Xlib.h>
])
])

AS_IF([test "x$with_x11" != "xno"], [
	AC_CHECK_HEADER([X11/extensions/XShm.h], [], [ AS_IF([test "x$with_x11" = "xyes"], [AC_MSG_ERROR("XShm header files was not found")], [with_x11="no"]) ],
[#include <X11/Xlib.h>
])
])

AS_IF([test "x$with_x11" != "xno"], [
	AC_CHECK_HEADER([X11/xpm.h], [], [ AS_IF([test "x$with_x11" = "xyes"], [AC_MSG_ERROR("xpm header files was not found")], [with_x11="no"]) ],
[#include <X11/Xlib.h>
])
])

AS_IF([test "x$with_x11" != "xno"], [
	AC_CHECK_LIB([Xpm], [XpmCreatePixmapFromData], [X11_LIBS="-lXpm -lX11 -L/usr/X11R6/lib"], [ AS_IF([test "x$with_x11" = "xyes"], [AC_MSG_ERROR(["Xpm/X11 not found (expected to be found in /usr/X11R6/lib)"])], [with_x11="no"]) ], [-lXpm -lX11 -L/usr/X11R6/lib])
	LIBS=$push_LIBS
])

AS_IF([test "x$with_x11" != "xno"], [
	AC_CHECK_LIB([Xext], [XShmQueryExtension], [X11_LIBS="-lXext $X11_LIBS"], [ AS_IF([test "x$with_x11" = "xyes"], [AC_MSG_ERROR(["SHM/X11 not found (expected to be found in /usr/X11R6/lib)"])], [with_x11="no"]) ], [-lX11 -lXext -L/usr/X11R6/lib])
	LIBS=$push_LIBS
])

AS_IF([test "x$with_x11" != "xno"], [
	AC_CHECK_LIB([Xxf86vm], [XF86VidModeQueryExtension], [X11_LIBS="-lXxf86vm $X11_LIBS"], [ AS_IF([test "x$with_x11" = "xyes"], [AC_MSG_ERROR(["xvidmode was not found (expected to be found in /usr/X11R6/lib)"]) ], [with_x11="no"]) ], [-lX11 -lXext -L/usr/X11R6/lib])
	LIBS=$push_LIBS
])
AC_SUBST(HAVE_X11)
AS_IF([test "x$with_x11" != "xno"], [
	X11_LIBS=
	HAVE_X11=
],[
	HAVE_X11=1
	AC_DEFINE(HAVE_X11)
])

AS_IF([test "x$with_sdl" = "xyes" && test "x$with_sdl2" = "xyes"], [AC_MSG_ERROR("Can not use both --with-sdl and --with-sdl2")])
AS_IF([test "x$with_sdl" = "xyes"], [with_sdl2="no"])
AS_IF([test "x$with_sdl2" = "xyes"], [with_sdl="no"])

AC_SUBST(SDL2_CFLAGS)
AC_SUBST(SDL2_LIBS)
AS_IF([test "x$with_sdl2" != "xno"], [
	AM_PATH_SDL2(["2.0.0"], [with_sdl2=yes], AS_IF([test "x$with_sdl2" = "xyes"], [AC_MSG_ERROR("libSDL2 was not found")], [with_sdl2="no"]) )
])
SDL2_CFLAGS=$SDL_CFLAGS
SDL2_LIBS=$SDL_LIBS
SDL_CFLAGS=
SDL_LIBS=
AC_SUBST(HAVE_SDL2)

AS_IF([test "x$with_sdl2" = "xno"],
	HAVE_SDL2=
],[
	HAVE_SDL2=1
	AC_DEFINE(HAVE_SDL2)
	with_sdl="no"
])

org_cflags="$CFLAGS"
org_cppflags="$CPPFLAGS"
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)
AS_IF([test "x$with_sdl" != "xno"],[
	PKG_CHECK_MODULES([SDL],[sdl],[],[
	dnl Fall back to non-pkg-config method
		AC_CHECK_LIB([SDL], [SDL_Init], [SDL_LIBS="-lSDL"], [ AS_IF([test "x$with_sdl" = "xyes"],[AC_MSG_ERROR("libSDL was not found")],[with_sdl="no"]) ])
		AS_IF([test "x$with_sdl" != "xno"],[
			CFLAGS="$CFLAGS -I/usr/include/SDL"
			CPPFLAGS="$CPPFLAGS -I/usr/include/SDL"
			AC_CHECK_HEADER([SDL.h], [SDL_CFLAGS="-I/usr/include/SDL"], [ AS_IF([test "x$with_sdl" = "xyes"], [AC_MSG_ERROR("SDL header files not found")],[with_sdl="no"]) ])
		])
	])
])
AC_SUBST(HAVE_SDL)
AS_IF([test "x$with_sdl" = "xno"],[
	HAVE_SDL=
],[
	HAVE_SDL=1
	AC_DEFINE(HAVE_SDL)
])
CFLAGS="$org_cflags"
CPPFLAGS="$org_cflags"

AC_SUBST(FREETYPE2_LIBS)
AC_SUBST(FREETYPE2_CFLAGS)
AS_IF([test "x$HAVE_SDL" = "x1" || test "x$HAVE_SDL2" = "x1" || test "x$HAVE_X11" = "x1"],[
	PKG_CHECK_MODULES([FREETYPE2],[freetype2],[],[
	dnl Fall back to non-pkg-config method
		AC_CHECK_LIB([freetype], [FT_Init_FreeType], [],[AC_MSG_ERROR("libfreetype not found")])
		AC_CHECK_HEADER([ft2built.h], [] , [AC_MSG_ERROR("libfreetype header files was not found")])
	])
dnl locate the ttf-unifont
	AS_AC_EXPAND(UNIFONTDIR, $with_unifontdir)
	AC_CHECK_FILE([$UNIFONTDIR/unifont.ttf], [], [AC_MSG_ERROR([$UNIFONTDIR/unifont.ttf not found - please use --with-unifontdir=/path/ (needed by X11, SDL1.x and SDL2)])])
	AC_CHECK_FILE([$UNIFONTDIR/unifont_csur.ttf], [], [AC_MSG_ERROR([$UNIFONTDIR/unifont_csur.ttf not found - please use --with-unifontdir=/path/ (needed by X11, SDL1.x and SDL2)])])
	AC_CHECK_FILE([$UNIFONTDIR/unifont_upper.ttf], [], [AC_MSG_ERROR([$UNIFONTDIR/unifont_upper.ttf not found - please use --with-unifontdir=/path/ (needed by X11, SDL1.x and SDL2)])])
	AC_DEFINE_UNQUOTED([UNIFONTDIR], "$UNIFONTDIR")
])

AC_LANG_PUSH(C++)
org_cxxflags="$CXXFLAGS"
org_cppflags="$CPPFLAGS"
AC_SUBST(ADPLUG_CXXFLAGS)
AC_SUBST(ADPLUG_LIBS)
AS_IF([test "x$with_adplug" != "xno"],[
	PKG_CHECK_MODULES([ADPLUG],[adplug],[ADPLUG_CXXFLAGS=$ADPLUG_CFLAGS],[
	dnl Fall back to non-pkg-config method
		AC_CHECK_HEADER([adplug/adplug.h], [], [with_adplug_failed=yes])
		AS_IF([test "x$with_adplug_failed" = "xyes"],[
			CXXFLAGS="$CXXFLAGS -I/usr/include/libbinio"
			CPPFLAGS="$CPPFLAGS -I/usr/include/libbinio"
			unset ac_cv_header_adplug_adplug_h
			AC_MSG_NOTICE("Trying again with -I/usr/include/libbinio")
			AC_CHECK_HEADER([adplug/adplug.h],
				[ADPLUG_CXXFLAGS="-I/usr/include/libbinio"],
				[ AS_IF([test "x$with_adplug" = "xyes"], [AC_MSG_ERROR("Adplug header files was not found")], [with_adplug="no"]) ]
			)
		])
		AS_IF([test "x$with_adplug" != "xno"],[
			AC_CHECK_LIB([adplug], [docell0], [ADPLUG_LIBS="-ladplug"], [ AS_IF([test "x$with_adplug" = "xyes"], [AC_MSG_ERROR("Adplug was not found")], [with_adplug="no"]) ])
			LIBS=$push_LIBS
		])
	])
])
CXXFLAGS="$org_cxxflags"
CPPFLAGS="$org_cppflags"

AS_IF([test "x$with_adplug" != "xno"],[
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
			]]
		)],
		[AC_MSG_RESULT([ok])],
		[AC_MSG_ERROR("Adplug was compiled with a different c++ compiler")]
	)
	LIBS=$push_LIBS
	CXXFLAGS="$org_cxxflags"
	CPPFLAGS="$org_cppflags"
])

AS_IF([test "x$with_adplug" != "xno"],[
AC_MSG_CHECKING([If libbinio used by AdPlug contains bugs https://github.com/adplug/libbinio/issues/2])
	CXXFLAGS="$CXXFLAGS $ADPLUG_CXXFLAGS"
	LIBS="$LIBS $ADPLUG_LIBS"

	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([[
#include <binio.h>
#include <binstr.h>
#include <cstdlib>
#include <string.h>
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
])

dnl SIDPLAY specifics
AC_MSG_CHECKING([GIT submodules are checked out])
AS_IF([test -f playsid/libsidplayfp-git/src/sidemu.cpp], [ AS_IF([test -f playsid/libsidplayfp-git/src/builders/resid-builder/resid/sid.h], [],
                                                                 [AC_MSG_ERROR([playsid/libsidplayfp-git/src/builders/resid-builder/resid is missing - please execute 'git submodule update --init --recursive'])] )],
      [AC_MSG_ERROR([playsid/libsidplayfp-git is missing - please execute 'git submodule update --init --recursive'])]
)
AC_MSG_RESULT([ok])
origCXX="$CXX"
origCXXFLAGS="$CXXFLAGS"
AC_LANG_PUSH([C++])
HAVE_SIDPLAYFP_BUILDERS_RESIDFP_H=1
HAVE_SIDPLAYFP_BUILDERS_RESID_H=1
RESID_INLINING=1
RESID_INLINE=inline
AX_CXX_COMPILE_STDCXX_11([noext], [optional])
AC_ARG_VAR([XA], [6502 assembler - must be full path])
AC_PATH_PROG([XA],[xa])
AS_IF([test -z "$XA"], [AC_MSG_ERROR([xa is needed for compiling 6502 assembler used by libsidplayfp])])
AC_PATH_PROG([HEXDUMP],[hexdump])
AS_IF([test -z "$HEXDUMP"], [AC_MSG_ERROR([hexdump is needed for compiling libsidplayfp])])
AC_PATH_PROG([PERL], [perl])
AS_IF([test -z "$PERL"], [AC_MSG_ERROR([perl is needed for compiling libsidplayfp])])
dnl TODO HAVE_VISIBILITY
AX_CHECK_COMPILE_FLAG([-fvisibility=hidden -fvisibility-inlines-hidden], [HAVE_VISIBILITY=1;has_visibility=yes], [has_visibility=no])
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AS_IF([test $ac_cv_sizeof_short -ne 2], [AC_MSG_ERROR([LIBSIDPLAYFP: size of short must be 16 bit])])
AS_IF([test $ac_cv_sizeof_int -lt 4], [AC_MSG_ERROR([LIBSIDPLAYFP: only 32 bit or better CPUs are supported])])
AS_IF([test "x${ac_cv_header_stdint_h}" != "xyes"], [AC_MSG_ERROR([Required header stdint.h not found])])
AC_CHECK_DECL(
    [strcasecmp],
    [AC_CHECK_FUNCS([strcasecmp])],
    [AC_CHECK_DECL(
        [stricmp],
        [AC_CHECK_FUNCS([stricmp])]
    )]
)
AC_CHECK_DECL(
    [strncasecmp],
    [AC_CHECK_FUNCS([strncasecmp])],
    [AC_CHECK_DECL(
        [strnicmp],
        [AC_CHECK_FUNCS([strnicmp])]
    )]
)
AC_CACHE_CHECK([for working bool], ac_cv_cxx_bool,
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([],
    [[bool flag;]])],
  [ac_cv_cxx_bool=yes],
  [ac_cv_cxx_bool=no])]
)
AS_IF([test "x$ac_cv_cxx_bool" = "xno"], [HAVE_BOOL=0], [HAVE_BOOL=1])
dnl TODO RESID_BRANCH_HINTS
dnl Enable branch prediction hints.
AC_ARG_ENABLE([branch-hints],
  [AS_HELP_STRING([--enable-branch-hints],
    [enable static branch prediction hints [default=yes]])],
  [],
  [enable_branch_hints=yes]
)
AS_IF([test "$enable_branch_hints" != no], [RESID_BRANCH_HINTS=1], [RESID_BRANCH_HINTS=0])
AC_CACHE_CHECK([for log1p], [resid_cv_log1p],
  [AC_TRY_COMPILE([#include <math.h>], [ log1p(1); ],
    resid_cv_log1p=yes, resid_cv_log1p=no)])
AS_IF([test "$resid_cv_log1p" = no], [HAVE_LOG1P=0], [HAVE_LOG1P=1])
AC_CACHE_CHECK([for __builtin_expect], [resid_cv_builtin_expect],
  [AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() { __builtin_expect(0, 0); }])],
    [resid_cv_builtin_expect=yes], [resid_cv_builtin_expect=no])]
)
AS_IF([test "$resid_cv_builtin_expect" = no], [HAVE_BUILTIN_EXPECT=0], [HAVE_BUILTIN_EXPECT=1])
LIBSIDPLAYCXX="$CXX -DHAVE_CONFIG_H=1 -DSIDTUNE_NO_STDIN_LOADER=1 -DPACKAGE_NAME=\\\"sidplayfp\\\" -DVERSION=\\\"2.3.1\\\" -DPACKAGE_VERSION=\\\"2.2.3-1\\\" -DPACKAGE_URL=\\\"\\\""
LIBSIDPLAYCXXFLAGS="$CXXFLAGS"
AC_SUBST(LIBSIDPLAYCXX)
AC_SUBST(LIBSIDPLAYCXXFLAGS)
AC_LANG_POP
CXX=$origCXX
CXXFLAGS=$origCXXFLAGS
NEW_8580_FILTER=0

AC_SUBST(HAVE_BOOL)
AC_SUBST(HAVE_LOG1P)
AC_SUBST(HAVE_BUILTIN_EXPECT)
AC_SUBST(RESID_INLINING)
AC_SUBST(RESID_INLINE)
AC_SUBST(RESID_BRANCH_HINTS)
AC_SUBST(NEW_8580_FILTER)
AC_SUBST(HAVE_SIDPLAYFP_BUILDERS_RESIDFP_H)
AC_SUBST(HAVE_SIDPLAYFP_BUILDERS_RESID_H)
AC_LANG_POP()

PKG_CHECK_MODULES([LIBDISCID],[libdiscid],[],[AC_MSG_ERROR([libdiscid not found])])

AS_IF([test "x$with_alsa" != "xno"],[
	PKG_CHECK_MODULES([ALSA],[alsa],[],[
		AS_IF([test "x$with_alsa" = "xyes"], [AC_MSG_ERROR([pkg-config failed])], [with_alsa="no"])
	])
])

AC_SUBST(HAVE_ALSA)
AS_IF([test "x$with_alsa" = "xno"],[HAVE_ALSA=],[HAVE_ALSA=1])

AC_SUBST(HAVE_ADPLUG)
AC_SUBST(ADPLUG_INI)
AS_IF([test "x$with_adplug" = "xno"],[
	HAVE_ADPLUG=
	ADPLUG_INI=
],[
	HAVE_ADPLUG=1
	ADPLUG_INI=opltype
])

AC_SUBST(LINUX)
AC_SUBST(CDROM_SUPPORT)
AC_SUBST(X11_LIBS)
AC_SUBST(CDROM_INI)
AC_SUBST(HAVE_FRAMEBUFFER)
HAVE_FRAMEBUFFER=1
LINUX=1
	AC_CHECK_HEADER([linux/limits.h], [], [AC_MSG_RESULT([linux kernel header files was not found]);LINUX=])
AS_IF([test "$LINUX+set" != "+set"], [ AC_CHECK_HEADER([linux/major.h], [], [AC_MSG_RESULT([linux kernel header files was not found]);LINUX=]) ])
AS_IF([test "$LINUX+set" != "+set"], [ AC_CHECK_HEADER([linux/fb.h], [AC_DEFINE(HAVE_FRAMEBUFFER)], [AC_MSG_RESULT([linux kernel header files was not found]);LINUX=;HAVE_FRAMEBUFFER=]) ])
AS_IF([test "$LINUX+set" != "+set"], [ AC_CHECK_HEADER([linux/cdrom.h], [CDROM_SUPPORT=1;CDROM_INI="cdrom"], [AC_MSG_RESULT([linux cdrom kernel header files was not found])]) ])

AC_SUBST(HAVE_OSS)
AC_SUBST(OSS_LIBS)
AC_SUBST(OSS_CFLAGS)
AS_IF([test "x$with_oss" != "xno"], [
	PKG_CHECK_MODULES([OSS],[oss],[HAVE_OSS=1],[
dnl Fall back to non-pkg-config method (Normal for Linux systems)
		AC_CHECK_HEADER([sys/soundcard.h],
			[HAVE_OSS=1;AC_DEFINE(HAVE_SYS_SOUNDCARD_H)],
			[ AS_IF([test "x$with_oss" = "xyes"], [AC_MSG_ERROR([kernel OSS header file was not found], 1)],[with_oss="no";HAVE_OSS=]) ]
		)
	])
])

AC_SUBST(HAVE_COREAUDIO)
AC_SUBST(COREAUDIO_CFLAGS)
AC_SUBST(COREAUDIO_LIBS)
AS_IF([test "x$with_coreaudio" != "xno"],[
	AC_CHECK_HEADER([CoreServices/CoreServices.h], [HAVE_COREAUDIO=1], [ AS_IF([test "x$with_coreaudio" = "xyes"], [AC_MSG_ERROR([CoreAudio not found], 1)], [with_coreaudio="no"; HAVE_COREAUDIO=]) ])
])
AS_IF([test "x$with_coreaudio" != "xno"],[
	AC_CHECK_HEADER([AudioUnit/AudioUnit.h], [HAVE_COREAUDIO=1], [ AS_IF([test "x$with_coreaudio" = "xyes"], [AC_MSG_ERROR([CoreAudio not found], 1)], [with_coreaudio="no"; HAVE_COREAUDIO=]) ])
])
AS_IF([test "x$with_coreaudio" != "xno"],[
	AC_CHECK_HEADER([AudioToolbox/AudioToolbox.h], [HAVE_COREAUDIO=1], [ AS_IF([test "x$with_coreaudio" = "xyes"], [AC_MSG_ERROR([CoreAudio not found], 1)], [with_coreaudio="no"; HAVE_COREAUDIO=]) ])
])
AS_IF([test "x$with_coreaudio" != "xno"],[
	AC_CHECK_HEADER([CoreAudio/CoreAudio.h], [HAVE_COREAUDIO=1], [ AS_IF([test "x$with_coreaudio" = "xyes"], [AC_MSG_ERROR([CoreAudio not found], 1)], [with_coreaudio="no"; HAVE_COREAUDIO=]) ])
])
AS_IF([test "x$with_coreaudio" != "xno"],[
	COREAUDIO_CFLAGS=""
	COREAUDIO_LIBS="-framework CoreAudio -framework Carbon -framework AudioUnit"
])

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

AC_ARG_WITH([efence], [AS_HELP_STRING([--with-efence],[Enable usage of efence])], [EFENCE_LIBS="-lefence"; AC_DEFINE(HAVE_EFENCE, 1)])
AC_SUBST(EFENCE_LIBS)

AC_ARG_WITH([duma], [AS_HELP_STRING([--with-duma],[Enable usage of duma])], [DUMA_LIBS="-lduma"; AC_DEFINE(HAVE_DUMA, 1)])
AC_SUBST(DUMA_LIBS)


AC_SUBST(HAVE_MAKEINFO)
AC_CHECK_PROG([HAVE_MAKEINFO], [makeinfo], [yes], [])

AC_SUBST(HAVE_LZW)
AC_MSG_CHECKING([Include lzw support (used by GIF and arcZIP)])
AS_IF([test "x$with_lzw" = "xyes"],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_LZW, 1)
	HAVE_LZW=1
],[
	AC_MSG_RESULT([no])
])

AC_SUBST(HAVE_LZH)
AC_MSG_CHECKING([Include lzh support (used by .ym file support)])
AS_IF([test "x$with_lzh" = "xyes"],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_LZH, 1)
	HAVE_LZH=1
],[
	AC_MSG_RESULT([no])
])

AC_MSG_CHECKING("cp -f implies --remove-destination")
cp -f --remove-destination Rules.make.in Rules.make.in.tmp > /dev/null 2>&1
AS_IF([test "$?" -ne 0],[
	AC_MSG_RESULT([yes])
	CP="cp -f"
],[
	AC_MSG_RESULT([no])
	CP="cp -f --remove-destination"
])
rm -f Rules.make.in.tmp
AC_SUBST(CP)

AC_ARG_WITH([libiconv],
	[AS_HELP_STRING([--with-libiconv=], [@<:@auto/no/gnu/native@:>@ what kind of libiconv library to use])],
	[with_libiconv=$withval],
	[with_libiconv=auto])

found_iconv=no
case $with_libiconv in
	auto)
		# Check in the C library first
		AC_CHECK_FUNC([iconv_open], [with_libiconv=no; found_iconv=yes])
		# Check if we have GNU libiconv
		AS_IF([test $found_iconv = "no"], [ AC_CHECK_LIB([iconv], [libiconv_open], [ICONV_LIBS=-liconv; with_libiconv=gnu; found_iconv=yes]) ])
		# Check if we have a iconv in -liconv, possibly from vendor
		AS_IF([test $found_iconv = "no"], [ AC_CHECK_LIB([iconv], [iconv_open], [ICONV_LIBS=-liconv; with_libiconv=native; found_iconv=yes]) ])
		;;
	no)
		AC_CHECK_FUNC([iconv_open], [with_libiconv=no; found_iconv=yes])
		;;
	gnu|yes)
		AC_CHECK_LIB([iconv], [libiconv_open], [ICONV_LIBS=-liconv; with_libiconv=gnu; found_iconv=yes])
		;;
	native)
		AC_CHECK_LIB([iconv], [iconv_open], [ICONV_LIBS=-liconv; with_libiconv=native; found_iconv=yes])
		;;
esac
LIBS=$push_LIBS

AS_IF([test "x$found_iconv" = "xno"], [ AC_MSG_ERROR([*** No iconv() implementation found in C library or libiconv]) ])
AC_SUBST(ICONV_LIBS)

case $with_libiconv in
	gnu)
		AC_DEFINE(USE_LIBICONV_GNU, 1, [Using GNU libiconv])
		;;
	native)
		AC_DEFINE(USE_LIBICONV_NATIVE, 1, [Using a native implementation of iconv in a separate library])
		;;
esac

AS_IF([test "x$with_desktop_file_install" != "xno"],[
	AS_IF([test "x$with_x11" != "xno" || test "x$with_sdl" != "xno" || test "x$with_sdl2" != "xno"], [
		AC_CHECK_PROG([DESKTOP_FILE_INSTALL], [desktop-file-install], [desktop-file-install], [failed])
		AS_IF([test "x$DESKTOP_FILE_INSTALL" = "xfailed"],[
			AC_MSG_ERROR([Cannot find desktop-file-install (required by x11 and SDL driver).])
		])
	])
])

AS_IF([test "x$with_dumptools" = "xyes"], [BUILD_DUMPTOOLS=1])
AC_SUBST(BUILD_DUMPTOOLS)

AC_CONFIG_FILES(Rules.make)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(goodies/helpc/Makefile)
AC_CONFIG_FILES(goodies/pack/Makefile)
AC_CONFIG_FILES(ocp.ini)
AC_CONFIG_FILES(desktop/opencubicplayer.desktop)

AC_CONFIG_FILES(doc/opencp.dox)
AC_CONFIG_FILES(doc/texi/faq.texi)
AC_CONFIG_FILES(doc/texi/ocp.texi)
AC_CONFIG_FILES(playsid/libsidplayfp-git/src/builders/resid-builder/resid/siddefs.h)
AC_CONFIG_FILES(playsid/libsidplayfp-git/src/builders/residfp-builder/residfp/siddefs-fp.h)

AC_OUTPUT

AS_IF([test "x$with_debug"     = "xyes"], [echo "debug:             ON" ], [echo "debug:             OFF"])
AS_IF([test "x$with_adplug"    = "xno" ], [echo "adplug:            OFF"], [echo "adplug:            ON"])
AS_IF([test "x$with_mad"       = "xno" ], [echo "mad:               OFF"], [echo "mad:               ON"])
                                           echo "sidplayfp:         GIT/builtin"
AS_IF([test "x$with_flac"      = "xno" ], [echo "FLAC:              OFF"], [echo "FLAC:              ON"])
AS_IF([test "x$with_x11"       = "xno" ], [echo "x11:               OFF"], [echo "x11:               ON  (UNIFONTDIR="$UNIFONTDIR")"])
AS_IF([test "x$with_sdl"       = "xno" ], [echo "SDL:               OFF"], [echo "SDL:               ON  (UNIFONTDIR="$UNIFONTDIR")"])
AS_IF([test "x$with_sdl2"      = "xno" ], [echo "SDL2:              OFF"], [echo "SDL2:              ON  (UNIFONTDIR="$UNIFONTDIR")"])
AS_IF([test "x$with_alsa"      = "xno" ], [echo "ALSA:              OFF"], [echo "ALSA:              ON"])
AS_IF([test "x$with_oss"       = "xno" ], [echo "OSS:               OFF"], [echo "OSS:               ON"])
AS_IF([test "x$with_coreaudio" = "xno" ], [echo "CoreAudio:         OFF"], [echo "CoreAudio:         ON"])
AS_IF([test "$LINUX+set"      != "+set"], [echo "linux cdrom:       ON"
                                           echo "linux framebuffer: ON" ], [echo "linux cdrom:       OFF"
                                                                            echo "linux framebuffer: OFF"])
AS_IF([test "x$with_lzw"       = "xno" ], [echo "include LZW:       OFF"], [echo "include LZW:       ON"])
AS_IF([test "x$with_lzh"       = "xno" ], [echo "include LZH:       OFF"], [echo "include LZH:       ON"])
AS_IF([test "x$with_dumptools" = "xyes"], [echo "include DumpTools: ON" ], [echo "include DumpTools: OFF"])
