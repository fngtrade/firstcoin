
Debian
====================
This directory contains files used to package firstcoind/firstcoin-qt
for Debian-based Linux systems. If you compile firstcoind/firstcoin-qt yourself, there are some useful files here.

## firstcoin: URI support ##


firstcoin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install firstcoin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your firstcoin-qt binary to `/usr/bin`
and the `../../share/pixmaps/firstcoin128.png` to `/usr/share/pixmaps`

firstcoin-qt.protocol (KDE)

