#include <errno.h>
#include <inttypes.h>
#include <net/if.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl802154.h"
#include "nl_extras.h"
#include "iwpan.h"

static char scantypebuf[100];

static const char *scantype_name(enum nl802154_scan_types scantype)
{
	switch (scantype) {
	case NL802154_SCAN_ED:
		return "ed";
	case NL802154_SCAN_ACTIVE:
		return "active";
	case NL802154_SCAN_PASSIVE:
		return "passive";
	case NL802154_SCAN_ENHANCED_ACTIVE:
		return "enhanced";
	case NL802154_SCAN_RIT_PASSIVE:
		return "rit";
	default:
		sprintf(scantypebuf, "Invalid scantype (%d)", scantype);
		return scantypebuf;
	}
}

/* for help */
#define SCAN_TYPES "Valid scanning types are: ed, active, passive, enhanced, rit."

/* return 0 if ok, internal error otherwise */
static int get_scan_type(int *argc, char ***argv, enum nl802154_scan_types *type)
{
	char *tpstr;

	if (*argc < 2)
		return 1;

	if (strcmp((*argv)[0], "type"))
		return 1;

	tpstr = (*argv)[1];
	*argc -= 2;
	*argv += 2;

	if (strcmp(tpstr, "ed") == 0) {
		*type = NL802154_SCAN_ED;
		return 0;
	} else if (strcmp(tpstr, "active") == 0) {
		*type = NL802154_SCAN_ACTIVE;
		return 0;
	} else if (strcmp(tpstr, "passive") == 0) {
		*type = NL802154_SCAN_PASSIVE;
		return 0;
	} else if (strcmp(tpstr, "enhanced") == 0) {
		*type = NL802154_SCAN_ENHANCED_ACTIVE;
		return 0;
	} else if (strcmp(tpstr, "rit") == 0) {
		*type = NL802154_SCAN_RIT_PASSIVE;
		return 0;
	}

	fprintf(stderr, "invalid interface type %s\n", tpstr);
	return 2;
}

static int get_option_value(int *argc, char ***argv, const char *marker, unsigned long *result, bool *valid)
{
	unsigned long value;
	char *tpstr, *end;

	*valid = false;

	if (*argc < 2)
		return 0;

	if (strcmp((*argv)[0], marker))
		return 0;

	tpstr = (*argv)[1];
	*argc -= 2;
	*argv += 2;

	value = strtoul(tpstr, &end, 0);
	if (*end != '\0')
		return 1;

	*result = value;
	*valid = true;

	return 0;
}

static int scan_trigger_handler(struct nl802154_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv,
				enum id_input id)
{
	enum nl802154_scan_types type;
	unsigned long page, channels, duration;
	int tpset;
	bool valid_page, valid_channels, valid_duration;

	if (argc < 2)
		return 1;

	tpset = get_scan_type(&argc, &argv, &type);
	if (tpset)
		return tpset;

	tpset = get_option_value(&argc, &argv, "page", &page, &valid_page);
	if (tpset)
		return tpset;
	if (valid_page && page > UINT8_MAX)
		return 1;

	tpset = get_option_value(&argc, &argv, "channels", &channels, &valid_channels);
	if (tpset)
		return tpset;
	if (valid_channels && channels > UINT32_MAX)
		return 1;

	tpset = get_option_value(&argc, &argv, "duration", &duration, &valid_duration);
	if (tpset)
		return tpset;
	if (valid_duration && duration > UINT8_MAX)
		return 1;

	if (argc)
		return 1;

	/* Mandatory argument */
	NLA_PUT_U8(msg, NL802154_ATTR_SCAN_TYPE, type);
	/* Optional arguments */
	if (valid_duration)
		NLA_PUT_U8(msg, NL802154_ATTR_SCAN_DURATION, duration);
	if (valid_page)
		NLA_PUT_U8(msg, NL802154_ATTR_PAGE, page);
	if (valid_channels)
		NLA_PUT_U32(msg, NL802154_ATTR_SCAN_CHANNELS, channels);

	/* TODO: support IES parameters for active scans */

	return 0;

nla_put_failure:
	return -ENOBUFS;
}

static int scan_abort_handler(struct nl802154_state *state,
			      struct nl_cb *cb,
			      struct nl_msg *msg,
			      int argc, char **argv,
			      enum id_input id)
{
	return 0;
}

