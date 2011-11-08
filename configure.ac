AC_INIT(mufs-fuse, 1.0)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AM_PROG_CC_C_O
LIBS=


export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

PKG_CHECK_MODULES(MUFS, [fuse >= 2.2 libcurl sqlite3 > 3.3])

CFLAGS="$CFLAGS -Wall -W"

oldlibs="$LIBS"
LIBS="$LIBS $MUFS_LIBS"
AC_CHECK_FUNC([fuse_opt_parse], [have_fuse_opt_parse=yes])
LIBS="$oldlibs"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
