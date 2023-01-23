# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 0.2.102

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
BuildRequires: ncurses-devel zlib-devel bzip2-devel libSDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files libjpeg62-turbo-devel libpng16-devel xa libdiscid-devel cjson-devel alsa-devel libfreetype2-devel gnu-unifont-bitmap-fonts ancient
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel SDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libSDL2-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient
%endif
%endif

# Include libmad if given
%{?_with_libmad:BuildRequires: libmad-devel}

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
 Changes from version 0.2.101 to 0.2.102:
 * IT files did not detect reverse jumps are song being looped
 * IT playback plugin did not reset all state variables on load
 * XDG Base Directory compliance
   * Comply with both $XDG_CONFIG_HOME and $XDG_DATA_HOME
   * Migrate $HOME/.ocp/
 * Add support for files compressed on Amiga systems with the system built-in
   compression routines using the library known as 'ancient'
 * Updates for building on Haiku
 * If iconv CP437 fails to load, fall back to CP850 and then ASCII
 * Some few calls to iconv() were not protected against "NULL"
 * Add MIME for entries missing in the freedesktop MIME database
 * Update desktop file with additional MIME types
 * Call update_mime_database and update_desktop_database
 * Starting ocp with files as arguments stopped no loger was working
 * If a file fails to load, display error message in the fileselector
 * Replace setup:/alsa/*.dev files with a single setup:/alsaconfig.dev dialog
 * nprintf() didn't limit UTF-8 strings correctly
 * Only accept .TAR files that contains the ustar magic
 * If playback plugin are not operational, multiple corner-case issues has now
   been fixed
 * When editing fixed UTF-8 text-fields, backspace / delete-key would not
   unreserve the buffer-space, artificailly shrinking the available text until
   a new edit was initialized
 * Add the rReverb and iReverb plugins from the original DOS project, with some
   additional fixes
 * Show both panning/balance and chorus/reverb at the same time if they both can
   be active and can fit on screen

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
