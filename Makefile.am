## Process this file with automake to produce Makefile.in

bin_PROGRAMS = mufs

mufs_SOURCES = main.c log.c hash.c megaupload.c md5.c db.c


mufs_LDADD = $(MUFS_LIBS)
mufs_CFLAGS = $(MUFS_CFLAGS)
mufs_CPPFLAGS = -D_REENTRANT -DFUSE_USE_VERSION=26 -DLIBDIR=\"$(libdir)\"

