// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
//
// SPDX-License-Identifier: ISC

/* IEEE 802.15.4 socket example */
/* gcc af_inet6_tx.c -o af_inet6_tx */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>

#define IEEE802154_ADDR_LEN 8
#define MAX_PACKET_LEN 2048

int main(int argc, char *argv[]) {
	int ret, sd;
	struct sockaddr_in6 dst;
	struct ifreq ifr;
	char buf[MAX_PACKET_LEN + 1];

	/* Create IPv6 address family socket for the SOCK_DGRAM type */
	sd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("socket");
		return 1;
	}

	/* Bind the socket to lowpan0 to make sure we send over it, adapt to your setup */
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lowpan0");
	ret = setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr));
	if (ret < 0) {
		perror("setsockopt");
		return 1;
	}

	/* Prepare destination socket address struct */
	memset(&dst, 0, sizeof(dst));
	dst.sin6_family = AF_INET6;
	/* Port within the compressed port range for potential NHC UDP compression */
	dst.sin6_port = htons(61617);
	inet_pton(AF_INET6, "ff02::1", &(dst.sin6_addr));

	sprintf(buf, "Hello world from AF_INET6 socket example!");

	/* sendto() is used for implicity in this example, bin()/send() would
	 * be an alternative */
	ret = sendto(sd, buf, strlen(buf), 0, (struct sockaddr *)&dst, sizeof(dst));
	if (ret < 0) {
		perror("sendto");
	}

	shutdown(sd, SHUT_RDWR);
	close(sd);
	return 0;
}
