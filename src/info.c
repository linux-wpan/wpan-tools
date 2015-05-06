#include <stdbool.h>
#include <errno.h>
#include <net/if.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl802154.h"
#include "nl_extras.h"
#include "iwpan.h"

static int print_phy_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL802154_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	int rem_page, i;
	int64_t phy_id = -1;
	bool print_name = true;
	struct nlattr *nl_page;
	enum nl802154_cca_modes cca_mode;

	nla_parse(tb_msg, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb_msg[NL802154_ATTR_WPAN_PHY]) {
		if (nla_get_u32(tb_msg[NL802154_ATTR_WPAN_PHY]) == phy_id)
			print_name = false;
		phy_id = nla_get_u32(tb_msg[NL802154_ATTR_WPAN_PHY]);
	}
	if (print_name && tb_msg[NL802154_ATTR_WPAN_PHY_NAME])
		printf("wpan_phy %s\n", nla_get_string(tb_msg[NL802154_ATTR_WPAN_PHY_NAME]));

	if (tb_msg[NL802154_ATTR_CHANNELS_SUPPORTED]) {
		unsigned char page = 0;
		unsigned long channel;
		printf("supported channels:\n");
		nla_for_each_nested(nl_page,
				    tb_msg[NL802154_ATTR_CHANNELS_SUPPORTED],
				    rem_page) {
			channel = nla_get_u32(nl_page);
			if (channel) {
				printf("\tpage %d: ", page, channel);
				for (i = 0; i <= 31; i++) {
					if (channel & 0x1)
						printf("%d,", i);
					channel >>= 1;
				}
				/* TODO hack use sprintf here */
				printf("\b \b\n");
			}
			page++;
		}
	}

	if (tb_msg[NL802154_ATTR_PAGE])
		printf("current_page: %d\n", nla_get_u8(tb_msg[NL802154_ATTR_PAGE]));

	if (tb_msg[NL802154_ATTR_CHANNEL])
		printf("current_channel: %d\n", nla_get_u8(tb_msg[NL802154_ATTR_CHANNEL]));

	if (tb_msg[NL802154_ATTR_CCA_MODE]) {
		cca_mode = nla_get_u32(tb_msg[NL802154_ATTR_CCA_MODE]);
		printf("cca_mode: %d", cca_mode);
		if (cca_mode == NL802154_CCA_ENERGY_CARRIER) {
			enum nl802154_cca_opts cca_opt;

			cca_opt = nla_get_u32(tb_msg[NL802154_ATTR_CCA_OPT]);
			switch (cca_opt) {
			case NL802154_CCA_OPT_ENERGY_CARRIER_AND:
				printf(" logical and ");
				break;
			case NL802154_CCA_OPT_ENERGY_CARRIER_OR:
				printf(" logical or ");
				break;
			default:
				printf(" logical op mode unkown ");
			}
		}
		printf("\n");
	}

	if (tb_msg[NL802154_ATTR_TX_POWER])
		printf("tx_power: %.2g\n", MBM_TO_DBM(nla_get_s32(tb_msg[NL802154_ATTR_TX_POWER])));

	return 0;
}

static int handle_info(struct nl802154_state *state,
		       struct nl_cb *cb,
		       struct nl_msg *msg,
		       int argc, char **argv,
		       enum id_input id)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_phy_handler, NULL);

	return 0;
}

__COMMAND(NULL, info, "info", NULL, NL802154_CMD_GET_WPAN_PHY, 0, 0, CIB_PHY, handle_info,
	  "Show capabilities for the specified wireless device.", NULL);
TOPLEVEL(list, NULL, NL802154_CMD_GET_WPAN_PHY, NLM_F_DUMP, CIB_NONE, handle_info,
	 "List all wireless devices and their capabilities.");
TOPLEVEL(phy, NULL, NL802154_CMD_GET_WPAN_PHY, NLM_F_DUMP, CIB_NONE, handle_info, NULL);
