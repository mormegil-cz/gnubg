###############################################################
#
# spec file for package gnubg (Version 0.14)
#
# Copyright (c) 2003 Achim Mueller, Germany.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# please send bugfixes or comments to info@gnubg.org
#
###############################################################

Name:         gnubg
License:      GNU General Public License (GPL) - all versions
Group:        Amusements/Games/Board/Other
Packager:     <ace@gnubg.org>
Summary:      A backgammon game and analyser
Version:      0.14
Release:      3
Source:       %{name}-%{version}.tar.gz
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
URL:          http://gnubg.org

%define prefix /usr

%description


Authors:
--------
GNU Backgammon (@gnubg{}) is software for playing and analysing backgammon
positions, games and matches. It's based on a neural network. Although it
already plays at a very high level, it's still work in progress. You may
play GNU Backgammon using the command line or a graphical interface

Authors:
--------
Joseph Heled <joseph@gnubg.org>
Oystein Johansen <oystein@gnubg.org>
David Montgomery
Jim Segrave
Joern Thyssen <jth@gnubg.org>
Gary Wong <gtw@gnu.org>

%package databases
Summary:     Databases for gnubg
Requires:    %{name}
Group:        Amusements/Games/Board/Other

%description databases
This package contains the GNU Backgammon bearoff databases.

%package sounds
Summary:     Sounds for gnubg
Requires:    %{name}
Group:        Amusements/Games/Board/Other

%description sounds
This package contains the sounds for GNU Backgammon.

%prep
%setup

%build
export CFLAGS="$RPM_OPT_FLAGS"
./configure --prefix=%{prefix} \
            --with-python \
            --infodir=%{prefix}/share/info \
            --mandir=%{prefix}/share/man \
            --without-gdbm \
            --without-guile

make


%install
make DESTDIR=$RPM_BUILD_ROOT install
rm -f $RPM_BUILD_ROOT/usr/share/info/dir*

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%post
%install_info --info-dir=%{_infodir} %{_infodir}/%{name}.info.gz

%postun
%install_info_delete --info-dir=%{_infodir} %{_infodir}/%{name}.info.gz

%files
%defattr(-,root,root)
%doc AUTHORS README COPYING ChangeLog
%{prefix}/bin/*
%{prefix}/share/info/*
%{prefix}/share/gnubg/met/*
%{prefix}/share/gnubg/*.png
%{prefix}/share/gnubg/boards.xml
%{prefix}/share/gnubg/gnubg.*

%{prefix}/share/gnubg/texinfo.dtd
%{prefix}/share/gnubg/textures*
%{prefix}/share/locale/*/*/*
%{prefix}/share/man/*/*

%files databases
%{prefix}/share/gnubg/*.bd

%files sounds
%{prefix}/share/gnubg/sounds/*


%changelog -n gnubg
* Mon Oct 27 2003 - <ace@gnubg.org>
- info now fits (stuff is FHS conform)

* Thu Oct 23 2003 - <ace@gnubg.org>
- disabled gdbm and guile
- changed info- and manpath

* Mon Oct 20 2003 - <ace@gnubg.org>
- divided into three packages (gnubg, databases, sounds)

* Fri Oct 18 2003 - <ace@gnubg.org>
- initial package (Version 0.14)

