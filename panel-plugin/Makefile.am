plugindir = @XFCE4_PANEL_PLUGINSDIR@

plugin_LTLIBRARIES =							\
	libwavelan.la

libwavelan_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-module

libwavelan_la_SOURCES =							\
	wavelan.c							\
	wi.h								\
	wi_bsd.c

libwavelan_la_CFLAGS =							\
	@XFCE4_PANEL_CFLAGS@

libwavelan_la_LIBADD = 							\
	@XFCE4_PANEL_LIBS@
