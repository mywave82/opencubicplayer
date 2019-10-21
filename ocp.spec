# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 0.1.22

# Default to _with_libmad if neither _with_libmad or _without_libmad is defined
%{!?_with_libmad: %{!?_without_libmad: %define _with_libmad --with-libmad}}

# Error if both _with_libmad and _without_libmad is defined
%{?_with_libmad: %{?_without_libmad: %{error: both _with_libmad and _without_libmad}}}

Name: %{name}
Version: %{version}
Release: 0
Summary: Linux port of Open Cubic Player
Group: Applications/Multimedia
URL: http://stian.cubic.org/coding-ocp.php
Buildroot: /var/tmp/ocp-buildroot
Source0: http://stian.cubic.org/ocp/%{name}-%{version}.tar.bz2
Source1: ftp://ftp.cubic.org/pub/player/gfx/opencp25image1.zip
Source2: ftp://ftp.cubic.org/pub/player/gfx/opencp25ani1.zip
License: GPL-2, Creative Commons Attribution 3.0
# OCP itself is GPL-2
# The extra data provided is Creative Commons Attribute 3.0

%if 0%{?suse_version}
#suse doesn't have libXpm
BuildRequires: ncurses-devel zlib-devel libadplug-devel libSDL-devel libogg-devel libvorbis-devel libsidplay1-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme unzip texinfo update-desktop-files
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: ncurses-devel zlib-devel adplug-devel SDL-devel libogg-devel libvorbis-devel libsidplay-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme libXpm-devel unzip texinfo
%else
BuildRequires: ncurses-devel zlib-devel adplug-devel libSDL-devel libogg-devel libvorbis-devel libsidplay-devel gcc >= 3.0-0 gcc-c++ >= 3.0-0 flac-devel desktop-file-utils hicolor-icon-theme libXpm-devel unzip texinfo
%endif
%endif

# Include libmad if given
%{?_with_libmad:BuildRequires: libmad-devel}

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
* Sat Jan 09 2010 - stian (at) nixia.no
 - Initial makeover of the .spec file

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
# Assembler optimizations for x86 requires relocations, so please tell SELinux this if possible
%ifarch i386 i486 i586 i686 x86
if [ -x /usr/bin/chcon ]; then
	chcon -t textrel_shlib_t %{_exec_prefix}/lib/ocp-%{version}/devwmix.so %{_exec_prefix}/lib/ocp-%{version}/devwmixf.so %{_exec_prefix}/lib/ocp-%{version}/autoload/10-mixclip.so %{_exec_prefix}/lib/ocp-%{version}/autoload/30-mcpbase.so
fi
%endif

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
%{_prefix}/bin/ocp-%{version}
%{_prefix}/bin/ocp
%{_prefix}/bin/ocp-curses
%{_prefix}/bin/ocp-sdl
%{_prefix}/bin/ocp-vcsa
%{_prefix}/bin/ocp-x11
%{_infodir}/ocp.info.gz
%{_prefix}/share/icons/hicolor/16x16/apps/opencubicplayer.xpm
%{_prefix}/share/icons/hicolor/48x48/apps/opencubicplayer.xpm
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
