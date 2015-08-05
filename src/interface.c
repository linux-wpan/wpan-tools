#include <net/if.h>
#include <errno.h>
#include <string.h>
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

SECTION(interface);

static char iftypebuf[100];

const char *iftype_name(enum nl802154_iftype iftype)
{
	switch (iftype) {
	case NL802154_IFTYPE_MONITOR:
		return "monitor";
	case NL802154_IFTYPE_NODE:
		return "node";
	case NL802154_IFTYPE_COORD:
		return "coordinator";
	default:
		sprintf(iftypebuf, "Invalid iftype (%d)", iftype);
		return iftypebuf;
	}
}

/* for help */
#define IFACE_TYPES "Valid interface types are: node, monitor, coordinator."

/* return 0 if ok, internal error otherwise */
static int get_if_type(int *argc, char ***argv, enum nl802154_iftype *type,
		       bool need_type)
{
	char *tpstr;

	if (*argc < 1 + !!need_type)
		return 1;

	if (need_type && strcmp((*argv)[0], "type"))
		return 1;

	tpstr = (*argv)[!!need_type];
	*argc -= 1 + !!need_type;
	*argv += 1 + !!need_type;

	if (strcmp(tpstr, "node") == 0) {
		*type = NL802154_IFTYPE_NODE;
		return 0;
	} else if (strcmp(tpstr, "monitor") == 0) {
		*type = NL802154_IFTYPE_MONITOR;
		return 0;
	} else if (strcmp(tpstr, "coordinator") == 0) {
		*type = NL802154_IFTYPE_COORD;
		return 0;
	}

	fprintf(stderr, "invalid interface type %s\n", tpstr);
	return 2;
}

#define EUI64_ALEN	8

static int extendedaddr_a2n(unsigned char *mac_addr, char *arg)
{
	int i;

	for (i = 0; i < EUI64_ALEN ; i++) {
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

		mac_addr[EUI64_ALEN - 1 - i] = temp;
		if (!cp)
			break;
		arg = cp;
	}
	if (i < EUI64_ALEN - 1)
		return -1;

	return 0;
}

/* return 0 if ok, internal error otherwise */
static int get_eui64(int *argc, char ***argv, void *eui64)
{
	int ret;

	if (*argc < 1)
		return 0;

	ret = extendedaddr_a2n(eui64, (*argv)[0]);
	if (ret) {
		fprintf(stderr, "invalid extended address\n");
		return 2;
	}


	*argc -= 1;
	*argv += 1;

	return 0;
}

static int handle_interface_add(struct nl802154_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv,
				enum id_input id)
{
	char *name;
	enum nl802154_iftype type;
	uint64_t eui64 = 0;
	int tpset;

	if (argc < 1)
		return 1;

	name = argv[0];
	argc--;
	argv++;

	tpset = get_if_type(&argc, &argv, &type, true);
	if (tpset)
		return tpset;

	tpset = get_eui64(&argc, &argv, &eui64);
	if (tpset)
		return tpset;

	if (argc)
		return 1;

	NLA_PUT_STRING(msg, NL802154_ATTR_IFNAME, name);
	NLA_PUT_U32(msg, NL802154_ATTR_IFTYPE, type);
	NLA_PUT_U64(msg, NL802154_ATTR_EXTENDED_ADDR, eui64);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(interface, add, "<name> type <type> [extended address <hex as 00:11:..>]",
	NL802154_CMD_NEW_INTERFACE, 0, CIB_PHY, handle_interface_add,
	"Add a new virtual interface with the given configuration.\n"
	IFACE_TYPES "\n\n");
COMMAND(interface, add, "<name> type <type> [extended address <hex as 00:11:..>]",
	NL802154_CMD_NEW_INTERFACE, 0, CIB_NETDEV, handle_interface_add, NULL);

static int handle_interface_del(struct nl802154_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv,
				enum id_input id)
{
	return 0;
}
TOPLEVEL(del, NULL, NL802154_CMD_DEL_INTERFACE, 0, CIB_NETDEV, handle_interface_del,
	 "Remove this virtual interface");
HIDDEN(interface, del, NULL, NL802154_CMD_DEL_INTERFACE, 0, CIB_NETDEV, handle_interface_del);

static int print_iface_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb_msg[NL802154_ATTR_MAX + 1];
	unsigned int *wpan_phy = arg;
	const char *indent = "";

