# $Id: Makefile.am 694 2005-07-11 14:18:53Z benny $

INCLUDES =								\
	-DG_LOG_DOMAIN=\"xfce4-wavelan-plugin\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-wavelan-plugin

#plugin_LTLIBRARIES =							\
#	libwavelan.la

xfce4_wavelan_plugin_LDFLAGS =							\
				@LIBXFCE4PANEL_LIBS@

xfce4_wavelan_plugin_SOURCES =							\
	inline-icons.h							\
	wavelan.c							\
	wi.h								\
	wi_bsd.c							\
	wi_common.c							\
	wi_linux.c

xfce4_wavelan_plugin_CFLAGS =							\
	@LIBXFCE4PANEL_CFLAGS@

inline-icons.h: $(srcdir)/error.png $(srcdir)/link0.png $(srcdir)/link1.png $(srcdir)/link2.png $(srcdir)/link3.png $(srcdir)/link4.png $(srcdir)/link5.png
	gdk-pixbuf-csource --raw --build-list error_icon_data error.png link0_icon_data link0.png link1_icon_data link1.png link2_icon_data link2.png link3_icon_data link3.png link4_icon_data link4.png link5_icon_data link5.png > inline-icons.h

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
# 
desktop_in_in_files = wavelan.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 								\
				error.png						\
				link0.png						\
				link1.png						\
				link2.png						\
				link3.png						\
				link4.png						\
				link5.png						\
				$(desktop_in_in_files)

CLEANFILES =							\
				$(desktop_DATA) $(desktop_in_files)
                                
# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" \
		$< > $@


