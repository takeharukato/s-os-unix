dnl
dnl -*- coding:utf-8 mode:autoconf -*-
dnl
dnl

AC_PREREQ([2.71])
AC_INIT([S-OS],[0.5beta],[takeharu1219@ybb.ne.jp])
AM_INIT_AUTOMAKE([foreign 1.11])

AC_CONFIG_HEADERS(include/config.h)
AC_CONFIG_SRCDIR([src/sos.c])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_SED
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([NCURSES], [ncurses])
AC_SUBST(NCURSES_CFLAGS)
AC_SUBST(NCURSES_LIBS)

AC_CHECK_HEADERS(termios.h term.h unistd.h curses.h byteswap.h)
AC_CHECK_HEADERS(float.h iso646.h limits.h stdarg.h stdbool.h stddef.h stdint.h)
AC_CHECK_FUNCS(getcwd strdup strerror strtol strcasecmp)
AC_CHECK_FUNCS(cfmakeraw memmove sigprocmask)
AC_CHECK_FUNCS(fcntl)
AC_CHECK_FUNCS(strrchr memcmp)
AC_FUNC_MEMCMP

dnl special options

AC_ARG_WITH(delay,
[  --with-delay		Compile with exprement delayed write feature.],
[ case "$withval" in
  no)
    AC_MSG_RESULT(disabled delayed flush)
    ;;
  yes)
    AC_MSG_RESULT(enabled delayed flush)
    AC_DEFINE(OPT_DELAY_FLUSH)
    AC_DEFINE([OPT_DELAY_FLUSH], [], [enabled delayed flush])
    ;;
  *)
    AC_MSG_RESULT(enabled delayed flush: delay=$withval usec)
    AC_DEFINE([OPT_DELAY_FLUSH], [], [enabled delayed flush])
    AC_DEFINE_UNQUOTED(OPT_DELAY_FLUSH,$withval)
    ;;
  esac ]
[ AC_MSG_RESULT(disabled delayed flush)
]
)

AC_DEFINE_UNQUOTED([DOSFILE],["dos.bin"],[Sword dos module file.])

AC_ARG_WITH(rcfile,
[  --with-rcfile=file	define default rcfile.],
[ case "$withval" in
  yes)
    AC_MSG_RESULT(you must specify rcfile)
    ;;
  *)
    AC_MSG_RESULT(set startup file as $withval)
    AC_DEFINE_UNQUOTED([RCFILE],["$withval"],[default rcfile])
    ;;
  esac ]
[
  AC_DEFINE_UNQUOTED([RCFILE],["sos.ini"],[default rcfile])
]
)

AC_ARG_WITH(forceansi,
	[  --with-forceansi	assume as ansi terminal if tgetenv failed ],
	[ AC_DEFINE([OPT_DEFAULT_ANSI],[],[assume as ansi terminal if tgetenv failed.])
	  AC_MSG_RESULT(Assume as ansi terminal if tgetenv failed.)]
)

AC_ARG_WITH(wmkeymap,
[  --with-wmkeymap	set default control key Word Master like ],
[ AC_DEFINE([OPT_KEYMAP_WM],[],[set default control key Word Master like])
  AC_MSG_RESULT(Set default keymap to Word Master like)]
)

AC_HEADER_TIOCGWINSZ

AC_CONFIG_FILES([Makefile
	        include/Makefile
                src/Makefile
		share/Makefile])
AC_OUTPUT
