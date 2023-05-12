# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 0.2.104

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
 Changes from version 0.2.103 to 0.2.104:

 * Update libsidplayfp
 * Update adplug
   * Latest version of the upstream version
     * Adds support for *.PIS and *.MTR
   * Reimplemented the OPL2/3 status viewer
   * Buffer for compositing tracker data for music that supports this, was not
     cleared between pattern loads. Causing visual data to be accumulated.
   * OCP now supports multiple of the emulator implementations
   * Default emulator to use has been changed
   * Configuration dialog added into setup:
 * Elapsed time is now based on played samples and not counting seconds passed
 * Screen resizing should be more consistent on remembering settings
 * Add support for 3-bytes-per-pixel in SDL 1.x
 * Files that are detected as valid for libancient but fails decompression,
   OCP failed to reset the filehandler read-position back to 0.
 * Track viewer had some excessive CPU usage
 * Analyzer viewer has the scale gain range increased, and the current gain is
   visible in the header.
 * Quick help documentation has been updated, with special focus on the keyboard
   shortcuts.

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
