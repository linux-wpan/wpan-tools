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
#define CONFIG_IEEE802154_NL802154_EXPERIMENTAL
#include "nl802154.h"
#include "iwpan.h"

SECTION(node);

static int print_node_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL802154_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *ninfo[NL802154_NODE_INFO_ATTR_MAX + 1];
	static struct nla_policy stats_policy[NL802154_NODE_INFO_ATTR_MAX + 1] = {
		[NL802154_NODE_INFO_ATTR_LQI] = { .type = NLA_U8 },
	};

	nla_parse(tb, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL802154_ATTR_EXTENDED_ADDR])
		printf("extended addr: %llx\n", le64toh(nla_get_u64(tb[NL802154_ATTR_EXTENDED_ADDR])));

	if (!tb[NL802154_ATTR_NODE_INFO]) {
		fprintf(stderr, "sta stats missing!\n");
		return NL_SKIP;
	}

	if (nla_parse_nested(ninfo, NL802154_NODE_INFO_ATTR_MAX,
			     tb[NL802154_ATTR_NODE_INFO],
			     stats_policy)) {
		fprintf(stderr, "failed to parse nested attributes!\n");
		return NL_SKIP;
	}

	if (ninfo[NL802154_NODE_INFO_ATTR_LQI])
		printf("lqi: %x ", nla_get_u8(ninfo[NL802154_NODE_INFO_ATTR_LQI]));

	if (ninfo[NL802154_NODE_INFO_ATTR_TX_SUCCESS])
		printf("tx: %llu ", nla_get_u64(ninfo[NL802154_NODE_INFO_ATTR_TX_SUCCESS]));

	if (ninfo[NL802154_NODE_INFO_ATTR_TX_NO_ACK])
		printf("no ack: %llu ", nla_get_u64(ninfo[NL802154_NODE_INFO_ATTR_TX_NO_ACK]));

	printf("\n");

	return NL_SKIP;
}

static int handle_node_dump(struct nl802154_state *state,
			    struct nl_cb *cb,
			    struct nl_msg *msg,
			    int argc, char **argv,
			    enum id_input id)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_node_handler, NULL);
	return 0;
}
COMMAND(node, dump, NULL,
	NL802154_CMD_GET_NODE, NLM_F_DUMP, CIB_NETDEV, handle_node_dump,
	"List all stations known, e.g. the AP on managed interfaces");
