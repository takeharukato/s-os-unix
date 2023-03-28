#-*- mode: makefile.am; coding:utf-8 -*-
#
#
bin_PROGRAMS = sos tst-swd-fs1.out
COMMON=dio.c storage.c disk-2d.c fs-sword.c fs-utils.c compat.c \
	misc.c list.c queue.c
sos_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
sos_CFLAGS = ${NCURSES_CFLAGS}
sos_SOURCES = sos.c simz80.c util.c trap.c screen.c keymap.c ${COMMON}
sos_LDADD =  ${NCURSES_LIBS}

tst_swd_fs1_out_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
tst_swd_fs1_out_CFLAGS = ${NCURSES_CFLAGS}
tst_swd_fs1_out_SOURCES = tst-swd-fs1.c ${COMMON}

tst_swd_fs1_out_LDADD =  ${NCURSES_LIBS}
