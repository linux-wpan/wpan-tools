/*
 * Linux IEEE 802.15.4 ping tool
 *
 * Copyright (C) 2015 Stefan Schmidt <stefan@datenfreihafen.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

#include <netlink/netlink.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "../src/nl802154.h"

#define MIN_PAYLOAD_LEN 5
#define MAX_PAYLOAD_LEN 105 //116 with short address
#define IEEE802154_ADDR_LEN 8
/* Set the dispatch header to not 6lowpan for compat */
#define NOT_A_6LOWPAN_FRAME 0x00

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

#ifdef _GNU_SOURCE
static const struct option perf_long_opts[] = {
	{ "daemon", no_argument, NULL, 'd' },
	{ "address", required_argument, NULL, 'a' },
	{ "extended", no_argument, NULL, 'e' },
	{ "count", required_argument, NULL, 'c' },
	{ "size", required_argument, NULL, 's' },
	{ "interface", required_argument, NULL, 'i' },
	{ "version", no_argument, NULL, 'v' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, 0, NULL, 0 },
};
#endif

struct config {
	char packet_len;
	unsigned short packets;
	bool extended;
	bool server;
	char *interface;
	struct nl_sock *nl_sock;
	int nl802154_id;
	struct sockaddr_ieee802154 src;
	struct sockaddr_ieee802154 dst;
};

extern char *optarg;

void usage(const char *name) {
	printf("Usage: %s OPTIONS\n"
	"OPTIONS:\n"
	"--daemon |-d\n"
	"--address | -a server address\n"
	"--extended | -e use extended addressing scheme 00:11:22:...\n"
	"--count | -c number of packets\n"
	"--size | -s packet length\n"
	"--interface | -i listen on this interface (default wpan0)\n"
	"--version | -v print out version\n"
	"--help | -h this usage text\n", name);
}

