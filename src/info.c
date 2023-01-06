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

static void print_freq_handler(int channel_page, int channel)
{
	float freq = 0;

	switch (channel_page) {
	case 0:
		if (channel == 0) {
			freq = 868.3;
			printf("%5.1f", freq);
			break;
		} else if (channel > 0 && channel < 11) {
			freq = 906 + 2 * (channel - 1);
		} else {
			freq = 2405 + 5 * (channel - 11);
		}
		printf("%5.0f", freq);
		break;
	case 1:
		if (channel == 0) {
			freq = 868.3;
			printf("%5.1f", freq);
			break;
		} else if (channel >= 1 && channel <= 10) {
			freq = 906 + 2 * (channel - 1);
		}
		printf("%5.0f", freq);
		break;
	case 2:
		if (channel == 0) {
			freq = 868.3;
			printf("%5.1f", freq);
			break;
		} else if (channel >= 1 && channel <= 10) {
			freq = 906 + 2 * (channel - 1);
		}
		printf("%5.0f", freq);
		break;
	case 3:
		if (channel >= 0 && channel <= 12) {
			freq = 2412 + 5 * channel;
		} else if (channel == 13) {
			freq = 2484;
		}
		printf("%4.0f", freq);
		break;
	case 4:
		switch (channel) {
		case 0:
			freq = 499.2;
			break;
		case 1:
			freq = 3494.4;
			break;
		case 2:
			freq = 3993.6;
			break;
		case 3:
			freq = 4492.8;
			break;
		case 4:
			freq = 3993.6;
			break;
		case 5:
			freq = 6489.6;
			break;
		case 6:
			freq = 6988.8;
			break;
		case 7:
			freq = 6489.6;
			break;
		case 8:
			freq = 7488.0;
			break;
		case 9:
			freq = 7987.2;
			break;
		case 10:
			freq = 8486.4;
			break;
		case 11:
			freq = 7987.2;
			break;
		case 12:
			freq = 8985.6;
			break;
		case 13:
			freq = 9484.8;
			break;
		case 14:
			freq = 9984.0;
			break;
		case 15:
			freq = 9484.8;
			break;
		}
		printf("%6.1f", freq);
		break;
	case 5:
		if (channel >= 0 && channel <= 3) {
			freq = 780 + 2 * channel;
		} else if (channel >= 4 && channel <= 7) {
			freq = 780 + 2 * (channel - 4);
		}
		printf("%3.0f", freq);
		break;
	case 6:
		if (channel >= 0 && channel <= 7) {
			freq = 951.2 + 0.6 * channel;
		} else if (channel >= 8 && channel <= 9) {
			freq = 954.4 + 0.2 * (channel - 8);
		} else if (channel >= 10  && channel <= 21) {
			freq = 951.1 + 0.4 * (channel - 10);
		}

		printf("%5.1f", freq);
		break;
	default:
		printf("Unknown");
		break;
	}
}

static char cca_mode_buf[100];

static const char *print_cca_mode_handler(enum nl802154_cca_modes cca_mode,
					  enum nl802154_cca_opts cca_opt)
{
	switch (cca_mode) {
	case NL802154_CCA_ENERGY:
		sprintf(cca_mode_buf,"(%d) %s", cca_mode, "Energy above threshold");
		break;
	case NL802154_CCA_CARRIER:
		sprintf(cca_mode_buf,"(%d) %s", cca_mode, "Carrier sense only");
		break;
	case NL802154_CCA_ENERGY_CARRIER:
		switch (cca_opt) {
		case NL802154_CCA_OPT_ENERGY_CARRIER_AND:
			sprintf(cca_mode_buf, "(%d, cca_opt: %d) %s", cca_mode, cca_opt,
				"Carrier sense with energy above threshold (logical operator is 'and')");
			break;
		case NL802154_CCA_OPT_ENERGY_CARRIER_OR:
			sprintf(cca_mode_buf, "(%d, cca_opt: %d) %s", cca_mode, cca_opt,
				"Carrier sense with energy above threshold (logical operator is 'or')");
			break;
		default:
			sprintf(cca_mode_buf, "Unknown CCA option (%d) for CCA mode (%d)",
				cca_opt, cca_mode);
			break;
		}
		break;
	case NL802154_CCA_ALOHA:
		sprintf(cca_mode_buf,"(%d) %s", cca_mode, "ALOHA");
		break;
	case NL802154_CCA_UWB_SHR:
		sprintf(cca_mode_buf,"(%d) %s", cca_mode,
			"UWB preamble sense based on the SHR of a frame");
		break;
	case NL802154_CCA_UWB_MULTIPLEXED:
		sprintf(cca_mode_buf,"(%d) %s", cca_mode,
			"UWB preamble sense based on the packet with the multiplexed preamble");
		break;
	default:
		sprintf(cca_mode_buf, "Unknown CCA mode (%d)", cca_mode);
		break;
	}
	return cca_mode_buf;
}

