AM_CPPFLAGS =								\
	-DG_LOG_DOMAIN=\"xfce4-wavelan-plugin\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = libwavelan.la

libwavelan_la_LIBADD =							\
	$(LIBM)								\
	@GLIB_LIBS@							\
	@GTK_LIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UI_LIBS@						\
	@LIBXFCE4UTIL_LIBS@

libwavelan_la_SOURCES =							\
	wavelan.c							\
	wi.h								\
	wi_bsd.c							\
	wi_common.c							\
	wi_darwin.c							\
	wi_linux.c

libwavelan_la_CFLAGS =							\
	@GLIB_CFLAGS@							\
	@GTK_CFLAGS@							\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UI_CFLAGS@						\
	@LIBXFCE4UTIL_CFLAGS@

libwavelan_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

# .desktop file
desktop_in_files = wavelan.desktop.in

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files) Makefile
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)
