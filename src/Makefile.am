# SPDX-FileCopyrightText: 2014 Alexander Aring <alex.aring@gmail.com>
#
# SPDX-License-Identifier: ISC

bin_PROGRAMS = \
	iwpan

iwpan_SOURCES = \
	iwpan.c \
	iwpan.h \
	sections.c \
	info.c \
	interface.c \
	phy.c \
	mac.c \
	scan.c \
	event.c \
	nl_extras.h \
	nl802154.h

iwpan_CFLAGS = $(AM_CFLAGS) $(LIBNL3_CFLAGS)
iwpan_LDADD = $(LIBNL3_LIBS)
