# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 3.5.0

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
BuildRequires: ncurses-devel zlib-devel libbz2-devel libmad-devel SDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg62-devel libpng16-devel xa libdiscid-devel cJSON-devel alsa-devel freetype2-devel gnu-unifont-otf-fonts libgme-devel ancient-devel speex-devel libopus-devel wavpack-devel
Requires: curl gnu-unifont-otf-fonts
%else
%if 0%{?fedora} || 0%{?rhel} || 0%{?centos}
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel SDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel freetype-devel unifont-fonts ancient-devel game-music-emu-devel speex-devel opus-devel wavpack-devel
Requires: curl unifont-fonts
%else
BuildRequires: ncurses-devel zlib-devel bzip2-devel libmad-devel libSDL3-devel libogg-devel libvorbis-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo libjpeg-turbo-devel libpng-devel xa libdiscid-devel cjson-devel alsa-lib-devel libfreetype-devel unifont-fonts ancient-devel libgme-devel speex-devel libopus-devel
Requires: curl unifont-fonts
%endif
%endif
Requires: curl

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
* Wed Aug 12 2026 Stian Skjelstad <stian.skjelstad@gmail.com> - 3.5.0
- [opus] Initial version
- [sidplayfp/residfp] Update to latest masters
- [sidplayfp/residfp] New tunable parameters:
- [sidplayfp/residfp] * Wave Offset 6581
- [sidplayfp/residfp] * DAC Leakage Level
- [sidplayfp/residfp] * DC Block Resistor
- [speex] Seek home did not work as expected

%prep
%setup -q -n %{name}-%{version}
unzip $RPM_SOURCE_DIR/opencp25image1.zip
unzip -o $RPM_SOURCE_DIR/opencp25ani1.zip

%build
%if 0%{?suse_version}
CFLAGS=$RPM_OPT_FLAGS CXXFLAGS=$RPM_OPT_FLAGS ./configure \
 --prefix=%{_prefix} \
 --exec_prefix=%{_exec_prefix} \
 --infodir=%{_infodir} \
 --sysconfdir=/etc \
 --with-unifont-otf=/usr/share/fonts/truetype/Unifont.otf \
 --with-unifont-csur-otf=/usr/share/fonts/truetype/Unifont_CSUR.otf \
 --with-unifont-upper-otf=/usr/share/fonts/truetype/Unifont_Upper.otf \
 --without-update-mime-database \
 --without-update-desktop-database
%else
CFLAGS=$RPM_OPT_FLAGS CXXFLAGS=$RPM_OPT_FLAGS ./configure \
 --prefix=%{_prefix} \
 --exec_prefix=%{_exec_prefix} \
 --infodir=%{_infodir} \
 --sysconfdir=/etc \
 --with-unifontdir-ttf=/usr/share/fonts/unifont \
 --with-unifontdir-otf=/usr/share/fonts/unifont \
 --without-update-mime-database \
 --without-update-desktop-database
%endif
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
cp CPPIC*.TGA CPANI* %{buildroot}/usr/share/ocp/data/
mkdir -p {buildroot}/usr/share/applications

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%config %{_prefix}/share/ocp/etc/ocp.ini
%if 0%{?suse_version}
%{_libdir}/ocp
%else
%{_exec_prefix}/lib/ocp
%endif
%{_prefix}/bin/ocp
%{_prefix}/bin/ocp-curses
%{_prefix}/bin/ocp-sdl3
%{_prefix}/bin/ocp-vcsa
%{_infodir}/ocp.info.gz
%{_mandir}/man1/ocp.1*
%{_prefix}/share/icons/hicolor/16x16/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/22x22/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/24x24/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/32x32/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/48x48/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/64x64/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/128x128/apps/opencubicplayer.png
%{_prefix}/share/icons/hicolor/scalable/apps/opencubicplayer.svg
%{_prefix}/share/applications/cubic.org-opencubicplayer.desktop
%{_prefix}/share/mime/packages/opencubicplayer.xml

%dir %{_prefix}/share/ocp
%{_prefix}/share/ocp/data

%docdir %{_prefix}/share/doc/ocp
%{_prefix}/share/doc/ocp
