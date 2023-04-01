#-*- mode: makefile.am; coding:utf-8 -*-
#
#
bin_PROGRAMS = sos
noinst_LIBRARIES = lib-sos-common.a
lib_sos_common_a_SOURCES = dio.c storage.c disk-2d.c fs-sword.c fs-utils.c compat.c \
	misc.c list.c queue.c
sos_CPPFLAGS = -DVERSION=\"${VERSION}\" -DDATADIR=\"$(pkgdatadir)\"
sos_CFLAGS = ${NCURSES_CFLAGS}
sos_SOURCES = sos.c simz80.c util.c trap.c screen.c keymap.c
sos_LDADD =  lib-sos-common.a ${NCURSES_LIBS}
