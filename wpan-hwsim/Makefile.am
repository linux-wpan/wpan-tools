# SPDX-FileCopyrightText: 2018 Alexander Aring <aring@mojatatu.com>
#
# SPDX-License-Identifier: ISC

bin_PROGRAMS = wpan-hwsim

wpan_hwsim_SOURCES = wpan-hwsim.c \
		     mac802154_hwsim.h

wpan_hwsim_CFLAGS = $(AM_CFLAGS) $(LIBNL3_CFLAGS)
wpan_hwsim_LDADD = $(LIBNL3_LIBS)
