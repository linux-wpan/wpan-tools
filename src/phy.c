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

static int print_edscan_handler(struct nl_msg *msg, void *arg)
{
    uint8_t status;
    uint8_t scan_type;
    uint8_t channel_page;
    uint32_t unscanned_channels;
    uint8_t result_list_size;
    //uint8_t energy_detect_list[ 32 ];
    char *energy_detect_list;
    uint8_t detected_category;

    struct genlmsghdr *gnlh;
    struct nlattr *tb_msg[ NL802154_ATTR_MAX + 1 ];
    int i;

    gnlh = nlmsg_data( nlmsg_hdr( msg ) );

    nla_parse( tb_msg, NL802154_ATTR_MAX, genlmsg_attrdata( gnlh, 0 ),
          genlmsg_attrlen( gnlh, 0 ), NULL );

    if ( tb_msg[ NL802154_ATTR_STATUS ] ) {
        status = nla_get_u8( tb_msg[ NL802154_ATTR_STATUS ] );
    } else {
        goto protocol_error;
    }

    if ( tb_msg[ NL802154_ATTR_SCAN_TYPE ] ) {
        scan_type = nla_get_u8( tb_msg[ NL802154_ATTR_SCAN_TYPE ] );
    } else {
        goto protocol_error;
    }

    if ( tb_msg[ NL802154_ATTR_PAGE ] ) {
        channel_page = nla_get_u8( tb_msg[ NL802154_ATTR_PAGE ] );
    } else {
        goto protocol_error;
    }


    if ( tb_msg[ NL802154_ATTR_CHANNEL_MASK ] ) {
        channel_page = nla_get_u32( tb_msg[ NL802154_ATTR_CHANNEL_MASK ] );
    } else {
        goto protocol_error;
    }

    if ( tb_msg[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] ) {
        channel_page = nla_get_u32( tb_msg[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] );
    } else {
        goto protocol_error;
    }

    if ( tb_msg[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] ) {
        channel_page = nla_get_u32( tb_msg[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] );
    } else {
        goto protocol_error;
    }

    if ( tb_msg[ NL802154_ATTR_ENERGY_DETECT_LIST ] ) {
        energy_detect_list = nla_get_string( tb_msg[ NL802154_ATTR_ENERGY_DETECT_LIST ] );
    } else {
        goto protocol_error;
    }

    printf( "{ " );
    for( i=0; i < 32; i++ ) {
        printf( "%u, ", (uint8_t) energy_detect_list[ i ] );
    }
    printf( "}\n" );

    return 0;

protocol_error:
    return -EINVAL;
}

static int handle_ed_scan(struct nl802154_state *state,
               struct nl_cb *cb,
               struct nl_msg *msg,
               int argc, char **argv,
               enum id_input id)
{
    const uint8_t scan_type = 0; // IEEE802154_MAC_SCAN_ED (FIXME: don't use magic numbers)
    uint32_t scan_channels = -1; // sweep all channels (FIXME: modify mask based on page, e.g. query NL802154_ATTR_CHANNELS_SUPPORTED)
    uint8_t scan_duration = 3; // common scan duration
    uint8_t channel_page = 0; // (FIXME: extract the page from the phy)
    uint8_t security_level = 0;
    uint8_t key_id_mode = 0;
    // uint8_t key_source[4 + 1] = {};
    const char key_source[4 + 1] = {};
    uint8_t key_index = 0;

    NLA_PUT_U8(msg, NL802154_ATTR_SCAN_TYPE, scan_type);
    NLA_PUT_U32(msg, NL802154_ATTR_CHANNEL_MASK, htole32( scan_channels ) );
    NLA_PUT_U8(msg, NL802154_ATTR_DURATION, scan_duration);
    NLA_PUT_U8(msg, NL802154_ATTR_PAGE, channel_page);
    NLA_PUT_U8(msg, NL802154_ATTR_SECURITY_LEVEL, security_level);
    NLA_PUT_U8(msg, NL802154_ATTR_KEY_ID_MODE, key_id_mode);
    NLA_PUT_STRING(msg, NL802154_ATTR_KEY_SOURCE, key_source);
    NLA_PUT_U8(msg, NL802154_ATTR_KEY_INDEX, key_index);

    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_edscan_handler, NULL);

    return 0;

nla_put_failure:
    return -ENOBUFS;
}

COMMAND(get, ed_scan, "<page> <duration>",
    NL802154_CMD_GET_ED_SCAN, 0, CIB_PHY, handle_ed_scan, NULL);
