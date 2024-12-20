# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 3.0.0

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
 Changes from version 0.2.109 to 3.0.0:

 * Update external libraries for mingw build to latest versions.
 * Add more magic numbers for up and comming version of ancient (decompression library for solid files).
 * SetMode() did not have paremeters defined in the prototype, not all users had parameters defined. Caused crashes one some combination of mode changes.
 * Update to latest version of libsidplayfp
 * Update to latest version of adplug
 * Add modland.com support directly from the file browser using a local copy of the file-list provided by modland.com.
 * Speed up filebrowser if an earlier scanned .tar.gz now has unscanned modules. The archive is not persistent open due to caching.
 * XM files would smash the stack on big-endian due to to loops had counter-limits in reverse order in endian-reversal code.
 * MacOS/CoreAudio: Add missing mutex locking in two API functions.
 * SDL/SDL2 audio: Add missing mutex locking in two API functions.
 * SDL2 audio: Use SDL_LockAudioDevice, SDL_UnlockAudioDevice and SDL_CloseAudioDevice SDL 2.x functions instead of legacy 1.x functions.
 * Do not attempt to divide by zero, if a song is reported as zero long.
 * Logic for buffersize in playtimidity (MIDI files) was not working as expected, especially on Windows.
 * If a file was unable to be accessed, pressing ENTER on it would cause a NULL-pointer dereference (Problem introduced in v0.2.102, adding support for ancient)
 * Attempting to load a defective S3M file could trigger two different issues. Do not cal mcpSet(), since we have not initialized the mcp device yet, and the que variable was no reset on to NULL after free causing a double free in this special use case.
 * When adding a directory-tree to the playlist, group the files by their owning directory, and sort each group of files alphabetically (strcasecmp).
 * Detect Sidplayer files as playable.
 * modland.com stores "Atari Digi-Mix" as *.mix instead of *.ym, so add that file-extension.
 * Add FEST as a valid 4-channel signature
 * Add "Atari STe/Falcon, Octalyser" CD61 and CD81 signatures
 * Add "M&K!" as a valid MOD signature. These files are likely "His Master's Noise"
 * Add support for Atari Falcon, Digital Tracker (MOD) files.
 * Avoid double free(), could occure if trying to load an invalid MOD file
 * Adjust MIME database, multiple of the magic searches were too aggressive.
 * detecting .BAM files with .[0-9][0-9][0-9] filenames
 * Remove adplugdb->wipe() call, it is not for freeing memory

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
