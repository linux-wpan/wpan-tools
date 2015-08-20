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
