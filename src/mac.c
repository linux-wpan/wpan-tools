// SPDX-FileCopyrightText: 2014 Alexander Aring <alex.aring@gmail.com>
//
// SPDX-License-Identifier: ISC

#include <net/if.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl_extras.h"
#include "nl802154.h"
#include "iwpan.h"

static int handle_pan_id_set(struct nl802154_state *state,
			     struct nl_cb *cb,
			     struct nl_msg *msg,
			     int argc, char **argv,
			     enum id_input id)
{
	unsigned long pan_id;
	char *end;

	if (argc < 1)
		return 1;

	/* PAN ID */
	pan_id = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	if (pan_id > UINT16_MAX)
		return 1;

	NLA_PUT_U16(msg, NL802154_ATTR_PAN_ID, htole16(pan_id));

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, pan_id, "<pan_id>",
	NL802154_CMD_SET_PAN_ID, 0, CIB_NETDEV, handle_pan_id_set, NULL);

static int handle_short_addr_set(struct nl802154_state *state,
				 struct nl_cb *cb,
				 struct nl_msg *msg,
				 int argc, char **argv,
				 enum id_input id)
{
	unsigned long short_addr;
	char *end;

	if (argc < 1)
		return 1;

	/* SHORT ADDR */
	short_addr = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	if (short_addr > UINT16_MAX)
		return 1;

	NLA_PUT_U16(msg, NL802154_ATTR_SHORT_ADDR, htole16(short_addr));

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, short_addr, "<short_addr>",
	NL802154_CMD_SET_SHORT_ADDR, 0, CIB_NETDEV, handle_short_addr_set, NULL);

static int handle_max_frame_retries_set(struct nl802154_state *state,
					struct nl_cb *cb,
					struct nl_msg *msg,
					int argc, char **argv,
					enum id_input id)
{
	long retries;
	char *end;

	if (argc < 1)
		return 1;

	/* RETRIES */
	retries = strtol(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	if (retries > INT8_MAX)
		return 1;

	NLA_PUT_S8(msg, NL802154_ATTR_MAX_FRAME_RETRIES, retries);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, max_frame_retries, "<retries>",
	NL802154_CMD_SET_MAX_FRAME_RETRIES, 0, CIB_NETDEV,
	handle_max_frame_retries_set, NULL);

static int handle_backoff_exponent(struct nl802154_state *state,
				   struct nl_cb *cb,
				   struct nl_msg *msg,
				   int argc, char **argv,
				   enum id_input id)
{
	unsigned long max_be;
	unsigned long min_be;
	char *end;

	if (argc < 2)
		return 1;

	/* MIN_BE */
	min_be = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	/* MAX_BE */
	max_be = strtoul(argv[1], &end, 0);
	if (*end != '\0')
		return 1;

	if (min_be > UINT8_MAX || max_be > UINT8_MAX)
		return 1;

	NLA_PUT_U8(msg, NL802154_ATTR_MIN_BE, min_be);
	NLA_PUT_U8(msg, NL802154_ATTR_MAX_BE, max_be);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, backoff_exponents, "<min_be> <max_be>",
	NL802154_CMD_SET_BACKOFF_EXPONENT, 0, CIB_NETDEV,
	handle_backoff_exponent, NULL);

static int handle_max_csma_backoffs(struct nl802154_state *state,
				    struct nl_cb *cb,
				    struct nl_msg *msg,
				    int argc, char **argv,
				    enum id_input id)
{
	unsigned long backoffs;
	char *end;

	if (argc < 1)
		return 1;

	/* BACKOFFS */
	backoffs = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	if (backoffs > UINT8_MAX)
		return 1;

	NLA_PUT_U8(msg, NL802154_ATTR_MAX_CSMA_BACKOFFS, backoffs);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, max_csma_backoffs, "<backoffs>",
	NL802154_CMD_SET_MAX_CSMA_BACKOFFS, 0, CIB_NETDEV,
	handle_max_csma_backoffs, NULL);


static int handle_lbt_mode(struct nl802154_state *state,
			   struct nl_cb *cb,
			   struct nl_msg *msg,
			   int argc, char **argv,
			   enum id_input id)
{
	unsigned long mode;
	char *end;

	if (argc < 1)
		return 1;

	/* LBT_MODE */
	mode = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	if (mode > UINT8_MAX)
		return 1;

	NLA_PUT_U8(msg, NL802154_ATTR_LBT_MODE, mode);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, lbt, "<1|0>",
	NL802154_CMD_SET_LBT_MODE, 0, CIB_NETDEV, handle_lbt_mode, NULL);

static int handle_ackreq_default(struct nl802154_state *state,
				 struct nl_cb *cb,
				 struct nl_msg *msg,
				 int argc, char **argv,
				 enum id_input id)
{
	unsigned long ackreq;
	char *end;

	if (argc < 1)
		return 1;

	/* ACKREQ_DEFAULT */
	ackreq = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	if (ackreq > UINT8_MAX)
		return 1;

	NLA_PUT_U8(msg, NL802154_ATTR_ACKREQ_DEFAULT, ackreq);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, ackreq_default, "<1|0>",
	NL802154_CMD_SET_ACKREQ_DEFAULT, 0, CIB_NETDEV, handle_ackreq_default,
	NULL);

static int handle_set_max_associations(struct nl802154_state *state,
				      struct nl_cb *cb,
				      struct nl_msg *msg,
				      int argc, char **argv,
				      enum id_input id)
{
	unsigned long max_associations;
	char *end;

	if (argc < 1)
		return 1;

