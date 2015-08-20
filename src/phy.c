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

static int handle_channel_set(struct nl802154_state *state,
			      struct nl_cb *cb,
			      struct nl_msg *msg,
			      int argc, char **argv,
			      enum id_input id)
{
	unsigned long channel;
	unsigned long page;
	char *end;

	if (argc < 2)
		return 1;

	/* PAGE */
	page = strtoul(argv[0], &end, 10);
	if (*end != '\0')
		return 1;

	/* CHANNEL */
	channel = strtoul(argv[1], &end, 10);
	if (*end != '\0')
		return 1;

	if (page > UINT8_MAX || channel > UINT8_MAX)
		return 1;

	NLA_PUT_U8(msg, NL802154_ATTR_PAGE, page);
	NLA_PUT_U8(msg, NL802154_ATTR_CHANNEL, channel);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, channel, "<page> <channel>",
	NL802154_CMD_SET_CHANNEL, 0, CIB_PHY, handle_channel_set, NULL);

static int handle_tx_power_set(struct nl802154_state *state,
			       struct nl_cb *cb,
			       struct nl_msg *msg,
			       int argc, char **argv,
			       enum id_input id)
{
	float dbm;
	char *end;

	if (argc < 1)
		return 1;

	/* TX_POWER */
	dbm = strtof(argv[0], &end);
	if (*end != '\0')
		return 1;

	NLA_PUT_S32(msg, NL802154_ATTR_TX_POWER, DBM_TO_MBM(dbm));

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, tx_power, "<dBm>",
	NL802154_CMD_SET_TX_POWER, 0, CIB_PHY, handle_tx_power_set, NULL);

static int handle_cca_mode_set(struct nl802154_state *state,
			       struct nl_cb *cb,
			       struct nl_msg *msg,
			       int argc, char **argv,
			       enum id_input id)
{
	enum nl802154_cca_modes cca_mode;
	char *end;

	if (argc < 1)
		return 1;

	/* CCA_MODE */
	cca_mode = strtoul(argv[0], &end, 10);
	if (*end != '\0')
		return 1;

	if (cca_mode == NL802154_CCA_ENERGY_CARRIER) {
		enum nl802154_cca_opts cca_opt;

		if (argc < 2)
			return 1;

		/* CCA_OPT */
		cca_opt = strtoul(argv[1], &end, 10);
		if (*end != '\0')
			return 1;

		NLA_PUT_U32(msg, NL802154_ATTR_CCA_OPT, cca_opt);
	}

	NLA_PUT_U32(msg, NL802154_ATTR_CCA_MODE, cca_mode);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, cca_mode, "<mode|3 <1|0>>",
	NL802154_CMD_SET_CCA_MODE, 0, CIB_PHY, handle_cca_mode_set, NULL);

static int handle_cca_ed_level(struct nl802154_state *state,
			       struct nl_cb *cb,
			       struct nl_msg *msg,
			       int argc, char **argv,
			       enum id_input id)
{
	float level;
	char *end;

	if (argc < 1)
		return 1;

	/* CCA_ED_LEVEL */
	level = strtof(argv[0], &end);
	if (*end != '\0')
		return 1;

	NLA_PUT_S32(msg, NL802154_ATTR_CCA_ED_LEVEL, DBM_TO_MBM(level));

	return 0;

nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, cca_ed_level, "<level>",
	NL802154_CMD_SET_CCA_ED_LEVEL, 0, CIB_PHY, handle_cca_ed_level, NULL);
