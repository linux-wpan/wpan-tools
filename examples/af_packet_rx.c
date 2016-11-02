/*
 * IEEE 802.15.4 socket example
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * Author: Stefan Schmidt <stefan@osg.samsung.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* gcc af_packet_rx.c -o af_packet_rx */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <linux/if_packet.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>

#define IEEE802154_ADDR_LEN 8
#define MAX_PACKET_LEN 127

#ifndef ETH_P_IEEE802154
#define ETH_P_IEEE802154 0x00F6
#endif

int main(int argc, char *argv[]) {
	int ret, sd, i;
	ssize_t len;
	unsigned char buf[MAX_PACKET_LEN + 1];
	struct sockaddr_ll sll;
	struct ifreq ifr;

	/* Create AF_PACKET address family socket for the SOCK_RAW type */
	sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IEEE802154));
	if (sd < 0) {
		perror("socket");
		return 1;
	}

	/* Get interface index */
	strncpy(ifr.ifr_name, "monitor0", IFNAMSIZ);
	ret = ioctl(sd, SIOCGIFINDEX, &ifr);
	if (ret < 0) {
		perror("ioctl");
		close(sd);
		return 1;
	}

	/* Prepare source socket address struct */
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons(ETH_P_IEEE802154);

	/* Bind socket on this side */
	ret = bind(sd, (struct sockaddr *)&sll, sizeof(sll));
	if (ret < 0) {
		perror("bind");
		close(sd);
		return 1;
	}

	while (1) {
		/* Receive and print the whole packet payload, including FCS */
		len = recv(sd, buf, MAX_PACKET_LEN, 0);
		if (len < 0) {
			perror("recv");
			continue;
		}
		printf("Received:");
		for (i = 0; i < len; i++)
			printf(" %x", buf[i]);
		printf("\n");
	}

	shutdown(sd, SHUT_RDWR);
	close(sd);
	return 0;
}
