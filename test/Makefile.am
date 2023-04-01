
noinst_PROGRAMS = tst-swd-fs1.out tst-swd-fs2.out

tst_swd_fs1_out_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
tst_swd_fs1_out_CFLAGS = ${NCURSES_CFLAGS}
tst_swd_fs1_out_SOURCES = tst-swd-fs1.c
tst_swd_fs1_out_LDADD = ../src/lib-sos-common.a ${NCURSES_LIBS}

tst_swd_fs2_out_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
tst_swd_fs2_out_CFLAGS = ${NCURSES_CFLAGS}
tst_swd_fs2_out_SOURCES = tst-swd-fs2.c
tst_swd_fs2_out_LDADD = ../src/lib-sos-common.a ${NCURSES_LIBS}
