# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 0.2.100

# Default to _with_libmad if neither _with_libmad or _without_libmad is defined
%{!?_with_libmad: %{!?_without_libmad: %define _with_libmad --with-libmad}}

# Error if both _with_libmad and _without_libmad is defined
%{?_with_libmad: %{?_without_libmad: %{error: both _with_libmad and _without_libmad}}}

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
BuildRequires: ncurses-devel zlib-devel bzip2-devel libSDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files libjpeg62-turbo-devel libpng16-devel xa libdiscid-devel cjson-devel alsa-devel libfreetype2-devel gnu-unifont-bitmap-fonts
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel SDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libSDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts
%endif
%endif

# Include libmad if given
%{?_with_libmad:BuildRequires: libmad-devel}

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
 Changes from version 0.2.99 to 0.2.100:
 * Former attempt at supporting plugins to be built-in ("static") has been revived and is enabled by default. By now playback-plugins and audio/wavetable drivers must be external still.
 * Unifont is transitioning from truetype to opentype and OCP is adapting the change.
    ./configure syntax has been changed to accomodate the changes:
    If specify by directory, the options available are
      --with-unifontdir-ttf=/path
      --with-unifontdir-otf=/path
    If specify by file (due to non-standard naming convensions and casing)
      --with-unifont-ttf=/path
      --with-unifont-csur-ttf=/path
      --with-unifont-upper-ttf=/path
      --with-unifont-otf=/path
      --with-unifont-csur-otf=/path
      --with-unifont-upper-otf=/path
 * Many internal functions used by playback-plugins are now being forwarded from core to plugins by API-tables.
 * Double-free in playopl (adplug playback) if file failed to load.
 * Move all file-type detection and type registration into each playback plugin, and autoload all playback plugins.
 * In fileselector file-type-editor, selecting a blank file type (redetect file-type), buffer overflowed and relied on that being blank.
 * Avoid crashing when opening a midi file due to missing parameter
 * 16bit WAV files were probably not able to play on big-endian systems
 * FLAC: avoid divide by zero error at end of tracks
 * Timidity/MIDI: isolate all library globals into a session and partially fixing channel muting.
 * Internal API changes
 * ALSA: Make it possible to custom select with text input both PCM output device and Mixer, and bring volume mixer back to life
 * Information about selected channel disappeared from the screen when in scope, phase and note dots view if you visited the file browser.
 * Add international keyboard support for SDL 1.x
 * Add international keyboard support for X11.
 * quickfind now supports international characters.
 * cpiinst overshot memory when clearing unused space.
 * Quicksearch .. and other overriden filenames in filebrowser where not working.
 * Update libsidplayfp to lastest upstream master

%prep
%setup -q -n %{name}-%{version}
unzip $RPM_SOURCE_DIR/opencp25image1.zip
unzip -o $RPM_SOURCE_DIR/opencp25ani1.zip

%build
CFLAGS=$RPM_OPT_FLAGS CXXFLAGS=$RPM_OPT_FLAGS ./configure --prefix=%{_prefix} --exec_prefix=%{_exec_prefix} --infodir=%{_infodir} --sysconfdir=/etc %{?_with_libmad} %{?_without_libmad}
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
mkdir -p %{buildroot}%{_prefix}/share/ocp-%{version}/data
cp CP* %{buildroot}%{_prefix}/share/ocp-%{version}/data
rm -f %{buildroot}/%{_infodir}/dir

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%config %{_prefix}/share/ocp-%{version}/etc/ocp.ini
%{_exec_prefix}/lib/ocp-%{version}
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

%dir %{_prefix}/share/ocp-%{version}
#%dir %{_prefix}/share/ocp-%{version}/data
%dir %{_prefix}/share/ocp-%{version}/etc
%{_prefix}/share/ocp-%{version}/data
#data/ocp.hlp
#data/CP*.TAG
#data/CP*.DAT

%docdir %{_prefix}/share/doc/ocp-%{version}
%{_prefix}/share/doc/ocp-%{version}
