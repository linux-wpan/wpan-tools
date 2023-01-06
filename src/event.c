#include <net/if.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl802154.h"
#include "nl_extras.h"
#include "iwpan.h"

struct print_event_args {
	struct timeval ts; /* internal */
	bool have_ts; /* must be set false */
	bool frame, time, reltime;
};

static int print_event(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL802154_ATTR_MAX + 1], *nst, *nestedcoord;
	struct nlattr *pan[NL802154_COORD_MAX + 1];
	struct print_event_args *args = arg;
	char ifname[100];
	static struct nla_policy pan_policy[NL802154_COORD_MAX + 1] = {
		[NL802154_COORD_PANID] = { .type = NLA_U16, },
		[NL802154_COORD_ADDR] = { .minlen = 2, .maxlen = 8, }, /* 2 or 8 */
	};
	uint8_t reg_type;
	uint32_t wpan_phy_idx = 0;
	int rem_nst;
	uint16_t status;
	int ret;

	if (args->time || args->reltime) {
		unsigned long long usecs, previous;

		previous = 1000000ULL * args->ts.tv_sec + args->ts.tv_usec;
		gettimeofday(&args->ts, NULL);
		usecs = 1000000ULL * args->ts.tv_sec + args->ts.tv_usec;
		if (args->reltime) {
			if (!args->have_ts) {
				usecs = 0;
				args->have_ts = true;
			} else
				usecs -= previous;
		}
		printf("%llu.%06llu: ", usecs/1000000, usecs % 1000000);
	}

	nla_parse(tb, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL802154_ATTR_IFINDEX] && tb[NL802154_ATTR_WPAN_PHY]) {
		if_indextoname(nla_get_u32(tb[NL802154_ATTR_IFINDEX]), ifname);
		printf("%s (phy #%d): ", ifname, nla_get_u32(tb[NL802154_ATTR_WPAN_PHY]));
	} else if (tb[NL802154_ATTR_WPAN_DEV] && tb[NL802154_ATTR_WPAN_PHY]) {
		printf("wdev 0x%llx (phy #%d): ",
			(unsigned long long)nla_get_u64(tb[NL802154_ATTR_WPAN_DEV]),
			nla_get_u32(tb[NL802154_ATTR_WPAN_PHY]));
	} else if (tb[NL802154_ATTR_IFINDEX]) {
		if_indextoname(nla_get_u32(tb[NL802154_ATTR_IFINDEX]), ifname);
		printf("%s: ", ifname);
	} else if (tb[NL802154_ATTR_WPAN_DEV]) {
		printf("wdev 0x%llx: ", (unsigned long long)nla_get_u64(tb[NL802154_ATTR_WPAN_DEV]));
	} else if (tb[NL802154_ATTR_WPAN_PHY]) {
		printf("phy #%d: ", nla_get_u32(tb[NL802154_ATTR_WPAN_PHY]));
	}

	switch (gnlh->cmd) {
	case NL802154_CMD_NEW_WPAN_PHY:
		printf("renamed to %s\n", nla_get_string(tb[NL802154_ATTR_WPAN_PHY_NAME]));
		break;
	case NL802154_CMD_DEL_WPAN_PHY:
		printf("delete wpan_phy\n");
		break;
	case NL802154_CMD_TRIGGER_SCAN:
		printf("scan started\n");
		break;
	case NL802154_CMD_SCAN_DONE:
		if (tb[NL802154_ATTR_SCAN_DONE_REASON])
			status = nla_get_u8(tb[NL802154_ATTR_SCAN_DONE_REASON]);
		if (status == NL802154_SCAN_DONE_REASON_ABORTED)
			printf("scan aborted\n");
		else
			printf("scan finished\n");
		break;
	case NL802154_CMD_ABORT_SCAN:
		printf("scan aborted\n");
		break;
	case NL802154_CMD_SCAN_EVENT:
		nestedcoord = tb[NL802154_ATTR_COORDINATOR];
		if (!nestedcoord)
			break;
		ret = nla_parse_nested(pan, NL802154_COORD_MAX, nestedcoord, pan_policy);
		if (ret < 0)
			break;
		if (!pan[NL802154_COORD_PANID])
			break;
		printf("beacon received: PAN 0x%04x",
		       le16toh(nla_get_u16(pan[NL802154_COORD_PANID])));
		if (pan[NL802154_COORD_ADDR]) {
			struct nlattr *coord = pan[NL802154_COORD_ADDR];
			if (nla_len(coord) == 2) {
				uint16_t addr = nla_get_u16(coord);
				printf(", addr 0x%04x\n", le16toh(addr));
			} else {
				uint64_t addr = nla_get_u64(coord);
				printf(", addr 0x%016" PRIx64 "\n", le64toh(addr));
			}
		}
		break;
	default:
		printf("unknown event %d\n", gnlh->cmd);
		break;
	}
	fflush(stdout);
	return NL_SKIP;
}

static int __prepare_listen_events(struct nl802154_state *state)
{
	int mcid, ret;

	/* Configuration multicast group */
	mcid = genl_ctrl_resolve_grp(state->nl_sock, NL802154_GENL_NAME,
				     "config");
	if (mcid < 0)
		return mcid;
	ret = nl_socket_add_membership(state->nl_sock, mcid);
	if (ret)
		return ret;

	/* Scan multicast group */
	mcid = genl_ctrl_resolve_grp(state->nl_sock, NL802154_GENL_NAME,
				     "scan");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state->nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* MLME multicast group */
	mcid = genl_ctrl_resolve_grp(state->nl_sock, NL802154_GENL_NAME,
				     "mlme");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state->nl_sock, mcid);
		if (ret)
			return ret;
	}

	return 0;
}

static int __do_listen_events(struct nl802154_state *state,
			      struct print_event_args *args)
{
	struct nl_cb *cb = nl_cb_alloc(iwpan_debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
	if (!cb) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		return -ENOMEM;
	}
	nl_socket_set_cb(state->nl_sock, cb);
	/* No sequence checking for multicast messages */
	nl_socket_disable_seq_check(state->nl_sock);
	/* Install print_event message handler */
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_event, args);

	/* Loop waiting until interrupted by signal */
	while (1) {
		int ret = nl_recvmsgs(state->nl_sock, cb);
		if (ret) {
			fprintf(stderr, "nl_recvmsgs return error %d\n", ret);
			break;
		}
	}
	/* Free allocated nl_cb structure */
	nl_cb_put(cb);
	return 0;
}

static int print_events(struct nl802154_state *state,
			struct nl_cb *cb,
			struct nl_msg *msg,
			int argc, char **argv,
			enum id_input id)
{
	struct print_event_args args;
	int ret;

	memset(&args, 0, sizeof(args));

	argc--;
	argv++;

	while (argc > 0) {
		if (strcmp(argv[0], "-f") == 0)
			args.frame = true;
		else if (strcmp(argv[0], "-t") == 0)
			args.time = true;
		else if (strcmp(argv[0], "-r") == 0)
			args.reltime = true;
		else
			return 1;
		argc--;
		argv++;
	}
	if (args.time && args.reltime)
		return 1;
	if (argc)
		return 1;

	/* Prepare reception of all multicast messages */
	ret = __prepare_listen_events(state);
	if (ret)
		return ret;

	/* Read message loop */
	return __do_listen_events(state, &args);
}
TOPLEVEL(monitor, "[-t|-r] [-f]", 0, 0, CIB_NONE, print_events,
	"Monitor events from the kernel.\n"
	"-t - print timestamp\n"
	"-r - print relative timestamp\n"
	"-f - print full frame for auth/assoc etc.");
