INCLUDES = \
	-DG_LOG_DOMAIN=\"Nautilus-SBManager\" \
	-DDATADIR=\"$(datadir)\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-Werror \
	$(libglib2_CFLAGS) \
	$(libgtk_CFLAGS) \
	$(libnautilusextension_CFLAGS)

nautilus_extensiondir=$(NAUTILUS_EXTENSION_DIR)

nautilus_extension_LTLIBRARIES=libnautilus-sbmanager.la

libnautilus_sbmanager_la_SOURCES = \
	nautilus-sbmanager.h \
	nautilus-sbmanager.c \
	nautilus-sbmgr.c

libnautilus_sbmanager_la_LDFLAGS = -module -avoid-version
libnautilus_sbmanager_la_LIBADD  = $(libglib2_LIBS) $(libgtk_LIBS) $(libnautilusextension_LIBS)
