###############################################################
#
# Spec file for package gnubg (Version 0.14).
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
Version:      0.15
Release:      4 
Source:       %{name}-%{version}.tar.gz
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
URL:          http://www.gnubg.org


%description
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

%define prefix /usr


%prep
%setup

%build

./autogen.sh
./configure --prefix=%{prefix} \
            --with-python \
	    --infodir=${RPM_BUILD_ROOT}%{prefix}/share/info \
	    --mandir=${RPM_BUILD_ROOT}%{prefix}/share/man \
            --without-gdbm \
	    --without-guile \
	    --with-board3d \
	    --enable-simd
	    
make  

%install
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

# create this dir empty so we can own it
mkdir -p ${RPM_BUILD_ROOT}%{_datadir}/aclocal
rm -f $RPM_BUILD_ROOT%{_infodir}/dir
 
%post
/sbin/install-info %{_infodir}/gnubg.info.gz %{_infodir}/dir
 
%preun
if [ $1 = 0 ]; then
    /sbin/install-info --delete %{_infodir}/gnubg.info.gz %{_infodir}/dir
fi

%clean
rm -rf ${RPM_BUILD_ROOT}


%files
%defattr(-,root,root)
# %doc AUTHORS README COPYING ChangeLog
%{prefix}/bin/*
%{prefix}/share/info/*
%{prefix}/share/gnubg/doc/*
%{prefix}/share/gnubg/flags/*
%{prefix}/share/gnubg/fonts/*
%{prefix}/share/gnubg/met/*
%{prefix}/share/gnubg/scripts/*
#%{prefix}/share/gnubg/textures/*
#%{prefix}/share/gnubg/*.png
%{prefix}/share/gnubg/boards.xml
%{prefix}/share/gnubg/gnubg.*
#%{prefix}/share/gnubg/texinfo.dtd
#%{prefix}/share/gnubg/textures*
%{prefix}/share/locale/*/*/*
%{prefix}/share/man/*/*

%files databases
%{prefix}/share/gnubg/*.bd

%files sounds
%{prefix}/share/gnubg/sounds/*


%changelog -n gnubg
* Sat Nov 18 2006 - <ace@gnubg.org>
- some slight changes in specfile

* Wed Dec 28 2004 - <ace@gnubg.org>
- new weights including pruning

* Mon Oct 11 2004 - <ace@gnubg.org>
- fixed some minor bugs

* Wed Sep 01 2004 - <ace@gnubg.org>
- new rpms with 3d enabled

* Wed Nov 05 2003 - <ace@gnubg.org>
- made the spec suit to redhat and suse <ace@gnubg.org>
- disabled 3d (still problems with nvidia)
- added gpg signature

* Thu Oct 23 2003 - <ace@gnubg.org>
- disabled gdbm and guile
- changed info- and manpath

* Mon Oct 20 2003 - <ace@gnubg.org>
- divided into three packages (gnubg, databases, sounds)

* Fri Oct 18 2003 - <ace@gnubg.org>
- initial package (Version 0.14)
