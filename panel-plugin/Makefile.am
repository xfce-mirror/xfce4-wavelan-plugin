AM_CPPFLAGS =								\
	-DG_LOG_DOMAIN=\"xfce4-wavelan-plugin\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = libwavelan.la

libwavelan_la_LIBADD =							\
				$(LIBM)						\
				@LIBXFCE4PANEL_LIBS@				\
				@LIBXFCE4UI_LIBS@

libwavelan_la_SOURCES =							\
	wavelan.c							\
	wi.h								\
	wi_bsd.c							\
	wi_common.c							\
	wi_linux.c

libwavelan_la_CFLAGS =							\
	@LIBXFCE4PANEL_CFLAGS@							\
	@LIBXFCE4UI_CFLAGS@

libwavelan_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

# .desktop file
#
# We need to let intltool merge the translated fields, so we add a
# level of indirection: a <name>.desktop.in file.
# 
desktop_in_files = wavelan.desktop.in

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)
