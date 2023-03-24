#-*- mode: makefile.am; coding:utf-8 -*-
#
#
bin_PROGRAMS = sos

sos_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
sos_CFLAGS = ${NCURSES_CFLAGS}
sos_SOURCES = sos.c simz80.c trap.c dio.c screen.c util.c keymap.c compat.c misc.c \
	list.c queue.c storage.c disk-2d.c
sos_LDADD =  ${NCURSES_LIBS}
