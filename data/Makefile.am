pixmapdir = $(pkgdatadir)

PIX = background.png dot.png folder.png foldermarker.png iconshadow.png

pixmap_DATA = $(PIX)

desktopdir = $(datadir)/applications
desktop_in_files = sbmanager.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(pixmap_DATA)	\
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_in_files)
