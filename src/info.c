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

static void print_minmax_handler(int min, int max)
{
	int i;

	for (i = min; i <= max; i++)
		printf("%d,", i);

	/* TODO */
	printf("\b \n");
}

static int print_phy_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL802154_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	int rem_page, i, ret;
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

	/* TODO remove this handling it's deprecated */
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
		printf("tx_power: %.3g\n", MBM_TO_DBM(nla_get_s32(tb_msg[NL802154_ATTR_TX_POWER])));

	if (tb_msg[NL802154_ATTR_WPAN_PHY_CAPS]) {
		struct nlattr *tb_caps[NL802154_CAP_ATTR_MAX + 1];
		/* TODO fix netlink lib that we can use NLA_S32 here
		 * see function  validate_nla line if (pt->type > NLA_TYPE_MAX) */
		static struct nla_policy caps_policy[NL802154_CAP_ATTR_MAX + 1] = {
			[NL802154_CAP_ATTR_CHANNELS] = { .type = NLA_NESTED },
			[NL802154_CAP_ATTR_TX_POWERS] = { .type = NLA_NESTED },
			[NL802154_CAP_ATTR_CCA_ED_LEVELS] = { .type = NLA_NESTED },
			[NL802154_CAP_ATTR_CCA_MODES] = { .type = NLA_NESTED },
			[NL802154_CAP_ATTR_CCA_OPTS] = { .type = NLA_NESTED },
			[NL802154_CAP_ATTR_MIN_MINBE] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MAX_MINBE] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MIN_MAXBE] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MAX_MAXBE] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MIN_CSMA_BACKOFFS] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MAX_CSMA_BACKOFFS] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MIN_FRAME_RETRIES] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_MAX_FRAME_RETRIES] = { .type = NLA_U8 },
			[NL802154_CAP_ATTR_IFTYPES] = { .type = NLA_NESTED },
			[NL802154_CAP_ATTR_LBT] = { .type = NLA_U32 },
		};

		printf("capabilities:\n");

		ret = nla_parse_nested(tb_caps, NL802154_CAP_ATTR_MAX,
				       tb_msg[NL802154_ATTR_WPAN_PHY_CAPS],
				       caps_policy);
		if (ret) {
			printf("failed to parse caps\n");
			return -EIO;
		}

		if (tb_caps[NL802154_CAP_ATTR_IFTYPES]) {
			struct nlattr *nl_iftypes;
			int rem_iftypes;
			printf("\tiftypes: ");
			nla_for_each_nested(nl_iftypes,
					    tb_caps[NL802154_CAP_ATTR_IFTYPES],
					    rem_iftypes)
				printf("%s,", iftype_name(nla_type(nl_iftypes)));
			/* TODO */
			printf("\b \n");
		}

		if (tb_msg[NL802154_CAP_ATTR_CHANNELS]) {
			int rem_pages;
			struct nlattr *nl_pages;
			printf("\tchannels:\n");
			nla_for_each_nested(nl_pages, tb_caps[NL802154_CAP_ATTR_CHANNELS],
					    rem_pages) {
				int rem_channels;
				struct nlattr *nl_channels;
				printf("\t\tpage %d: ", nla_type(nl_pages));
				nla_for_each_nested(nl_channels, nl_pages, rem_channels)
					printf("%d,", nla_type(nl_channels));
				/*  TODO hack use sprintf here */
				printf("\b \b\n");
			}
		}

		if (tb_caps[NL802154_CAP_ATTR_TX_POWERS]) {
			int rem_pwrs;
			struct nlattr *nl_pwrs;

			printf("\ttx_powers: ");
			nla_for_each_nested(nl_pwrs, tb_caps[NL802154_CAP_ATTR_TX_POWERS], rem_pwrs)
				printf("%.3g,", MBM_TO_DBM(nla_get_s32(nl_pwrs)));
			/* TODO */
			printf("\b \n");
		}

		if (tb_caps[NL802154_CAP_ATTR_CCA_ED_LEVELS]) {
			int rem_levels;
			struct nlattr *nl_levels;

			printf("\tcca_ed_levels: ");
			nla_for_each_nested(nl_levels, tb_caps[NL802154_CAP_ATTR_CCA_ED_LEVELS], rem_levels)
				printf("%.3g,", MBM_TO_DBM(nla_get_s32(nl_levels)));
			/* TODO */
			printf("\b \n");
		}

		if (tb_caps[NL802154_CAP_ATTR_CCA_MODES]) {
			struct nlattr *nl_cca_modes;
			int rem_cca_modes;
			printf("\tcca_modes: ");
			nla_for_each_nested(nl_cca_modes,
					    tb_caps[NL802154_CAP_ATTR_CCA_MODES],
					    rem_cca_modes)
				printf("%d,", nla_type(nl_cca_modes));
			/* TODO */
			printf("\b \n");
		}

		if (tb_caps[NL802154_CAP_ATTR_CCA_OPTS]) {
			struct nlattr *nl_cca_opts;
			int rem_cca_opts;

			printf("\tcca_opts: ");
			nla_for_each_nested(nl_cca_opts,
					    tb_caps[NL802154_CAP_ATTR_CCA_OPTS],
					    rem_cca_opts) {
				printf("%d", nla_type(nl_cca_opts));
				switch (nla_type(nl_cca_opts)) {
				case NL802154_CCA_OPT_ENERGY_CARRIER_AND:
				case NL802154_CCA_OPT_ENERGY_CARRIER_OR:
					printf("(cca_mode: 3),");
					break;
				default:
					printf("unkown\n");
					break;
				}
			}
			/* TODO */
			printf("\b \n");
		}

		if (tb_caps[NL802154_CAP_ATTR_MIN_MINBE] &&
		    tb_caps[NL802154_CAP_ATTR_MAX_MINBE] &&
		    tb_caps[NL802154_CAP_ATTR_MIN_MAXBE] &&
		    tb_caps[NL802154_CAP_ATTR_MAX_MAXBE]) {
			printf("\tmin_be: ");
			print_minmax_handler(nla_get_u8(tb_caps[NL802154_CAP_ATTR_MIN_MINBE]),
					     nla_get_u8(tb_caps[NL802154_CAP_ATTR_MAX_MINBE]));
			printf("\tmax_be: ");
			print_minmax_handler(nla_get_u8(tb_caps[NL802154_CAP_ATTR_MIN_MAXBE]),
					     nla_get_u8(tb_caps[NL802154_CAP_ATTR_MAX_MAXBE]));
		}

		if (tb_caps[NL802154_CAP_ATTR_MIN_CSMA_BACKOFFS] &&
		    tb_caps[NL802154_CAP_ATTR_MAX_CSMA_BACKOFFS]) {
			printf("\tcsma_backoffs: ");
			print_minmax_handler(nla_get_u8(tb_caps[NL802154_CAP_ATTR_MIN_CSMA_BACKOFFS]),
					     nla_get_u8(tb_caps[NL802154_CAP_ATTR_MAX_CSMA_BACKOFFS]));
		}

		if (tb_caps[NL802154_CAP_ATTR_MIN_FRAME_RETRIES] &&
		    tb_caps[NL802154_CAP_ATTR_MAX_FRAME_RETRIES]) {
			printf("\tframe_retries: ");
			print_minmax_handler(nla_get_s8(tb_caps[NL802154_CAP_ATTR_MIN_FRAME_RETRIES]),
					     nla_get_s8(tb_caps[NL802154_CAP_ATTR_MAX_FRAME_RETRIES]));
		}

		if (tb_caps[NL802154_CAP_ATTR_LBT]) {
			printf("\tlbt: ");
			switch (nla_get_u32(tb_caps[NL802154_CAP_ATTR_LBT])) {
			case NL802154_SUPPORTED_BOOL_FALSE:
				printf("false\n");
				break;
			case NL802154_SUPPORTED_BOOL_TRUE:
				printf("true\n");
				break;
			case NL802154_SUPPORTED_BOOL_BOTH:
				printf("false,true\n");
				break;
			default:
				printf("unkown\n");
				break;
			}
		}
	}

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
