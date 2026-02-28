# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 3.2.0

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
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files libjpeg62-turbo-devel libpng16-devel xa libdiscid-devel cjson-devel alsa-devel libfreetype2-devel gnu-unifont-bitmap-fonts libgme-devel ancient-devel
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel SDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel game-music-emulator-devel
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel libgme-devel
%endif
%endif
Requires: curl

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
Changes from version 3.1.3 to 3.2.0:

 * [SDL] Add support for SDL3 (>= 3.2.0).
 * [mingw] Update libpng to latest version (1.6.55), and SDL from 2.x.x to to
   latest version (3.4.2).
 * Update libsidplayfp to latest version, includes a faster integer SID emulator
   crSID.
 * [CDA]
  * On Linux CDROM driver - silence two valgrind warnings (non-initialized
    fields in IOCTL, but they are output only fields)
  * Do not assert (and exit program) if attempting to play a DATA-only CD.
 * [ocp.ini]
  * Add support for extra file extensions the file-browser accepts in addition
    to those provided by the plugins. This can be edited from ALT-C dialog,
    option E.
  * Increase the line-buffer from 1024 bytes to 65536, and also allow for up to
    64K bytes of string data.
 * [M15,M31] adjust detection to not rely on filenames ending with .MOD, inspect
   instrument volumes and orderlength.
 * [NCurses] Default to the cursor being hidden.
 * Use strverscmp when sorting files in the file-browser
 * [SDL2/SDL3 video] If 320x200 is streetched to 640x200, request 640x400
   instead so we keep the original ratio.
 * [wurfel animation and background pictures]
  * Search in ZIP files
  * Allow animations to end with .ANI
  * File discovery message needs to use fwprintf() on Windows
  * Filenames with extensions longer or fewer than 3 characters were blindly
    accepted instead of rejected as background picture
  * .spec file now sources the historically files from https mirror instead ftp
  * Include the historically files in the windows build.
 * [ADPLUG, SID, YM, QOA, WAV, FLAC] Master balance was inverted
 * [XM] Improve the loader when the files have incorrect instrument sizes /
   samples truncated.
 * [IT]
  * Allow to load files with does does not contain all the sample data.
  * Remake tracker detection for the file comment in the file-browser - based on
    Schism tracker source code.
 * [dirdb] Use sorted lists and binary search instead of single-linked lists;
   Speeds up operations on the tree a lot.
 * [ZIP,TAR] Directory list is now stored in a sorted list instead of linked
   list - and searches are now performed with binary search.
 * [SID, Windows]
   * If browsing ROM files outside %APPDATA%OpenCubicPlayer/Data, the resulting
     path did not contain a drive and had slashes in the wrong direction.
   * If ROM filepath contained any non-ASCII characters, they would fail to open
     for usage in playback.
 * [CDFS] Protect against recursive directories and high directory depths.
 * [Archive Cache Database] BinarySearch was done 32bit instead of 64bit,
   causing assertion on large files.

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