static const char *commands[NL802154_CMD_MAX + 1] = {
	[NL802154_CMD_UNSPEC] = "unspec",
	[NL802154_CMD_GET_WPAN_PHY] = "get_wpan_phy",
	[NL802154_CMD_SET_WPAN_PHY] = "set_wpan_phy",
	[NL802154_CMD_NEW_WPAN_PHY] = "new_wpan_phy",
	[NL802154_CMD_DEL_WPAN_PHY] = "del_wpan_phy",
	[NL802154_CMD_GET_INTERFACE] = "get_interface",
	[NL802154_CMD_SET_INTERFACE] = "set_interface",
	[NL802154_CMD_NEW_INTERFACE] = "new_interface",
	[NL802154_CMD_DEL_INTERFACE] = "del_interface",
	[NL802154_CMD_SET_CHANNEL] = "set_channel",
	[NL802154_CMD_SET_PAN_ID] = "set_pan_id",
	[NL802154_CMD_SET_SHORT_ADDR] = "set_short_addr",
	[NL802154_CMD_SET_TX_POWER] = "set_tx_power",
	[NL802154_CMD_SET_CCA_MODE] = "set_cca_mode",
	[NL802154_CMD_SET_CCA_ED_LEVEL] = "set_cca_ed_level",
	[NL802154_CMD_SET_MAX_FRAME_RETRIES] = "set_max_frame_retries",
	[NL802154_CMD_SET_BACKOFF_EXPONENT] = "set_backoff_exponent",
	[NL802154_CMD_SET_MAX_CSMA_BACKOFFS] = "set_max_csma_backoffs",
	[NL802154_CMD_SET_LBT_MODE] = "set_lbt_mode",
	[NL802154_CMD_SET_ACKREQ_DEFAULT] = "set_ackreq_default",
};

static char cmdbuf[100];