static int nl802154_init(struct config *conf)
{
	int err;

	conf->nl_sock = nl_socket_alloc();
	if (!conf->nl_sock) {
		fprintf(stderr, "Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	nl_socket_set_buffer_size(conf->nl_sock, 8192, 8192);

	if (genl_connect(conf->nl_sock)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	conf->nl802154_id = genl_ctrl_resolve(conf->nl_sock, "nl802154");
	if (conf->nl802154_id < 0) {
		fprintf(stderr, "nl802154 not found.\n");
		err = -ENOENT;
		goto out_handle_destroy;
	}

	return 0;

out_handle_destroy:
	nl_socket_free(conf->nl_sock);
	return err;
}

static void nl802154_cleanup(struct config *conf)
{
	nl_close(conf->nl_sock);
	nl_socket_free(conf->nl_sock);
}

static int nl_msg_cb(struct nl_msg* msg, void* arg)
{
	struct config *conf = arg;
	struct sockaddr_nl nla;
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct nlattr *attrs[NL802154_ATTR_MAX+1];
	uint64_t temp;

	struct genlmsghdr *gnlh = (struct genlmsghdr*) nlmsg_data(nlh);

	nla_parse(attrs, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!attrs[NL802154_ATTR_SHORT_ADDR] || !attrs[NL802154_ATTR_PAN_ID]
	    || !attrs[NL802154_ATTR_EXTENDED_ADDR])
		return NL_SKIP;

	conf->src.family = AF_IEEE802154;
	conf->src.addr.pan_id = conf->dst.addr.pan_id = nla_get_u16(attrs[NL802154_ATTR_PAN_ID]);

	if (!conf->extended) {
		conf->src.addr.addr_type = IEEE802154_ADDR_SHORT;
		conf->src.addr.short_addr = nla_get_u16(attrs[NL802154_ATTR_SHORT_ADDR]);
	} else {
		conf->src.addr.addr_type = IEEE802154_ADDR_LONG;
		temp = htobe64(nla_get_u64(attrs[NL802154_ATTR_EXTENDED_ADDR]));
		memcpy(&conf->src.addr.hwaddr, &temp, IEEE802154_ADDR_LEN);
	}

	return NL_SKIP;
}

static int get_interface_info(struct config *conf) {
	struct nl_msg *msg;

	nl802154_init(conf);

	/* Build and send message */
	nl_socket_modify_cb(conf->nl_sock, NL_CB_VALID, NL_CB_CUSTOM, nl_msg_cb, conf);
	msg = nlmsg_alloc();
	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, conf->nl802154_id, 0, NLM_F_DUMP, NL802154_CMD_GET_INTERFACE, 0);
	nla_put_string(msg, NL802154_ATTR_IFNAME, "conf->interface");
	nl_send_sync(conf->nl_sock, msg);

	nl802154_cleanup(conf);
	return 0;
}

static void dump_packet(unsigned char *buf, int len) {
	int i;

	fprintf(stdout, "Packet payload:");
	for (i = 0; i < len; i++) {
		printf(" %x", buf[i]);
	}
	printf("\n");
}

static int generate_packet(unsigned char *buf, struct config *conf, unsigned int seq_num) {
	int i;

	buf[0] = NOT_A_6LOWPAN_FRAME;
	buf[1] = conf->packet_len;
	buf[2] = seq_num >> 8; /* Upper byte */
	buf[3] = seq_num & 0xFF; /* Lower byte */
	for (i = 4; i < conf->packet_len; i++) {
		buf[i] = 0xAB;
	}

	return 0;
}

static int print_address(char *addr, uint8_t dst_extended[IEEE802154_ADDR_LEN])
{
	snprintf(addr, 24, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", dst_extended[0],
		dst_extended[1], dst_extended[2], dst_extended[3], dst_extended[4],
		dst_extended[5], dst_extended[6], dst_extended[7]);
	return 0;
}

static int measure_roundtrip(struct config *conf, int sd) {
	unsigned char *buf;
	struct timeval start_time, end_time, timeout;
	long sec = 0, usec = 0;
	long sec_max = 0, usec_max = 0;
	long sec_min = 2147483647, usec_min = 2147483647;
	long sum_sec = 0, sum_usec = 0;
	int i, ret, count;
	unsigned short seq_num;
	float rtt_min = 0.0, rtt_avg = 0.0, rtt_max = 0.0;
	float packet_loss = 100.0;
	char addr[24];

	if (conf->extended)
		print_address(addr, conf->dst.addr.hwaddr);

	if (conf->extended)
		fprintf(stdout, "PING %s (PAN ID 0x%04x) %i data bytes\n",
			addr, conf->dst.addr.pan_id, conf->packet_len);
	else
		fprintf(stdout, "PING 0x%04x (PAN ID 0x%04x) %i data bytes\n",
			conf->dst.addr.short_addr, conf->dst.addr.pan_id, conf->packet_len);
	buf = (unsigned char *)malloc(MAX_PAYLOAD_LEN);

	/* 500ms seconds packet receive timeout */
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;
	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout,sizeof(struct timeval));

	count = 0;
	for (i = 0; i < conf->packets; i++) {
		generate_packet(buf, conf, i);
		seq_num = (buf[2] << 8)| buf[3];
		ret = sendto(sd, buf, conf->packet_len, 0, (struct sockaddr *)&conf->dst, sizeof(conf->dst));
		if (ret < 0) {
			perror("sendto");
		}
		gettimeofday(&start_time, NULL);
		ret = recv(sd, buf, conf->packet_len, 0);
		if (seq_num != ((buf[2] << 8)| buf[3])) {
			printf("Sequenze number did not match\n");
			continue;
		}
		if (ret > 0) {
			gettimeofday(&end_time, NULL);
			count++;
			sec = end_time.tv_sec - start_time.tv_sec;
			sum_sec += sec;
			usec = end_time.tv_usec - start_time.tv_usec;
			if (usec < 0) {
				usec += 1000000;
				sec--;
				sum_sec--;
			}

			sum_usec += usec;
			if (sec > sec_max)
				sec_max = sec;
			else if (sec < sec_min)
				sec_min = sec;
			if (usec > usec_max)
				usec_max = usec;
			else if (usec < usec_min)
				usec_min = usec;
			if (sec > 0)
				fprintf(stdout, "Warning: packet return time over a second!\n");

			if (conf->extended)
				fprintf(stdout, "%i bytes from %s seq=%i time=%.1f ms\n", ret,
					addr, (int)seq_num, (float)usec/1000);
			else
				fprintf(stdout, "%i bytes from 0x%04x seq=%i time=%.1f ms\n", ret,
					conf->dst.addr.short_addr, (int)seq_num, (float)usec/1000);
		} else
			fprintf(stderr, "Hit 500 ms packet timeout\n");
	}

	if (count)
		packet_loss = 100 - ((100 * count)/conf->packets);

	if (usec_min)
		rtt_min = (float)usec_min/1000;
	if (sum_usec && count)
		rtt_avg = ((float)sum_usec/(float)count)/1000;
	if (usec_max)
		rtt_max = (float)usec_max/1000;

	if (conf->extended)
		fprintf(stdout, "\n--- %s ping statistics ---\n", addr);
	else
		fprintf(stdout, "\n--- 0x%04x ping statistics ---\n", conf->dst.addr.short_addr);
	fprintf(stdout, "%i packets transmitted, %i received, %.0f%% packet loss\n",
		conf->packets, count, packet_loss);
	fprintf(stdout, "rtt min/avg/max = %.3f/%.3f/%.3f ms\n", rtt_min, rtt_avg, rtt_max);

	free(buf);
	return 0;
}

static void init_server(struct config *conf, int sd) {
	ssize_t len;
	unsigned char *buf;
	struct sockaddr_ieee802154 src;
	socklen_t addrlen;

	addrlen = sizeof(src);

	len = 0;
	fprintf(stdout, "Server mode. Waiting for packets...\n");
	buf = (unsigned char *)malloc(MAX_PAYLOAD_LEN);

	while (1) {
		len = recvfrom(sd, buf, MAX_PAYLOAD_LEN, 0, (struct sockaddr *)&src, &addrlen);
		if (len < 0) {
			perror("recvfrom");
		}
		//dump_packet(buf, len);
		/* Send same packet back */
		len = sendto(sd, buf, len, 0, (struct sockaddr *)&src, addrlen);
		if (len < 0) {
			perror("sendto");
		}
	}
	free(buf);
}

static int init_network(struct config *conf) {
	int sd;
	int ret;

	sd = socket(PF_IEEE802154, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("socket");
		return 1;
	}

	/* Bind socket on this side */
	ret = bind(sd, (struct sockaddr *)&conf->src, sizeof(conf->src));
	if (ret) {
		perror("bind");
		return 1;
	}

	if (conf->server)
		init_server(conf, sd);
	else
		measure_roundtrip(conf, sd);

	shutdown(sd, SHUT_RDWR);
	close(sd);
	return 0;
}

static int parse_dst_addr(struct config *conf, char *arg)
{
	int i;

	if (!arg)
		return -1;

	/* PAN ID is filled from netlink in get_interface_info */
	conf->dst.family = AF_IEEE802154;

	if (!conf->extended) {
		conf->dst.addr.addr_type = IEEE802154_ADDR_SHORT;
		conf->dst.addr.short_addr = strtol(arg, NULL, 16);
		return 0;
	}

	conf->dst.addr.addr_type = IEEE802154_ADDR_LONG;

	for (i = 0; i < IEEE802154_ADDR_LEN; i++) {
		int temp;
		char *cp = strchr(arg, ':');
		if (cp) {
			*cp = 0;
			cp++;
		}
		if (sscanf(arg, "%x", &temp) != 1)
			return -1;
		if (temp < 0 || temp > 255)
			return -1;

		conf->dst.addr.hwaddr[i] = temp;
		if (!cp)
			break;
		arg = cp;
	}
	if (i < IEEE802154_ADDR_LEN - 1)
		return -1;

	return 0;
}

int main(int argc, char *argv[]) {
	int c, ret;
	struct config *conf;
	char *dst_addr = NULL;

	conf = malloc(sizeof(struct config));

	/* Default to interface wpan0 if nothing else is given */
	conf->interface = "wpan0";

	/* Deafult to minimum packet size */
	conf->packet_len = MIN_PAYLOAD_LEN;

	/* Default to short addressing */
	conf->extended = false;

	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	while (1) {
#ifdef _GNU_SOURCE
		int opt_idx = -1;
		c = getopt_long(argc, argv, "a:ec:s:i:dvh", perf_long_opts, &opt_idx);
#else
		c = getopt(argc, argv, "a:ec:s:i:dvh");
#endif
		if (c == -1)
			break;
		switch(c) {
		case 'a':
			dst_addr = optarg;
			break;
		case 'e':
			conf->extended = true;
			break;
		case 'd':
			conf->server = true;
			break;
		case 'c':
			conf->packets = atoi(optarg);
			break;
		case 's':
			conf->packet_len = atoi(optarg);
			if (conf->packet_len >= MAX_PAYLOAD_LEN || conf->packet_len < MIN_PAYLOAD_LEN) {
				printf("Packet size must be between %i and %i.\n",
				       MIN_PAYLOAD_LEN, MAX_PAYLOAD_LEN - 1);
				return 1;
			}
			break;
		case 'i':
			conf->interface = optarg;
			break;
		case 'v':
			fprintf(stdout, "wpan-ping " PACKAGE_VERSION "\n");
			return 1;
		case 'h':
			usage(argv[0]);
			return 1;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	get_interface_info(conf);

	if (!conf->server) {
		ret = parse_dst_addr(conf, dst_addr);
		if (ret< 0) {
			fprintf(stderr, "Address given in wrong format.\n");
			return 1;
		}
	}
	init_network(conf);
	free(conf);
	return 0;
}
