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
	wavelan.c							\
	wi.h								\
	wi_bsd.c							\
	wi_common.c							\
	wi_linux.c

xfce4_wavelan_plugin_CFLAGS =							\
	@LIBXFCE4PANEL_CFLAGS@

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

EXTRA_DIST = 			$(desktop_in_in_files)

CLEANFILES =			$(desktop_DATA) $(desktop_in_files)
                                
# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" \
		$< > $@