struct ieee802154_addr {
	uint16_t pan_id;
	uint8_t addr_len;
	union {
		uint16_t short_addr;
		uint64_t extended_addr;
	};
	struct ieee802154_addr *next;
};

static struct ieee802154_addr *known_coord_list;

static bool coord_addrs_are_equal(struct ieee802154_addr *a,
				  struct ieee802154_addr *b)
{
	if (a->pan_id != b->pan_id)
		return false;

	if (a->addr_len != b->addr_len)
		return false;

	if (a->addr_len == 2 && a->short_addr == b->short_addr)
		return true;
	else if (a->extended_addr == b->extended_addr)
		return true;

	return false;
}

static bool coord_is_known(struct ieee802154_addr *addr)
{
	struct ieee802154_addr *item = known_coord_list;

	while (item) {
		if (coord_addrs_are_equal(addr, item))
			return true;

		item = item->next;
	}

	return false;
}

static void record_coord(struct ieee802154_addr *addr)
{
	struct ieee802154_addr *item;

	if (!known_coord_list) {
		known_coord_list = addr;
		return;
	}

	item = known_coord_list;
	if (!item->next) {
		item->next = addr;
		return;
	}

	do {
		item = item->next;
	} while (item->next);

	item->next = addr;
}

static void flush_coords(void)
{
	struct ieee802154_addr *item, *next;

	if (!known_coord_list)
		return;

	item = known_coord_list;
	do {
		next = item->next;
		free(item);
		item = next;
	} while (item);
}

static struct ieee802154_addr *parse_new_coordinator(struct nlattr *nestedcoord)
{
	struct nlattr *pan[NL802154_COORD_MAX + 1];
	static struct nla_policy pan_policy[NL802154_COORD_MAX + 1] = {
		[NL802154_COORD_PANID] = { .type = NLA_U16, },
		[NL802154_COORD_ADDR] = { .minlen = 2, .maxlen = 8, }, /* 2 or 8 */
		[NL802154_COORD_CHANNEL] = { .type = NLA_U8, },
		[NL802154_COORD_PAGE] = { .type = NLA_U8, },
		[NL802154_COORD_PREAMBLE_CODE] = { .type = NLA_U8, },
		[NL802154_COORD_MEAN_PRF] = { .type = NLA_U8, },
		[NL802154_COORD_SUPERFRAME_SPEC] = { .type = NLA_U16, },
		[NL802154_COORD_LINK_QUALITY] = { .type = NLA_U8, },
		[NL802154_COORD_GTS_PERMIT] = { .type = NLA_FLAG, },
	};
	struct ieee802154_addr *addr;
	struct nlattr *coord;
	char dev[20];
	int ret;

	ret = nla_parse_nested(pan, NL802154_COORD_MAX, nestedcoord, pan_policy);
	if (ret < 0) {
		fprintf(stderr, "failed to parse nested attributes! (ret = %d)\n",
			ret);
		return NULL;
	}
	if (!pan[NL802154_COORD_PANID] || !pan[NL802154_COORD_ADDR])
		return NULL;

	addr = malloc(sizeof(*addr));
	if (!addr)
		return NULL;

	addr->pan_id = nla_get_u16(pan[NL802154_COORD_PANID]);
	coord = pan[NL802154_COORD_ADDR];
	addr->addr_len = nla_len(coord);
	if (addr->addr_len == 2)
		addr->short_addr = nla_get_u16(coord);
	else
		addr->extended_addr = nla_get_u64(coord);

	return addr;
}

static int print_new_coordinator(struct nlattr *nestedcoord,
				 struct nlattr *ifattr)
{
	struct nlattr *pan[NL802154_COORD_MAX + 1];
	static struct nla_policy pan_policy[NL802154_COORD_MAX + 1] = {
		[NL802154_COORD_PANID] = { .type = NLA_U16, },
		[NL802154_COORD_ADDR] = { .minlen = 2, .maxlen = 8, }, /* 2 or 8 */
		[NL802154_COORD_CHANNEL] = { .type = NLA_U8, },
		[NL802154_COORD_PAGE] = { .type = NLA_U8, },
		[NL802154_COORD_PREAMBLE_CODE] = { .type = NLA_U8, },
		[NL802154_COORD_MEAN_PRF] = { .type = NLA_U8, },
		[NL802154_COORD_SUPERFRAME_SPEC] = { .type = NLA_U16, },
		[NL802154_COORD_LINK_QUALITY] = { .type = NLA_U8, },
		[NL802154_COORD_GTS_PERMIT] = { .type = NLA_FLAG, },
	};
	char dev[20];
	int ret;

