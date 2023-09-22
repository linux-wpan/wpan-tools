// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
//
// SPDX-License-Identifier: ISC

/* IEEE 802.15.4 socket example */
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
