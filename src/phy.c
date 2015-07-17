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

static int print_ed_scan_handler(struct nl_msg *msg, void *arg)
{
    int r;

    uint8_t status;
    uint8_t scan_type;
    uint8_t channel_page;
    uint32_t unscanned_channels;
    uint8_t result_list_size;
    //uint8_t energy_detect_list[ 32 ];
    char *energy_detect_list;
    uint8_t detected_category;

    struct genlmsghdr *gnlh;
    struct nlattr *tb[ NL802154_ATTR_MAX + 1 ];
    int i;

    printf( "in %s\n", __FUNCTION__ );

    gnlh = nlmsg_data( nlmsg_hdr( msg ) );
    if ( NULL ==  gnlh ) {
        fprintf( stderr, "gnlh was null\n" );
        goto protocol_error;
    }

    r = nla_parse( tb, NL802154_ATTR_MAX, genlmsg_attrdata( gnlh, 0 ),
          genlmsg_attrlen( gnlh, 0 ), NULL );
    if ( 0 != r ) {
        fprintf( stderr, "nla_parse\n" );
        goto protocol_error;
    }

    if ( ! (
        tb[ NL802154_ATTR_SCAN_STATUS ] &&
        tb[ NL802154_ATTR_SCAN_TYPE ] &&
        tb[ NL802154_ATTR_PAGE ] &&
        tb[ NL802154_ATTR_CHANNEL_MASK ] &&
        tb[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] &&
        tb[ NL802154_ATTR_SCAN_ENERGY_DETECT_LIST ] &&
        tb[ NL802154_ATTR_SCAN_DETECTED_CATEGORY ]
    ) ) {
        r = -EINVAL;
        goto out;
    }

    status = nla_get_u8( tb[ NL802154_ATTR_SCAN_STATUS ] );
    scan_type = nla_get_u8( tb[ NL802154_ATTR_SCAN_TYPE ] );
    channel_page = nla_get_u8( tb[ NL802154_ATTR_PAGE ] );
    unscanned_channels = nla_get_u32( tb[ NL802154_ATTR_CHANNEL_MASK ] );
    result_list_size = nla_get_u32( tb[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] );
    energy_detect_list = nla_get_string( tb[ NL802154_ATTR_SCAN_ENERGY_DETECT_LIST ] );
    detected_category = nla_get_u8( tb[ NL802154_ATTR_SCAN_DETECTED_CATEGORY ] );

    printf(
        "status: %u, "
        "scan_type: %u, "
        "channel_page: %u, "
        "unscanned_channels: %08x, "
        "result_list_size: %u, "
        "energy_detect_list: ",
        status,
        scan_type,
        channel_page,
        unscanned_channels,
        result_list_size
    );

    printf( "{ " );
    for( i=0; i < 32; i++ ) {
        printf( "%02x,", (uint8_t) energy_detect_list[ i ] );
    }
    printf( "}, detected_category: %u\n", detected_category );

    r = 0;
    goto out;

protocol_error:
    fprintf( stderr, "protocol error\n" );
    r = -EINVAL;

out:
    printf( "returning %d\n", r );
    return r;
}

static int put_null_security_bits(struct nl_msg *msg)
{
    int r;
    struct null_security_bits {
        uint8_t level;
        uint8_t key_id_mode;
        // key_source is not present
        uint8_t key_index;
    };
    static const struct null_security_bits bits = {
        .level = 0,
        .key_id_mode = 0,
        .key_index = 0,
    };
    struct nlattr *sec_attr;
    sec_attr = nla_nest_start(msg,NL802154_ATTR_SEC);
    if ( NULL == sec_attr ) {
        r = -ENOBUFS;
        goto out;
    }
    NLA_PUT_U8(msg, NL802154_ATTR_SEC_LEVEL, bits.level);
    NLA_PUT_U8(msg, NL802154_ATTR_SEC_KEY_ID_MODE, bits.key_id_mode);
    NLA_PUT_U8(msg, NL802154_ATTR_SEC_KEY_INDEX, bits.key_index);
    nla_nest_end(msg, sec_attr);
    r = 0;
    goto end_nest;

nla_put_failure:
    r = -ENOBUFS;
end_nest:
    nla_nest_end(msg, sec_attr);
out:
    return r;
}

static int handle_ed_scan(struct nl802154_state *state,
               struct nl_cb *cb,
               struct nl_msg *msg,
               int argc, char **argv,
               enum id_input id)
{
    int r;

    const uint8_t scan_type = 0; // IEEE802154_MAC_SCAN_ED (FIXME: don't use magic numbers)
    uint32_t scan_channels = -1; // sweep all channels (FIXME: modify mask based on page, e.g. query NL802154_ATTR_CHANNELS_SUPPORTED)
    uint8_t scan_duration = 3; // common scan duration
    uint8_t channel_page = 0; // (FIXME: extract the page from the phy)

    struct nlattr *ed_req_attr;

    ed_req_attr = nla_nest_start(msg, NL802154_ATTR_SCAN_REQ);
    if ( NULL == ed_req_attr ) {
        r = -ENOBUFS;
        goto out;
    }
    NLA_PUT_U8(msg, NL802154_ATTR_SCAN_TYPE, scan_type);
    NLA_PUT_U32(msg, NL802154_ATTR_CHANNEL_MASK, htole32( scan_channels ) );
    NLA_PUT_U8(msg, NL802154_ATTR_SCAN_DURATION, scan_duration);
    NLA_PUT_U8(msg, NL802154_ATTR_PAGE, channel_page);
    r = put_null_security_bits( msg );
    if ( 0 != r ) {
        goto end_nest;
    }

    r = nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_ed_scan_handler, NULL);
    if ( 0 != r ) {
        goto end_nest;
    }

    r = 0;
    goto end_nest;

nla_put_failure:
    r = -ENOBUFS;
end_nest:
    nla_nest_end(msg, ed_req_attr);
out:
    return r;
}

COMMAND(get, ed_scan, "<page> <duration>",
    NL802154_CMD_ED_SCAN_REQ, 0, CIB_PHY, handle_ed_scan, NULL);
