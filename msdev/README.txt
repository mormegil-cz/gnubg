
Compile steps
=============

1) Copy config.h.msdev to config.h in gnubg directory
	(backup your original config.h file if necessary).

2) Define a new environment variable MINGW to point to mingw base directory.
	E.g. MINGW=c:\mingw

3) Create a msdev directory in mingw/include.
	Copy the gdk and gtk parent include directories here.
	Copy gdbm.h from mingwinclude sub directory.
	Copy the gtkgl directory here too.

4) Open gnubg.dsw in ms studio and build.

Notes
=====

1) png and libart and others
need to be built by msdev, for simplicity just compile without these (remove from config.h).

2) gnubg.exe will be produced in \program files\gnubg.exe, you'll probably want to change this.
