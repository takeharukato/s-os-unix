#-*- mode: makefile.am; coding:utf-8 -*-
#
#
bin_PROGRAMS = sos tst-swd-fs1.out

sos_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
sos_CFLAGS = ${NCURSES_CFLAGS}
sos_SOURCES = sos.c simz80.c trap.c dio.c screen.c util.c keymap.c compat.c misc.c \
	list.c queue.c storage.c disk-2d.c
sos_LDADD =  ${NCURSES_LIBS}

tst_swd_fs1_out_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
tst_swd_fs1_out_CFLAGS = ${NCURSES_CFLAGS}
tst_swd_fs1_out_SOURCES = tst-swd-fs1.c storage.c disk-2d.c fs-sword.c \
		list.c queue.c compat.c misc.c
tst_swd_fs1_out_LDADD =  ${NCURSES_LIBS}
