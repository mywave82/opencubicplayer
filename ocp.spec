# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 3.3.1

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
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files libjpeg62-turbo-devel libpng16-devel xa libdiscid-devel cjson-devel alsa-devel libfreetype2-devel gnu-unifont-bitmap-fonts libgme-devel ancient-devel speex-devel
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel SDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel game-music-emulator-devel speex-devel
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel libgme-devel speex-devel
%endif
%endif
Requires: curl

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
Changes from version 3.3.0 to 3.3.1:

[FLAC]
 * Multiple metra-entries with same key caused crash.

[SNDH]
 * Add missing MIME type: audio/x-sndh

[HVL, OGG, TIMIDITY]
 * Used plrDevAPI->Idle() that returns available free space, instead of plrDevAPI->GetStats() to retrieve the current audio-delay.

[TIMIDITY]
 * Harden logic that delays events for UI.


Changes from version 3.2.3 to 3.3.0:

[SID]
 * Update libsidplayfp to v3.0.0
 * Update libresidfp to latest master
 * Adjust keyboard repeat detection for accelerating adjusting the filters.
 * enableOld6581caps option for C64 Assembly 326298
 * Fix potential crashes in 'o' and 'b' modes.

[ULT]
 * Adjust cmdVolSlide to be 4 times as powerful. Also add minor missing effects/commands.

[SNDH]
 * Add support for SNDH files using psgplay

[XM]
 * Give warning if tune contains non-standard non-supported ADPCM sample
 * After a ECx, a note without an instrument should unmute the channel. (#176)

[HVL]
 * Fix potential crashes in 'o' and 'b' modes.

[modland.com]
 * no longer hosts playsid directory, and update the list of unknown directories

[channel viewer]
 * a DC bias should not produce volume-bars active

[YM]
 * Update register to frequency logic to match SNDH. (Fixes a bug where the HI part of the registers ended up being ignored)

[nCurses]
 * Try to support rxvt styled terminals regarding SHIFT+F(n) keys.
 * Do not purge keyboard input buffer on conRestore/conSave
 * Move graphic refresh to its own timer callback instead of keyboard pull/read for a more consistent responsivness. Implemented for for nCurses, X11 and SDL.

[X11]
 * allow to enable/disable X11 SHM extension API during ./configure (for docker container)
 * Do not purge keyboard input buffer on conRestore/conSave
 * Move graphic refresh to its own timer callback instead of keyboard pull/read for a more consistent responsivness. Implemented for for nCurses, X11 and SDL.

[SDL]
 * With SDL3, scrollwheel now works as UP/DOWN keys - usefull in filebrowser.
 * install icons when building with SDL3.
 * Do not purge keyboard input buffer on conRestore/conSave
 * Move graphic refresh to its own timer callback instead of keyboard pull/read for a more consistent responsivness. Implemented for for nCurses, X11 and SDL.

[mingw]
 * update support-libraries to latest versions.
 * devpdisk, file-creation failed due to filename being in LFN syntax, but without any drive and directory.

[QOA]
 * Update to latest git (no impact for OCP)

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
