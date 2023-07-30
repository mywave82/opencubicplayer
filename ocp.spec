# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 0.2.106

Name: %{name}
Version: %{version}
Release: 0
Summary: Linux port of Open Cubic Player
Group: Applications/Multimedia
URL: https://stian.cubic.org/coding-ocp.php
Buildroot: /var/tmp/ocp-buildroot
Source0: https://stian.cubic.org/ocp/%{name}-%{version}.tar.bz2
Source1: ftp://ftp.cubic.org/pub/player/gfx/opencp25image1.zip
Source2: ftp://ftp.cubic.org/pub/player/gfx/opencp25ani1.zip
License: GPL-2, Creative Commons Attribution 3.0
# OCP itself is GPL-2
# The extra data provided is Creative Commons Attribute 3.0

%if 0%{?suse_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files libjpeg62-turbo-devel libpng16-devel xa libdiscid-devel cjson-devel alsa-devel libfreetype2-devel gnu-unifont-bitmap-fonts libgme-devel ancient-devel
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel SDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel game-music-emulator-devel
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel libgme-devel
%endif
%endif

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
 Changes from version 0.2.105 to 0.2.106:

 * [IT] Increase the number of max-samples to match openMPT (it can export files with more samples that original tracker and Schism supports)
 * [devpdisk] Reported time during playback was random
 * [adplug] Add support for SudoMakers RetroWave OPL3 Express, please configure it in setup:/adplugconfig.dev
 * [adplug] Make channel tracker aware of AM/FM modes so it more correctly can display visualization
 * [adplug] Left/Right OPL3 logic was incorrect in the register-tracker
 * [mingw] Initial support (BETA) for mingw, enables Windows support
 * [libancient] Add more fingerprints for compression formats that v2.1.0 can decompress
 * [Linux CDROM] Fix deadlock
 * [configure] cleanup --bindir --libdir and --datadir, and new syntax to override post ocp suffixes: ./configure LIBDIROCP=/usr/lib/ocp DATADIROCP=/usr/data/ocp
 * [configure] removed --with-dir-suffix
 * [CDROM *.CUE] REM didn't work as expected
 * [CDROM *.CUE] files didn't work if containing INDEX 00 (pregap)
 * [CDROM *.CUE] BINARY keyword should be little endian, but there are tools that produce big-endian files without marking them correctly. So we need to detect the endian used.
 * [CDROM *.CUE] files didn't include pregaps in the track table
 * [CDROM *.TOC] files didn't split the logic for pregap and offset into the raw file
 * [musicbrainz] Increase the buffersize, some data retrivals failed
 * [global MIME database] Add adplug fileformats
 * [global MIME database] Add Game Music Emulator fileformats
 * [SDL2] if entering fullscreen while in graphical effect mode, it could not be exited without visiting a textmode resolution.
 * [SDL2] Use SDL_OpenAudioDevice(), else the expected audio format between SDL2 and OCP might not be what we expect causing random noise to be played.
 * [X11] non-Shm usage could fail to successfully create butter on window resize
 * [X11] If background picture is loaded in GUI modes, it was not repainted on window-resize
 * [unifont] Allow for unifont ttf/otf files to be placed in the datadir by using ./configure --with-unifont-relative (you still need to copy the files in)
 * [*.VGZ] Silently convert them to *.VGM
 * [GME] Add support for Game Music Emulator library (libgme) for playback of various retro console systems

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
%{_prefix}/bin/ocp-sdl2
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
