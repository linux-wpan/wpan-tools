// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
//
// SPDX-License-Identifier: ISC

/* IEEE 802.15.4 socket example */
/* gcc af_ieee802154_rx.c -o af_ieee802154_rx */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#define IEEE802154_ADDR_LEN 8
#define MAX_PACKET_LEN 127
#define EXTENDED 1

enum {
	IEEE802154_ADDR_NONE = 0x0,
	IEEE802154_ADDR_SHORT = 0x2,
	IEEE802154_ADDR_LONG = 0x3,
};

struct ieee802154_addr_sa {
	int addr_type;
	uint16_t pan_id;
	union {
		uint8_t hwaddr[IEEE802154_ADDR_LEN];
		uint16_t short_addr;
	};
};

struct sockaddr_ieee802154 {
	sa_family_t family;
	struct ieee802154_addr_sa addr;
};

int main(int argc, char *argv[]) {
	int ret, sd;
	struct sockaddr_ieee802154 src, dst;
	unsigned char buf[MAX_PACKET_LEN + 1];
	socklen_t addrlen;
	/* IEEE 802.15.4 extended address to receive frames on, adapt to your setup */
	uint8_t long_addr[IEEE802154_ADDR_LEN] = {0xd6, 0x55, 0x2c, 0xd6, 0xe4, 0x1c, 0xeb, 0x57};

	/* Create IEEE 802.15.4 address family socket for the SOCK_DGRAM type */
	sd = socket(PF_IEEE802154, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("socket");
		return 1;
	}

	/* Prepare source socket address struct */
	memset(&src, 0, sizeof(src));
	src.family = AF_IEEE802154;
	/* Used PAN ID is 0x23 here, adapt to your setup */
	src.addr.pan_id = 0x0023;

#if EXTENDED /* IEEE 802.15.4 extended address usage */
	src.addr.addr_type = IEEE802154_ADDR_LONG;
	memcpy(&src.addr.hwaddr, &long_addr, IEEE802154_ADDR_LEN);
#else
	src.addr.addr_type = IEEE802154_ADDR_SHORT;
	src.addr.short_addr = 0x0002;
#endif

	/* Bind socket on this side */
	ret = bind(sd, (struct sockaddr *)&src, sizeof(src));
	if (ret) {
		perror("bind");
		close(sd);
		return 1;
	}

	addrlen = sizeof(dst);

	/* Infinite loop receiving 802.15.4 frames and print out */
	while (1) {
		ret = recvfrom(sd, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)&dst, &addrlen);
		if (ret < 0) {
			perror("recvfrom");
			continue;
		}
		buf[ret] = '\0';
#if EXTENDED
		printf("Received (from %s): %s\n", dst.addr.hwaddr, buf);
#else
		printf("Received (from %x): %s\n", dst.addr.short_addr, buf);
#endif
	}

	shutdown(sd, SHUT_RDWR);
	close(sd);
	return 0;
}