	/* Maximum number of PAN entries */
	max_associations = strtoul(argv[0], &end, 0);
	if (*end != '\0')
		return 1;

	NLA_PUT_U32(msg, NL802154_ATTR_MAX_ASSOCIATIONS, max_associations);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, max_associations, "<max_associations>",
	NL802154_CMD_SET_MAX_ASSOCIATIONS, 0, CIB_NETDEV,
	handle_set_max_associations, NULL);

int parse_associated_devices(struct nlattr *nestedassoc)
{
	struct nlattr *assoc[NL802154_DEV_ADDR_ATTR_MAX + 1];
	static struct nla_policy assoc_policy[NL802154_DEV_ADDR_ATTR_MAX + 1] = {
		[NL802154_DEV_ADDR_ATTR_PEER_TYPE] = { .type = NLA_U8, },
		[NL802154_DEV_ADDR_ATTR_MODE] = { .type = NLA_U8, },
		[NL802154_DEV_ADDR_ATTR_SHORT] = { .type = NLA_U16, },
		[NL802154_DEV_ADDR_ATTR_EXTENDED] = { .type = NLA_U64, },
	};
	bool short_addressing;
	uint8_t peer_type;
	int ret;

	ret = nla_parse_nested(assoc, NL802154_DEV_ADDR_ATTR_MAX, nestedassoc,
			       assoc_policy);
	if (ret < 0) {
		fprintf(stderr, "failed to parse nested attributes! (ret = %d)\n",
			ret);
		return NL_SKIP;
	}

	if (!assoc[NL802154_DEV_ADDR_ATTR_PEER_TYPE] ||
	    !assoc[NL802154_DEV_ADDR_ATTR_SHORT] ||
	    !assoc[NL802154_DEV_ADDR_ATTR_EXTENDED])
		return NL_SKIP;

	peer_type = nla_get_u8(assoc[NL802154_DEV_ADDR_ATTR_PEER_TYPE]);
	printf("%s: 0x%04x / 0x%016llx\n",
	       peer_type == NL802154_PEER_TYPE_PARENT ? "parent" : "child ",
	       nla_get_u16(assoc[NL802154_DEV_ADDR_ATTR_SHORT]),
	       nla_get_u64(assoc[NL802154_DEV_ADDR_ATTR_EXTENDED]));

	return NL_OK;
}

static int print_association_list_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL802154_ATTR_MAX + 1];
	struct nlattr *nestedassoc;

	nla_parse(tb, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	nestedassoc = tb[NL802154_ATTR_PEER];
	if (!nestedassoc) {
		fprintf(stderr, "peer info missing!\n");
		return NL_SKIP;
	}
	return parse_associated_devices(nestedassoc);
}

static int list_associations_handler(struct nl802154_state *state,
				     struct nl_cb *cb,
				     struct nl_msg *msg,
				     int argc, char **argv,
				     enum id_input id)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_association_list_handler, NULL);

	return 0;
}
TOPLEVEL(list_associations, NULL, NL802154_CMD_LIST_ASSOCIATIONS,
	NLM_F_DUMP, CIB_NETDEV,	list_associations_handler,
	"List the associated devices on this virtual interface");

static int associate_handler(struct nl802154_state *state,
			     struct nl_cb *cb,
			     struct nl_msg *msg,
			     int argc, char **argv,
			     enum id_input id)
{
	unsigned int pan_id;
	uint64_t laddr = 0;
	char *end;
	int tpset;

	if (argc < 4)
		return 1;

	/* PAN ID */
	if (strcmp(argv[0], "pan_id"))
		return 1;

	pan_id = strtoul(argv[1], &end, 0);
	if (*end != '\0')
		return 1;

	if (pan_id > UINT16_MAX)
		return 1;

	argc -= 2;
	argv += 2;

	/* Coordinator */
	if (strcmp(argv[0], "coord"))
		return 1;

	laddr = strtoull(argv[1], &end, 0);
	if (*end != '\0')
		return 1;

	NLA_PUT_U16(msg, NL802154_ATTR_PAN_ID, htole16(pan_id));
	NLA_PUT_U64(msg, NL802154_ATTR_EXTENDED_ADDR, htole64(laddr));

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
TOPLEVEL(associate, "pan_id <pan_id> coord <coord>", NL802154_CMD_ASSOCIATE, 0,
	 CIB_NETDEV, associate_handler,
	 "Join a PAN by sending an association request to the given coordinator");

static int disassociate_handler(struct nl802154_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv,
				enum id_input id)
{
	bool use_extended_addressing;
	uint64_t laddr;
	unsigned int saddr;
	char *end;
	int tpset;

	if (argc < 2)
		return 1;

	if (!strcmp(argv[0], "ext_addr")) {
		use_extended_addressing = true;
		laddr = strtoull(argv[1], &end, 0);
		if (*end != '\0')
			return 1;
	} else if (!strcmp(argv[0], "short_addr")) {
		use_extended_addressing = false;
		saddr = strtoul(argv[1], &end, 0);
		if (*end != '\0')
			return 1;

		if (saddr > UINT16_MAX - 2)
			return 1;
	} else {
		return 1;
	}

	if (use_extended_addressing)
		NLA_PUT_U64(msg, NL802154_ATTR_EXTENDED_ADDR, htole64(laddr));
	else
		NLA_PUT_U16(msg, NL802154_ATTR_SHORT_ADDR, htole16(saddr));

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
TOPLEVEL(disassociate, "short_addr|ext_addr <addr>", NL802154_CMD_DISASSOCIATE,
	 0, CIB_NETDEV, disassociate_handler,
	 "Send a disassociation notification to a device");
