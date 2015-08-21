AC_PREREQ(2.60)
AC_INIT([Userspace tools for Linux IEEE 802.15.4 stack],
	[0.5], [linux-wpan@vger.kernel.org], [wpan-tools],
	[https://github.com/linux-wpan/wpan-tools])
AC_CONFIG_SRCDIR([src/iwpan.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([
	foreign
	1.11
	-Wall
	-Werror
	-Wno-portability
	silent-rules
	tar-pax
	dist-bzip2
	subdir-objects
])
AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])
LT_INIT([
	disable-static
	pic-only
])
AC_PREFIX_DEFAULT([/usr])

AC_PROG_SED
AC_PROG_MKDIR_P

PKG_CHECK_MODULES(LIBNL3, [libnl-3.0 >= 3.1 libnl-genl-3.0 >= 3.1])

AC_CHECK_FUNCS([ \
	__secure_getenv \
	secure_getenv\
])

WPAN_TOOLS_CFLAGS="\
-Wall \
-Wchar-subscripts \
-Wformat-security \
-Wmissing-declarations \
-Wmissing-prototypes \
-Wnested-externs \
-Wpointer-arith \
-Wshadow \
-Wsign-compare \
-Wstrict-prototypes \
-Wtype-limits \
"
AC_SUBST([WPAN_TOOLS_CFLAGS])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
        Makefile
	src/Makefile
	wpan-ping/Makefile
])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE $VERSION
        =====

        prefix:                 ${prefix}
        sysconfdir:             ${sysconfdir}

        compiler:               ${CC}
        cflags:                 ${CFLAGS}
        ldflags:                ${LDFLAGS}
])
