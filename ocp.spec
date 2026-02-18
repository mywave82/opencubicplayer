# rpm spec file for RedHat / Fedora linux

%define name ocp
%define version 3.1.3

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
Requires: curl

%description
Open Cubic Player is a music player ported from DOS. Provides a nice text-based
frontend, with some few optional features in graphical. Plays modules, sids,
wave and mp3

%changelog
Changes from version 3.1.2 to 3.1.3:
 * [Windows] Update build dependencies to latest available releases
 * utf8_casefold()
   * Table for single- to single-point lookup is now done with a binarysearch,
     increasing the speed a lot when there are many files to add in the
     filebrowser view.
   * Update tables to Unicode 17
 * [zip] Remove void call to dirdbUnref()
 * [X11,SDL,SDL2,curses]
   * Unify the minimum text resolution to 80x20 (some had 80x25)
   * Gracefully ignore the physical size can know achieve the text resolution,
     and use virtual resolution of 80x20
   * Some few dialogs required 80x25, add scrolling
 * [Textlayout] Do not lock-up if window grows to smaller size than supported
   (easily happens when running in ncurses)
 * [DMF]
   * Support sample-header for both file-version <8 and >=8
   * Use correct buffer-size for input data if decompressing sample-data.
   * Reject patterns with more than 512 rows (not valid in the real editor)
   * When resizing order-list (splitting patterns with more than 256 rows into
     2), malloc() a buffer that is larger. (Old code performed buffer overflow)
   * Protect against buffer over/underflow when building the initial pattern-list
 * [HVL] Muting single channels, while displaying channel information caused crash
 * [669] Sample-looping did not work. Minor copy-paste error when the code was
   made endian-neutral.

Changes from version 3.1.1 to 3.1.2:
 * [QOA] Failed to compile on some systems.
 * [SID] Remove debug messages sent to the console
 * [FontSize control logic]
   * Was not consistent between SDL and X11 driver.
   * Was not possible to change away from 8x16 if window could not grow and fit
     16x32.
   * Help screen would reset font-size on X11. Now help screen follow to global
     selected font size instead.
 * [Windows]
   * OCP.INI update Messages: Do not use escape codes, use correct wide-char
     path and refer to `del` instead of `rm`.
   * Debug messages with paths for HomePath etc, now use wide-char paths.
   * Use wide-char version of fopen() when opening OCP.INI (support international
     user-names)
 * [MIDI]
   * Font-browser dialog had minor hickup in scrolling, and incorrect highlight
     for "No soundfont found".
   * When file wraps, do no free data and reload the file - reuse the already
     loaded data.
   * Remove double free() when attempting to load an invalid MIDI fil
 * [CURL]
   * Mention CURL in README.md and ocp.spec
   * Improve errors-messages if unable to launch the helper program

Changes from version 3.1.0 to 3.1.1:
 * [MIDI] loading files would cause crash (null dereference) if ~/.timidity.cfg
   not present
 * Avoid using extended SED syntax in stuff/Makefile

Changes from version 3.0.1 to 3.1.0:
 * Add 16x32 font, which is nice for high DPI screens. Access the setting via ALT-C.
 * [QOA] Add support for "Quite OK Audio" files
 * [IT]
   * Files created with other trackers than Impulse Tracker, was not marked
     correctly by file-detection.
   * Loading *.IT files with stereo samples caused random noise due to only
     reading one channel of the stereo sample causing multiple problems.
 * [devw]
   * Add support for playback of stereo-samples (instead of down-converting to
     mono)
   * If same sample could both be looped, and unlooped; minor curruption of data
     just after the loop-end could occure under special occations.
 * [S3M] commit 5732560 made stereo-commands pan aggressivly away from center
   (absolute + repeat relative) and "randomly" enable stereo if value was 1.
 * [XM] Make it possible to load songs that are partially truncated, missing some
   of the sample-data.
 * [modland.com] setup dialog:
   * select mirror: the custom editfield did not 100% match position when
     entering edit-mode.
   * cachedir: Add missing $ in the UI
 * Multiple updates regarding graphical modes:
   * Remove leftovers of "gdb-helpers"
   * Attemping to enable graphic mode needs to be able to gracefully fail, even
     when announced present (and revert back to text)
   * Changing fontsize, while graphical mode was enable did not have an impact on
     the textmodes.
   * Saving `fontsize` from ALT+C dialog did not work as expected, due to
     `screentype` likely being another value than 8 in ocp.ini. Update
     `screentype` value too when in this dialog to the relevant value.
   * SDL 1.x, enabling graphical mode two times in a row (and window size did not
     match from textmode), would cause the window to not resize correctly.
 * Remove a possible occation of strcpy where dst == src, which causes undefined
   behaviour.
 * [Windows]
   * Use WideChar / UTF-16 interface version of Windows filesystem API calls.
     This should make it possible to view file names that are using non-ASCII
     characters.
   * Make driver letters capital and sort them infront of the other protocols in
     the file browser.
 * [Timidity] Setup dialog for soundfonts:
   * When selecting soundfont / configfile, give hints in the bottom of the
     dialog about locations searched.
   * On Windows, add "*" to the end of the path when performing directory
     listings, to be compliant with the windows APIs.
   * On Windows, search $OCP\data for sf2 files.
   * On Windows, timidityplay.c didn't follow the TiMidity windows search logic.
   * If user-override only if found for timidity.cfg, relay this in the config
     dialog.
   * Add file-browser in the config-dialog. Making it possible to select SF2
     file outside the default search-scopes.
 * [adplug] Update to current master
 * [libsidplayfp] Update to current master
 * [MOD] MOD (Amiga ProTracker 1.1b), MODd (Dual Module Player) and MODt (old
   Amiga ProTracker) now use 8287Hz as the base-freqeuncy for samples matching
   Amiga PAL machines, while MODf (Fast Tracker II) still use 8363Hz. If a song
   now plays the samples slightly too fast, they should be marked with MODf
   (press <ALT>+<E> in the filebrowser, move the cursor over to "type:" and press
   <ENTER>)

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
