dnl Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
dnl Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
dnl

dnl Version information
XDT_VERSION_INIT([0.6.3], [git])

dnl Initial autoconf
AC_COPYRIGHT([Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
2010-2023 The Xfce development team. All rights reserved.])
AC_INIT([xfce4-wavelan-plugin], [xdt_version], [https://gitlab.xfce.org/panel-plugins/xfce4-wavelan-plugin])
AC_REVISION([xdt_version_build])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])

dnl Initial automake
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Check for basic programs
AC_PROG_CC()
AC_PROG_INSTALL()

LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl Check for standard header files
AC_CHECK_HEADERS([math.h stdio.h stdlib.h string.h sys/types.h \
                  sys/socket.h sys/ioctl.h linux/types.h linux/if.h \
                  linux/wireless.h string.h \
                  ctype.h], [], [], [#include <sys/socket.h>])

LT_LIB_M
AC_SUBST(LIBM)

dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl configure the panel plugin
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.16.0])

dnl Check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
panel-plugin/Makefile
po/Makefile.in
])
AC_OUTPUT
