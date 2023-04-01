
noinst_PROGRAMS = tst-swd-fs1.out tst-swd-fs2.out

COMMON=$(top_srcdir)/src/dio.c $(top_srcdir)/src/storage.c $(top_srcdir)/src/disk-2d.c $(top_srcdir)/src/fs-sword.c $(top_srcdir)/src/fs-utils.c $(top_srcdir)/src/compat.c \
$(top_srcdir)/src/misc.c $(top_srcdir)/src/list.c $(top_srcdir)/src/queue.c

tst_swd_fs1_out_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
tst_swd_fs1_out_CFLAGS = ${NCURSES_CFLAGS}
tst_swd_fs1_out_SOURCES = tst-swd-fs1.c ${COMMON}

tst_swd_fs2_out_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
tst_swd_fs2_out_CFLAGS = ${NCURSES_CFLAGS}
tst_swd_fs2_out_SOURCES = tst-swd-fs2.c ${COMMON}

tst_swd_fs1_out_LDADD =  ${NCURSES_LIBS}