static const char *command_name(enum nl802154_commands cmd)
{
	if (cmd <= NL802154_CMD_MAX && commands[cmd])
		return commands[cmd];

	sprintf(cmdbuf, "Unknown command (%d)", cmd);
	return cmdbuf;
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
				printf("\tpage %d: ", page);
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

	if (tb_msg[NL802154_ATTR_CHANNEL] &&
	    tb_msg[NL802154_ATTR_PAGE]) {
		printf("current_channel: %d, ", nla_get_u8(tb_msg[NL802154_ATTR_CHANNEL]));
		print_freq_handler(nla_get_u8(tb_msg[NL802154_ATTR_PAGE]),
				   nla_get_u8(tb_msg[NL802154_ATTR_CHANNEL]));
		printf(" MHz\n");
	}

	if (tb_msg[NL802154_ATTR_CCA_MODE]) {
		enum nl802154_cca_opts cca_opt = NL802154_CCA_OPT_ATTR_MAX;
		if (tb_msg[NL802154_ATTR_CCA_OPT])
			cca_opt = nla_get_u32(tb_msg[NL802154_ATTR_CCA_OPT]);

		cca_mode = nla_get_u32(tb_msg[NL802154_ATTR_CCA_MODE]);

		printf("cca_mode: %s", print_cca_mode_handler(cca_mode, cca_opt));
		printf("\n");
	}

	if (tb_msg[NL802154_ATTR_CCA_ED_LEVEL])
		printf("cca_ed_level: %.3g\n", MBM_TO_DBM(nla_get_s32(tb_msg[NL802154_ATTR_CCA_ED_LEVEL])));

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

		if (tb_caps[NL802154_CAP_ATTR_CHANNELS]) {
			int counter = 0;
			int rem_pages;
			struct nlattr *nl_pages;
			printf("\tchannels:\n");
			nla_for_each_nested(nl_pages, tb_caps[NL802154_CAP_ATTR_CHANNELS],
					    rem_pages) {
				int rem_channels;
				struct nlattr *nl_channels;
				counter = 0;
				printf("\t\tpage %d: ", nla_type(nl_pages));
				nla_for_each_nested(nl_channels, nl_pages, rem_channels) {
					if (counter % 3 == 0) {
						printf("\n\t\t\t[%2d] ", nla_type(nl_channels));
						print_freq_handler(nla_type(nl_pages), nla_type(nl_channels));
						printf(" MHz, ");
					} else {
						printf("[%2d] ", nla_type(nl_channels));
						print_freq_handler(nla_type(nl_pages), nla_type(nl_channels));
						printf(" MHz, ");
					}
					counter++;
				}
				/*  TODO hack use sprintf here */
				printf("\b\b \b\n");
			}
		}

		if (tb_caps[NL802154_CAP_ATTR_TX_POWERS]) {
			int rem_pwrs;
			int counter = 0;
			struct nlattr *nl_pwrs;

			printf("\ttx_powers: ");
			nla_for_each_nested(nl_pwrs, tb_caps[NL802154_CAP_ATTR_TX_POWERS], rem_pwrs) {
				if (counter % 6 == 0) {
					printf("\n\t\t\t%.3g dBm, ", MBM_TO_DBM(nla_get_s32(nl_pwrs)));
				} else {
					printf("%.3g dBm, ", MBM_TO_DBM(nla_get_s32(nl_pwrs)));
				}
				counter++;
			}
			/* TODO */
			printf("\b \n");
		}

		if (tb_caps[NL802154_CAP_ATTR_CCA_ED_LEVELS]) {
			int rem_levels;
			int counter = 0;
			struct nlattr *nl_levels;

			printf("\tcca_ed_levels: ");
			nla_for_each_nested(nl_levels, tb_caps[NL802154_CAP_ATTR_CCA_ED_LEVELS], rem_levels) {
				if (counter % 6 == 0) {
					printf("\n\t\t\t%.3g dBm, ", MBM_TO_DBM(nla_get_s32(nl_levels)));
				} else {
					printf("%.3g dBm, ", MBM_TO_DBM(nla_get_s32(nl_levels)));
				}
				counter++;
			}
			/* TODO */
			printf("\b \n");
		}

		if (tb_caps[NL802154_CAP_ATTR_CCA_MODES]) {
			struct nlattr *nl_cca_modes;
			int rem_cca_modes;
			printf("\tcca_modes: ");
			nla_for_each_nested(nl_cca_modes,
					    tb_caps[NL802154_CAP_ATTR_CCA_MODES],
					    rem_cca_modes) {
				/* Loop through all CCA options only if it is a
				 * CCA mode that takes CCA options into
				 * consideration.
				 */
				if (tb_caps[NL802154_CAP_ATTR_CCA_OPTS] &&
				    nla_type(nl_cca_modes) == NL802154_CCA_ENERGY_CARRIER) {
					struct nlattr *nl_cca_opts;
					int rem_cca_opts;

					nla_for_each_nested(nl_cca_opts,
								tb_caps[NL802154_CAP_ATTR_CCA_OPTS],
								rem_cca_opts) {
						printf("\n\t\t%s",
							print_cca_mode_handler(
								nla_type(nl_cca_modes),
								nla_type(nl_cca_opts)));
					}
				} else {
					printf("\n\t\t%s",
						print_cca_mode_handler(
							nla_type(nl_cca_modes),
							NL802154_CCA_OPT_ATTR_MAX));

				}
			}
			/* TODO */
			printf("\n");
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
				printf("unknown\n");
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
