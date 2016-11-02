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

/* gcc af_inet6_rx.c -o af_inet6_rx */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_PACKET_LEN 2048

int main(int argc, char *argv[]) {
	int ret, sd;
	ssize_t len;
	struct sockaddr_in6 src, dst;
	unsigned char buf[MAX_PACKET_LEN + 1];
	socklen_t addrlen;
	char ipv6[INET6_ADDRSTRLEN];

	/* Create IPv6 address family socket for the SOCK_DGRAM type */
	sd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("socket");
		return 1;
	}

	/* Prepare source and destination socket address structs */
	memset(&dst, 0, sizeof(dst));
	memset(&src, 0, sizeof(src));
	src.sin6_family = AF_INET6;
	src.sin6_addr = in6addr_any;
	/* Port within the compressed port range for potential NHC UDP compression */
	src.sin6_port = htons(61617);

	/* Bind socket on this side */
	ret = bind(sd, (struct sockaddr *)&src, sizeof(src));
	if (ret) {
		perror("bind");
		close(sd);
		return 1;
	}

	addrlen = sizeof(dst);

	/* Infinite loop receiving IPv6 packets and print out */
	while (1) {
		len = recvfrom(sd, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)&dst, &addrlen);
		if (len < 0) {
			perror("recvfrom");
			continue;
		}
		buf[len] = '\0';
		inet_ntop(AF_INET6, &(dst.sin6_addr), ipv6, INET6_ADDRSTRLEN);
		printf("Received (from %s): %s\n", ipv6, buf);
	}

	shutdown(sd, SHUT_RDWR);
	close(sd);
	return 0;
}