	ret = nla_parse_nested(pan, NL802154_COORD_MAX, nestedcoord, pan_policy);
	if (ret < 0) {
		fprintf(stderr, "failed to parse nested attributes! (ret = %d)\n",
			ret);
		return NL_SKIP;
	}
	if (!pan[NL802154_COORD_PANID])
		return NL_SKIP;

	printf("PAN 0x%04x", le16toh(nla_get_u16(pan[NL802154_COORD_PANID])));
	if (ifattr) {
		if_indextoname(nla_get_u32(ifattr), dev);
		printf(" (on %s)", dev);
	}
	printf("\n");
	if (pan[NL802154_COORD_ADDR]) {
		struct nlattr *coord = pan[NL802154_COORD_ADDR];
		if (nla_len(coord) == 2) {
			uint16_t addr = nla_get_u16(coord);
			printf("\tcoordinator 0x%04x\n", le16toh(addr));
		} else {
			uint64_t addr = nla_get_u64(coord);
			printf("\tcoordinator 0x%016" PRIx64 "\n", le64toh(addr));
		}
	}
	if (pan[NL802154_COORD_PAGE]) {
		printf("\tpage %u\n", nla_get_u8(pan[NL802154_COORD_PAGE]));
	}
	if (pan[NL802154_COORD_CHANNEL]) {
		printf("\tchannel %u\n", nla_get_u8(pan[NL802154_COORD_CHANNEL]));
	}
	if (pan[NL802154_COORD_SUPERFRAME_SPEC]) {
		printf("\tsuperframe spec. 0x%x\n", nla_get_u16(
				pan[NL802154_COORD_SUPERFRAME_SPEC]));
	}
	if (pan[NL802154_COORD_LINK_QUALITY]) {
		printf("\tLQI %x\n", nla_get_u8(
				pan[NL802154_COORD_LINK_QUALITY]));
	}
	if (pan[NL802154_COORD_GTS_PERMIT]) {
		printf("\tGTS permitted\n");
	}

	/* TODO: Beacon IES display/decoding */

	return NL_OK;
}

static int parse_and_print_new_coordinator(struct nlattr *nestedcoord,
					   struct nlattr *ifattr)
{
	struct ieee802154_addr *addr;

	addr = parse_new_coordinator(nestedcoord);
	if (!addr)
		return NL_SKIP;

	if (coord_is_known(addr)) {
		free(addr);
	} else {
		record_coord(addr);
		print_new_coordinator(nestedcoord, ifattr);
	}

	return NL_OK;
}

static int parse_and_print_scan_event(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL802154_ATTR_MAX + 1];
	struct nlattr *nestedcoord;

	nla_parse(tb, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	nestedcoord = tb[NL802154_ATTR_COORDINATOR];
	if (!nestedcoord) {
		fprintf(stderr, "coordinator info missing!\n");
		return NL_SKIP;
	}

	return parse_and_print_new_coordinator(nestedcoord, tb[NL802154_ATTR_IFINDEX]);
}

struct scan_done {
	volatile int done;
	int devidx;
};

static int scan_message_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct scan_done *sd = (struct scan_done *)arg;

	if (gnlh->cmd == NL802154_CMD_SCAN_EVENT)
		return parse_and_print_scan_event(msg, arg);

	if (gnlh->cmd != NL802154_CMD_SCAN_DONE &&
	    gnlh->cmd != NL802154_CMD_ABORT_SCAN)
		return 0;

	if (sd->devidx != -1) {
		struct nlattr *tb[NL802154_ATTR_MAX + 1];
		nla_parse(tb, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0), NULL);
		if (!tb[NL802154_ATTR_IFINDEX] ||
		    nla_get_u32(tb[NL802154_ATTR_IFINDEX]) != sd->devidx)
			return 0;
	}

	sd->done = 1;
	return 0;
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

struct abort_info {
	char *cmd[3];
	struct nl802154_state *state;
	enum id_input id;
	struct scan_done *sd;
};

static struct abort_info abort_info = {
	.cmd = { NULL, "scan", "abort" },
};

static void scan_sigint_handler(int signo)
{
	/* dev <iface> scan abort */
	handle_cmd(abort_info.state, abort_info.id, 3, abort_info.cmd);
	abort_info.sd->done = 1;
	nl_socket_set_nonblocking(abort_info.state->nl_sock);
}

