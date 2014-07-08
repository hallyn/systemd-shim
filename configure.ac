AC_INIT([systemd-shim], [6])
AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])
AC_PROG_CC
PKG_CHECK_MODULES(gio, gio-2.0)

AC_ARG_ENABLE([cgmanager],
	[AC_HELP_STRING([--enable-cgmanager], [enable cgmanager support [default=auto]])],
	[], [enable_cgmanager=auto])

if test "x$enable_cgmanager" = "xauto" ; then
	AC_CHECK_LIB([cgmanager],[cgmanager_create],[enable_cgmanager=yes],[enable_cgmanager=no],[-lnih -lnih-dbus -ldbus-1])
fi
AM_CONDITIONAL([ENABLE_CGMANAGER], [test "x$enable_cgmanager" = "xyes"])

AM_COND_IF([ENABLE_CGMANAGER],
	[PKG_CHECK_MODULES([CGMANAGER], [libcgmanager])
	PKG_CHECK_MODULES([NIH], [libnih >= 1.0.2])
	PKG_CHECK_MODULES([NIH_DBUS], [libnih-dbus >= 1.0.0])
	PKG_CHECK_MODULES([DBUS], [dbus-1 >= 1.2.16])
	])

AC_CONFIG_FILES([Makefile
                 data/Makefile
                 src/Makefile])
AC_OUTPUT