	nla_parse(tb_msg, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (wpan_phy && tb_msg[NL802154_ATTR_WPAN_PHY]) {
		unsigned int thiswpan_phy = nla_get_u32(tb_msg[NL802154_ATTR_WPAN_PHY]);
		indent = "\t";
		if (*wpan_phy != thiswpan_phy)
			printf("phy#%d\n", thiswpan_phy);
		*wpan_phy = thiswpan_phy;
	}

	if (tb_msg[NL802154_ATTR_IFNAME])
		printf("%sInterface %s\n", indent, nla_get_string(tb_msg[NL802154_ATTR_IFNAME]));
	else
		printf("%sUnnamed/non-netdev interface\n", indent);

	if (tb_msg[NL802154_ATTR_IFINDEX])
		printf("%s\tifindex %d\n", indent, nla_get_u32(tb_msg[NL802154_ATTR_IFINDEX]));
	if (tb_msg[NL802154_ATTR_WPAN_DEV])
		printf("%s\twpan_dev 0x%llx\n", indent,
		       (unsigned long long)nla_get_u64(tb_msg[NL802154_ATTR_WPAN_DEV]));
	if (tb_msg[NL802154_ATTR_EXTENDED_ADDR])
		printf("%s\textended_addr 0x%016" PRIx64 "\n", indent,
		       le64toh(nla_get_u64(tb_msg[NL802154_ATTR_EXTENDED_ADDR])));
	if (tb_msg[NL802154_ATTR_SHORT_ADDR])
		printf("%s\tshort_addr 0x%04x\n", indent,
		       le16toh(nla_get_u16(tb_msg[NL802154_ATTR_SHORT_ADDR])));
	if (tb_msg[NL802154_ATTR_PAN_ID])
		printf("%s\tpan_id 0x%04x\n", indent,
		       le16toh(nla_get_u16(tb_msg[NL802154_ATTR_PAN_ID])));
	if (tb_msg[NL802154_ATTR_IFTYPE])
		printf("%s\ttype %s\n", indent, iftype_name(nla_get_u32(tb_msg[NL802154_ATTR_IFTYPE])));
	if (tb_msg[NL802154_ATTR_MAX_FRAME_RETRIES])
		printf("%s\tmax_frame_retries %d\n", indent, nla_get_s8(tb_msg[NL802154_ATTR_MAX_FRAME_RETRIES]));
	if (tb_msg[NL802154_ATTR_MIN_BE])
		printf("%s\tmin_be %d\n", indent, nla_get_u8(tb_msg[NL802154_ATTR_MIN_BE]));
	if (tb_msg[NL802154_ATTR_MAX_BE])
		printf("%s\tmax_be %d\n", indent, nla_get_u8(tb_msg[NL802154_ATTR_MAX_BE]));
	if (tb_msg[NL802154_ATTR_MAX_CSMA_BACKOFFS])
		printf("%s\tmax_csma_backoffs %d\n", indent, nla_get_u8(tb_msg[NL802154_ATTR_MAX_CSMA_BACKOFFS]));
	if (tb_msg[NL802154_ATTR_LBT_MODE])
		printf("%s\tlbt %d\n", indent, nla_get_u8(tb_msg[NL802154_ATTR_LBT_MODE]));
	if (tb_msg[NL802154_ATTR_ACKREQ_DEFAULT])
		printf("%s\tackreq_default %d\n", indent, nla_get_u8(tb_msg[NL802154_ATTR_ACKREQ_DEFAULT]));

	return NL_SKIP;
}

static int handle_interface_info(struct nl802154_state *state,
				 struct nl_cb *cb,
				 struct nl_msg *msg,
				 int argc, char **argv,
				 enum id_input id)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_iface_handler, NULL);
	return 0;
}
TOPLEVEL(info, NULL, NL802154_CMD_GET_INTERFACE, 0, CIB_NETDEV, handle_interface_info,
	 "Show information for this interface.");

static unsigned int dev_dump_wpan_phy;

static int handle_dev_dump(struct nl802154_state *state,
			   struct nl_cb *cb,
			   struct nl_msg *msg,
			   int argc, char **argv,
			   enum id_input id)
{
	dev_dump_wpan_phy = -1;
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_iface_handler, &dev_dump_wpan_phy);
	return 0;
}
TOPLEVEL(dev, NULL, NL802154_CMD_GET_INTERFACE, NLM_F_DUMP, CIB_NONE, handle_dev_dump,
	 "List all network interfaces for wireless hardware.");