static int scan_handler(struct nl802154_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv,
			enum id_input id)
{
	struct scan_done sd;
	int ret, group;

	/* Abort scan upon SIGINT */
	abort_info.cmd[0] = argv[0];
	abort_info.state = state;
	abort_info.id = id;
	abort_info.sd = &sd;
	signal(SIGINT, scan_sigint_handler);

	/* Create netlink message to trigger the scan */
	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		return 2;
	}

	cb = nl_cb_alloc(iwpan_debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
	if (!cb) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		ret = 2;
		goto free_msg;
	}

	genlmsg_put(msg, 0, 0, state->nl802154_id, 0, 0,
		    NL802154_CMD_TRIGGER_SCAN, 0);

	sd.devidx = if_nametoindex(*argv);
	if (sd.devidx == 0)
		sd.devidx = -1;

	NLA_PUT_U32(msg, NL802154_ATTR_IFINDEX, sd.devidx);

	/* Skip "<iface> scan" */
	argc -= 2;
	argv += 2;

	/* Parse the "trigger" command */
	ret = scan_trigger_handler(state, cb, msg, argc, argv, id);
	if (ret)
		goto nla_put_failure;

	/* Configure socket to receive messages in Scan multicast group */
	group = genl_ctrl_resolve_grp(state->nl_sock, "nl802154", "scan");
	if (group < 0) {
		ret = group;
		goto nla_put_failure;
	}

	ret = nl_socket_add_membership(state->nl_sock, group);
	if (ret)
		goto nla_put_failure;

	/* Install scan message handler */
	nl_socket_set_cb(state->nl_sock, cb);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, scan_message_handler, &sd);

	/* No sequence checking for multicast messages */
	nl_socket_disable_seq_check(state->nl_sock);

	/* Trigger the scan */
	ret = nl_send_auto_complete(state->nl_sock, msg);
	if (ret < 0)
		goto nla_put_failure;

	ret = 0;

	/* Receive messages */
	sd.done = 0;
	do {
		nl_recvmsgs(state->nl_sock, cb);
	} while (!sd.done);

	flush_coords();

nla_put_failure:
	nl_cb_put(cb);
free_msg:
	nlmsg_free(msg);

	return ret;
}
TOPLEVEL(scan, "type <type> [page <page>] [channels <bitfield>] [duration <duration-order>]",
	0, 0, CIB_NETDEV, scan_handler,
	"Scan on this virtual interface with the given configuration.\n"
	SCAN_TYPES);
COMMAND(scan, abort, NULL, NL802154_CMD_ABORT_SCAN, 0, CIB_NETDEV, scan_abort_handler,
	"Abort ongoing scanning on this virtual interface");
COMMAND(scan, trigger,
	"type <type> [page <page>] [channels <bitfield>] [duration <duration-order>]",
	NL802154_CMD_TRIGGER_SCAN, 0, CIB_NETDEV, scan_trigger_handler,
	"Launch scanning on this virtual interface with the given configuration.\n"
	SCAN_TYPES);

SECTION(beacons);

static int send_beacons_handler(struct nl802154_state *state, struct nl_cb *cb,
				struct nl_msg *msg, int argc, char **argv,
				enum id_input id)
{
	unsigned long interval;
	bool valid_interval;
	int tpset;

	tpset = get_option_value(&argc, &argv, "interval", &interval, &valid_interval);
	if (tpset)
		return tpset;
	if (valid_interval && interval > UINT8_MAX)
		return 1;

	if (argc)
		return 1;

	/* Optional arguments */
	if (valid_interval)
		NLA_PUT_U8(msg, NL802154_ATTR_BEACON_INTERVAL, interval);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}

static int stop_beacons_handler(struct nl802154_state *state, struct nl_cb *cb,
				struct nl_msg *msg, int argc, char **argv,
				enum id_input id)
{
	return 0;
}

COMMAND(beacons, stop, NULL,
	NL802154_CMD_STOP_BEACONS, 0, CIB_NETDEV, stop_beacons_handler,
	"Stop sending beacons on this interface.");
COMMAND(beacons, send, "[interval <interval-order>]",
	NL802154_CMD_SEND_BEACONS, 0, CIB_NETDEV, send_beacons_handler,
	"Send beacons on this virtual interface at a regular pace.");
