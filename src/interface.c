#include <net/if.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl802154.h"
#include "iwpan.h"

SECTION(interface);

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

static int handle_interface_add(struct nl802154_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv,
				enum id_input id)
{
	char *name;
	enum nl802154_iftype type;
	int tpset;

	if (argc < 1)
		return 1;

	name = argv[0];
	argc--;
	argv++;

	tpset = get_if_type(&argc, &argv, &type, true);
	if (tpset)
		return tpset;

	if (argc)
		return 1;

	NLA_PUT_STRING(msg, NL802154_ATTR_IFNAME, name);
	NLA_PUT_U32(msg, NL802154_ATTR_IFTYPE, type);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(interface, add, "<name> type <type>",
	NL802154_CMD_NEW_INTERFACE, 0, CIB_PHY, handle_interface_add,
	"Add a new virtual interface with the given configuration.\n"
	IFACE_TYPES "\n\n");
COMMAND(interface, add, "<name> type <type>",
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
