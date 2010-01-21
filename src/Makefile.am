AM_CFLAGS =			\
	$(GLOBAL_CFLAGS)	\
	$(libiphone_CFLAGS)	\
	$(libglib2_CFLAGS)	\
	$(libgthread2_CFLAGS)	\
	$(libplist_CFLAGS)	\
	$(libclutter_CFLAGS)	\
	$(libcluttergtk_CFLAGS)	\
	$(libgtk_CFLAGS)	\
	$(libgdkpixbuf_CFLAGS)	\
	-DSBMGR_DATA=\"$(pkgdatadir)\"

AM_LDFLAGS =			\
	$(libiphone_LIBS)	\
	$(libglib2_LIBS)	\
	$(libgthread2_LIBS)	\
	$(libplist_LIBS)	\
	$(libclutter_LIBS)	\
	$(libcluttergtk_LIBS)	\
	$(libgtk_LIBS)		\
	$(libgdkpixbuf_LIBS)

bin_PROGRAMS = sbmanager

sbmanager_SOURCES = device.c \
					device.h \
					sbitem.c \
					sbitem.h \
					sbmanager.c
sbmanager_CFLAGS = $(AM_CFLAGS)
sbmanager_LDFLAGS = $(AM_LDFLAGS)

