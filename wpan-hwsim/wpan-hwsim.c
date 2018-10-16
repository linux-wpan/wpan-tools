/*
 * Linux IEEE 802.15.4 hwsim tool
 *
 * Copyright (C) 2018 Alexander Aring <aring@mojatatu.com>
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

#include <inttypes.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

#include <netlink/netlink.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include "mac802154_hwsim.h"
#include "config.h"

static struct nl_sock *nl_sock;
static int nlhwsim_id;

static int nlhwsim_init(void)
{
	int err;

	nl_sock = nl_socket_alloc();
	if (!nl_sock) {
		fprintf(stderr, "Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	nl_socket_set_buffer_size(nl_sock, 8192, 8192);

	if (genl_connect(nl_sock)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	nlhwsim_id = genl_ctrl_resolve(nl_sock, "MAC802154_HWSIM");
	if (nlhwsim_id < 0) {
		fprintf(stderr, "MAC802154_HWSIM not found.\n");
		err = -ENOENT;
		nl_close(nl_sock);
		goto out_handle_destroy;
	}

	return 0;

out_handle_destroy:
	nl_socket_free(nl_sock);
	return err;
}

static void nlhwsim_cleanup(void)
{
	nl_close(nl_sock);
	nl_socket_free(nl_sock);
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int hwsim_create(void)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int idx = 0, ret;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb)
		return -ENOMEM;

	msg = nlmsg_alloc();
	if (!msg) {
		nl_cb_put(cb);
		return -ENOMEM;
	}

	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nlhwsim_id, 0, 0,
		    MAC802154_HWSIM_CMD_NEW_RADIO, 0);
	ret = nl_send_auto(nl_sock, msg);
	if (ret < 0) {
		nl_cb_put(cb);
		return ret;
	}

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &idx);
	nl_recvmsgs(nl_sock, cb);
	printf("wpan_hwsim radio%d registered.\n", idx);

	nl_cb_put(cb);

	return 0;
}

static int nl_msg_dot_cb(struct nl_msg* msg, void* arg)
{
	static struct nla_policy edge_policy[MAC802154_HWSIM_EDGE_ATTR_MAX + 1] = {
		[MAC802154_HWSIM_EDGE_ATTR_ENDPOINT_ID] = { .type = NLA_U32 },
		[MAC802154_HWSIM_EDGE_ATTR_LQI] = { .type = NLA_U8 },
	};
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct genlmsghdr *gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
	struct nlattr *attrs[MAC802154_HWSIM_ATTR_MAX + 1];
	struct nlattr *edge[MAC802154_HWSIM_EDGE_ATTR_MAX + 1];
	unsigned int *ignore_idx = arg;
	struct nlattr *nl_edge;
	uint32_t idx, idx2;
	uint8_t lqi = 0xff;
	int rem_edge;
	int rc;

	nla_parse(attrs, MAC802154_HWSIM_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!attrs[MAC802154_HWSIM_ATTR_RADIO_ID])
		return NL_SKIP;

	idx = nla_get_u32(attrs[MAC802154_HWSIM_ATTR_RADIO_ID]);
	if (idx == *ignore_idx)
		return NL_SKIP;

	if (!attrs[MAC802154_HWSIM_ATTR_RADIO_EDGES])
		return NL_SKIP;

	nla_for_each_nested(nl_edge, attrs[MAC802154_HWSIM_ATTR_RADIO_EDGES],
			    rem_edge) {
		rc = nla_parse_nested(edge, MAC802154_HWSIM_EDGE_ATTR_MAX,
				      nl_edge, edge_policy);
		if (rc)
			return NL_SKIP;

		idx2 = nla_get_u32(edge[MAC802154_HWSIM_EDGE_ATTR_ENDPOINT_ID]);
		if (idx2 == *ignore_idx)
			continue;

		lqi = nla_get_u32(edge[MAC802154_HWSIM_EDGE_ATTR_LQI]);
		printf("\t%" PRIu32 " -> %" PRIu32 "[label=%" PRIu8 "];\n",
		       idx, idx2, lqi);
	}

	return NL_SKIP;
}

static int nl_msg_cb(struct nl_msg* msg, void* arg)
{
	static struct nla_policy edge_policy[MAC802154_HWSIM_EDGE_ATTR_MAX + 1] = {
		[MAC802154_HWSIM_EDGE_ATTR_ENDPOINT_ID] = { .type = NLA_U32 },
		[MAC802154_HWSIM_EDGE_ATTR_LQI] = { .type = NLA_U8 },
	};
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct genlmsghdr *gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
	struct nlattr *attrs[MAC802154_HWSIM_ATTR_MAX + 1];
	struct nlattr *edge[MAC802154_HWSIM_EDGE_ATTR_MAX + 1];
	unsigned int *ignore_idx = arg;
	struct nlattr *nl_edge;
	uint32_t idx;
	uint8_t lqi;
	int rem_edge;
	int rc;

	nla_parse(attrs, MAC802154_HWSIM_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!attrs[MAC802154_HWSIM_ATTR_RADIO_ID])
		return NL_SKIP;

	idx = nla_get_u32(attrs[MAC802154_HWSIM_ATTR_RADIO_ID]);

	if (idx == *ignore_idx)
		return NL_SKIP;

	printf("wpan_hwsim radio%" PRIu32 ":\n", idx);

	if (!attrs[MAC802154_HWSIM_ATTR_RADIO_EDGES])
		return NL_SKIP;

	nla_for_each_nested(nl_edge, attrs[MAC802154_HWSIM_ATTR_RADIO_EDGES],
			    rem_edge) {
		rc = nla_parse_nested(edge, MAC802154_HWSIM_EDGE_ATTR_MAX,
				      nl_edge, edge_policy);
		if (rc)
			return NL_SKIP;

		idx = nla_get_u32(edge[MAC802154_HWSIM_EDGE_ATTR_ENDPOINT_ID]);

		if (idx == *ignore_idx)
			continue;

		printf("\tedge:\n");
		printf("\t\tradio%" PRIu32 "\n", idx);
		lqi = nla_get_u32(edge[MAC802154_HWSIM_EDGE_ATTR_LQI]);
		printf("\t\tlqi: 0x%02x\n", lqi);
	}

	return NL_SKIP;
}

static int hwsim_dump(bool dot, int unsigned ignore_idx)
{
	struct nl_msg *msg;
	int rc;

	nl_socket_modify_cb(nl_sock, NL_CB_VALID, NL_CB_CUSTOM,
			    dot ? nl_msg_dot_cb : nl_msg_cb, &ignore_idx);
	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nlhwsim_id, 0, NLM_F_DUMP,
		    MAC802154_HWSIM_CMD_GET_RADIO, 0);

	if (dot)
		printf("digraph {\n");

	rc = nl_send_sync(nl_sock, msg);
	if (rc < 0)
		return rc;

	if (dot)
		printf("}\n");

	return rc;
}

static int hwsim_del(uint32_t idx)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int err = 0;
	int rc;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb)
		return -ENOMEM;

	msg = nlmsg_alloc();
	if (!msg) {
		nl_cb_put(cb);
		return -ENOMEM;
	}

	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nlhwsim_id, 0, 0,
		    MAC802154_HWSIM_CMD_DEL_RADIO, 0);
	nla_put_u32(msg, MAC802154_HWSIM_ATTR_RADIO_ID, idx);

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	rc = nl_send_auto(nl_sock, msg);
	nl_recvmsgs(nl_sock, cb);
	if (err < 0)
		fprintf(stderr, "Failed to remove radio: %s\n", strerror(abs(err)));
	nl_cb_put(cb);

	return rc;
}

static int hwsim_cmd_edge(int cmd, uint32_t idx, uint32_t idx2, uint8_t lqi)
{
	struct nlattr* edge;
	struct nl_msg *msg;
	struct nl_cb *cb;
	int err = 0;
	int rc;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb)
		return -ENOMEM;

	msg = nlmsg_alloc();
	if (!msg) {
		nl_cb_put(cb);
		return -ENOMEM;
	}

	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nlhwsim_id, 0, 0,
		    cmd, 0);
	nla_put_u32(msg, MAC802154_HWSIM_ATTR_RADIO_ID, idx);

	edge = nla_nest_start(msg, MAC802154_HWSIM_ATTR_RADIO_EDGE);
	if (!edge) {
		nl_cb_put(cb);
		return -ENOMEM;
	}

	nla_put_u32(msg, MAC802154_HWSIM_EDGE_ATTR_ENDPOINT_ID, idx2);
	if (cmd == MAC802154_HWSIM_CMD_SET_EDGE)
		nla_put_u8(msg, MAC802154_HWSIM_EDGE_ATTR_LQI, lqi);

	nla_nest_end(msg, edge);

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	rc = nl_send_auto(nl_sock, msg);
	nl_recvmsgs(nl_sock, cb);
	if (err < 0)
		fprintf(stderr, "Failed to add or remove edge: %s\n", strerror(abs(err)));
	nl_cb_put(cb);

	return rc;
}

static int hwsim_do_cmd(uint32_t cmd, unsigned int idx, unsigned int idx2,
			uint8_t lqi, bool dot, unsigned int ignore_idx)
{
	int rc;

	rc = nlhwsim_init();
	if (rc)
		return 1;

	switch (cmd) {
	case MAC802154_HWSIM_CMD_GET_RADIO:
		rc = hwsim_dump(dot, ignore_idx);
		break;
	case MAC802154_HWSIM_CMD_DEL_RADIO:
		rc = hwsim_del(idx);
		break;
	case MAC802154_HWSIM_CMD_NEW_RADIO:
		rc = hwsim_create();
		break;
	case MAC802154_HWSIM_CMD_NEW_EDGE:
	case MAC802154_HWSIM_CMD_DEL_EDGE:
	case MAC802154_HWSIM_CMD_SET_EDGE:
		rc = hwsim_cmd_edge(cmd, idx, idx2, lqi);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	nlhwsim_cleanup();

	return rc;
}

static void print_usage(void)
{
	printf("wpan_hwsim [OPTION...] [CMD...]\n");
	printf("\n");
	printf(" possible options:\n");
	printf("  -h, --help             show this help\n");
	printf("  -v, --version          show version\n");
	printf("  -d, --dot              dump topology as dot format\n");
	printf("  -i, --ignore           filter one node from dump (useful for virtual monitors)\n");
	printf("\n");
	printf(" possible commands:\n");
	printf("  add                    add new hwsim radio\n");
	printf("  del IDX                del hwsim radio according idx\n");
	printf("  edge add IDX IDX       add edge between radios\n");
	printf("  edge del IDX IDX       delete edge between radios\n");
	printf("  edge lqi IDX IDX LQI   set lqi value for a specific edge\n");
	printf("\n");
	printf("  To dump all node information just call this program without any command\n");
}

static void print_version(void)
{
	printf("wpan-hwsim " PACKAGE_VERSION "\n");
}

int main(int argc, const char *argv[])
{
	unsigned long int idx, idx2, lqi, ignore_idx = ULONG_MAX;
	bool dot = false;
	int cmd;
	int rc;
	int c;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help",	no_argument,		0,	'h' },
			{"version",	no_argument,		0,	'v' },
			{"dot",		no_argument,		0,	'd' },
			{"ignore",	required_argument,	0,	'i' },
			{0,				0,	0,	0 }
		};

		c = getopt_long(argc, (char **)argv, "vhdi:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'v':
			print_version();
			return EXIT_SUCCESS;
		case 'h':
			print_usage();
			return EXIT_SUCCESS;
		case 'd':
			dot = true;
			break;
		case 'i':
			ignore_idx = strtoul(optarg, NULL, 0);
			if (ignore_idx == ULONG_MAX) {
				fprintf(stderr, "Invalid radio index for ignore argument\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			print_usage();
			return EXIT_FAILURE;
		}
	}

	if (optind + 1 > argc) {
		rc = hwsim_do_cmd(MAC802154_HWSIM_CMD_GET_RADIO, 0, 0, 0, dot,
				  ignore_idx);
		if (rc < 0)
			return EXIT_FAILURE;
		else
			goto out;
	}

	if (!strncmp(argv[optind], "add", 3)) {
		rc = hwsim_do_cmd(MAC802154_HWSIM_CMD_NEW_RADIO, 0, 0, 0, 0, 0);
		if (rc < 0)
			return EXIT_FAILURE;
	} else if (!strncmp(argv[optind], "del", 3)) {
		if (optind + 2 > argc) {
			fprintf(stderr, "Missing radio index for delete\n");
			return EXIT_FAILURE;
		} else {
			idx = strtoul(argv[optind + 1], NULL, 0);
			if (idx == ULONG_MAX) {
				fprintf(stderr, "Invalid radio index for delete\n");
				return EXIT_FAILURE;
			}

			rc = hwsim_do_cmd(MAC802154_HWSIM_CMD_DEL_RADIO, idx, 0, 0, 0, 0);
			if (rc < 0)
				return EXIT_FAILURE;
		}
	} else if (!strncmp(argv[optind], "edge", 4)) {
		if (optind + 4 > argc) {
			fprintf(stderr, "Missing edge radio index information\n");
			return EXIT_FAILURE;
		} else {
			if (!strncmp(argv[optind + 1], "add", 3)) {
				cmd = MAC802154_HWSIM_CMD_NEW_EDGE;
			} else if (!strncmp(argv[optind + 1], "del", 3)) {
				cmd = MAC802154_HWSIM_CMD_DEL_EDGE;
			} else if (!strncmp(argv[optind + 1], "lqi", 3)) {
				cmd = MAC802154_HWSIM_CMD_SET_EDGE;
			} else {
				fprintf(stderr, "Invalid edge command\n");
				return EXIT_FAILURE;
			}

			if (cmd == MAC802154_HWSIM_CMD_SET_EDGE) {
				if (optind + 5 > argc) {
					fprintf(stderr, "LQI information missing\n");
					return EXIT_FAILURE;
				}

				lqi = strtoul(argv[optind + 4], NULL, 0);
				if (lqi == ULONG_MAX || lqi > 0xff) {
					fprintf(stderr, "Invalid lqi value\n");
					return EXIT_FAILURE;
				}
			}

			idx = strtoul(argv[optind + 2], NULL, 0);
			if (idx == ULONG_MAX) {
				fprintf(stderr, "Invalid first radio index for edge command\n");
				return EXIT_FAILURE;
			}

			idx2 = strtoul(argv[optind + 3], NULL, 0);
			if (idx2 == ULONG_MAX) {
				fprintf(stderr, "Invalid second radio index for edge command\n");
				return EXIT_FAILURE;
			}

			rc = hwsim_do_cmd(cmd, idx, idx2, lqi, 0, 0);
			if (rc < 0)
				return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "Unknown command\n");
		print_usage();
		return EXIT_FAILURE;
	}

out:
	return EXIT_SUCCESS;
}
