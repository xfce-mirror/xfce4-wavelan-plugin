# $Id$

INCLUDES =								\
	-DG_LOG_DOMAIN=\"xfce4-wavelan-plugin\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

plugindir = $(XFCE4_PANEL_PLUGINSDIR)
plugin_LTLIBRARIES =							\
	libwavelan.la

libwavelan_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-module

libwavelan_la_SOURCES =							\
	inline-icons.h							\
	wavelan.c							\
	wi.h								\
	wi_bsd.c							\
	wi_common.c							\
	wi_linux.c

libwavelan_la_CFLAGS =							\
	$(XFCE4_PANEL_CFLAGS)

libwavelan_la_LIBADD = 							\
	$(XFCE4_PANEL_LIBS)

inline-icons.h: $(srcdir)/error.png $(srcdir)/link0.png $(srcdir)/link1.png $(srcdir)/link2.png $(srcdir)/link3.png $(srcdir)/link4.png $(srcdir)/link5.png
	gdk-pixbuf-csource --raw --build-list error_icon_data error.png link0_icon_data link0.png link1_icon_data link1.png link2_icon_data link2.png link3_icon_data link3.png link4_icon_data link4.png link5_icon_data link5.png > inline-icons.h

EXTRA_DIST =								\
	error.png							\
	link0.png							\
	link1.png							\
	link2.png							\
	link3.png							\
	link4.png							\
	link5.png

# vi:set ts=8 sw=8 noet ai nocindent:
