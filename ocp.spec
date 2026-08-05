# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 3.4.1

Name: %{name}
Version: %{version}
Release: 0
Summary: Linux port of Open Cubic Player
Group: Applications/Multimedia
URL: https://stian.cubic.org/coding-ocp.php
Buildroot: /var/tmp/ocp-buildroot
Source0: https://stian.cubic.org/ocp/%{name}-%{version}.tar.bz2
Source1: https://stian.cubic.org/mirror/ftp.cubic.org/pub/player/gfx/opencp25image1.zip
Source2: https://stian.cubic.org/mirror/ftp.cubic.org/pub/player/gfx/opencp25ani1.zip
License: GPL-2, Creative Commons Attribution 3.0
# OCP itself is GPL-2
# The extra data provided is Creative Commons Attribute 3.0

%if 0%{?suse_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files libjpeg62-turbo-devel libpng16-devel xa libdiscid-devel cjson-devel alsa-devel libfreetype2-devel gnu-unifont-bitmap-fonts libgme-devel ancient-devel speex-devel libopus-devel
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel SDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel game-music-emulator-devel speex-devel opus-devel
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel libgme-devel speex-devel libopus-devel
%endif
%endif
Requires: curl

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
Changes from version 3.4.0 to 3.4.1

Bugfixes

[psgplay/sndh]
 * Try to make it compile on m68k target

[configure]
 * AC_MSG_ERROR calls should contain message inside [], not "". Failing to do so causes syntax error when running the configure script and it wants to display an error message.
 * if libspeex was not detected, auto vs forced logic was swapped around

[flac/mpX/ogg/wavpack]
 * Ensure that PictureViewer initializes to "NotVisible", avoid de-referencing invalid memory playing a file without a picture, after playing a file with a picture.

[timidity/midi]
 * Library messages should not depend on compile-time flag
 * cpiTimiditySetupInit() should be called before loading a MIDI song, so that reverb and chorus are initialized to wanted settings as early as possible.
 * Update Timidity, protect against roomsize reaching zero, causing inf math and assertion/abort on certain systems.

Changes from version 3.3.1 to 3.4.0:

Added support for *.SPX, *.WV, *.OGA, *.XMI and *.WM

[speex]
 * Initial support (*.spx)

[wavpack]
 * Initial support (*.wv)

[flac]
 * Added support for FLAC embedded into OGG (*.OGA)

[help]
 * Use the correct screen-width if displaying error-messages, avoid crash blanking invalid memory

[mingw]
 * Update version-numbers of packages used by mingw build

[adplug/opl]
 * update adplug to latest master
   * adds support for *.xmi, Miles Design music
   * adds support for *.wm, Silky's MusicV WM OPL3 format

[sidplayfp/sid]
 * Update to latest masters.

[sndh]
 * update psgplay to latest master.

[qoa]
 * update to latest master
   * Adds more sanitization
   * Fixes Out-Of-Bounds read for special crafted .QOA files

[libmad/mpX]
 * Skip frame 0, if it is a meta-frame.
 * Correct MPEG version and LAYER information in the status header.
 * File-browser detection, now uses information in meta-header to calculate song length if available if no ID3 tag is available.

[cue/toc]
 * Add support for WavPack, Ogg, FLAC and MP3 compressed audio
 *.WAV files, the result of case-insenstive-search was never used.

[udf]
 * Filesystem did not parse correctly if CDFS_DEBUG was not defined.

[wav]
 * Fix parsing META comments

[configure]
 * Detect and error-out if -flto is detected in CFLAGS/CXXFLAGS/LDFLAGS. It causes problems with the internal list of core plugins.
 * --with-strip_lto_flags, that can automatically strip away -flto flags.

%prep
%setup -q -n %{name}-%{version}
unzip $RPM_SOURCE_DIR/opencp25image1.zip
unzip -o $RPM_SOURCE_DIR/opencp25ani1.zip

%build
CFLAGS=$RPM_OPT_FLAGS CXXFLAGS=$RPM_OPT_FLAGS ./configure --prefix=%{_prefix} --exec_prefix=%{_exec_prefix} --infodir=%{_infodir} --sysconfdir=/etc
make

%post
if [ "$1" = "1" ] ; then  # first install
	if [ -x /sbin/install-info ]; then
		install-info --info-dir=%{_infodir} %{_infodir}/ocp.info.gz || true
	fi
fi

%preun
if [ "$1" = "0" ] ; then # last uninstall
	if [ -x /sbin/install-info ]; then
		install-info --delete --info-dir=%{_infodir} %{_infodir}/ocp.info.gz || true
	fi
fi

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
make DESTDIR=%{buildroot} install
%if 0%{?suse_version}
 %suse_update_desktop_file -n -r cubic.org-opencubicplayer AudioVideo Player
%endif

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%config %{_prefix}/share/ocp/etc/ocp.ini
%{_exec_prefix}/lib/ocp
%{_prefix}/bin/ocp
%{_prefix}/bin/ocp-curses
%{_prefix}/bin/ocp-sdl3
%{_prefix}/bin/ocp-vcsa
%{_infodir}/ocp.info.gz
%{_prefix}/share/icons/hicolor/16x16/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/22x22/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/24x24/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/32x32/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/48x48/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/48x48/apps/opencubicplayer.xpm
%{_prefix}/share/icons/hicolor/128x128/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/scalable/apps/opencubicplayer.svg
%{_prefix}/share/applications/cubic.org-opencubicplayer.desktop

%dir %{_prefix}/share/ocp

%docdir %{_prefix}/share/doc/ocp
%{_prefix}/share/doc/ocp
