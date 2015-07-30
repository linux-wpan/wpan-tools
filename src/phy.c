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

#ifndef IEEE802154_MAX_CHANNEL
#define IEEE802154_MAX_CHANNEL 26
#endif /* IEEE802154_MAX_CHANNEL */
#ifndef IEEE802154_MAX_PAGE
#define IEEE802154_MAX_PAGE 31
#endif /* IEEE802154_MAX_PAGE */

static int parse_nla_array_u8( struct nlattr *a, const int type, uint8_t *value, size_t *len ) {
    int r;

    const size_t maxlen = *len;
    struct nlattr *e;
    size_t _len = 0;
    int rem;

    if ( NULL == a || NULL == value || NULL == len || type <= 0 || type > NL802154_ATTR_MAX ) {
        r = -EINVAL;
        goto out;
    }

    nla_for_each_nested(e, a, rem) {
        if ( _len >= maxlen ) {
            break;
        }
        if ( type != nla_type( e ) ) {
            break;
        }
        value[ _len ] = nla_get_u8( e );
        _len++;
    }

    *len = _len;
    r = 0;
out:
    return r;
}

static int print_ed_scan_handler(struct nl_msg *msg, void *arg)
{
    int r;

    size_t len;
    uint8_t status;
    uint8_t scan_type;
    uint8_t channel_page;
    uint32_t scan_channels = *((uint32_t *)arg);
    uint32_t unscanned_channels;
    uint8_t result_list_size;
    uint8_t ed[ IEEE802154_MAX_CHANNEL + 1 ];
    uint8_t detected_category;

    struct genlmsghdr *gnlh;
    struct nlattr *tb[ NL802154_ATTR_MAX + 1 ];
    int i,j;

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
        tb[ NL802154_ATTR_SUPPORTED_CHANNEL ] &&
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
    unscanned_channels = nla_get_u32( tb[ NL802154_ATTR_SUPPORTED_CHANNEL ] );
    result_list_size = nla_get_u32( tb[ NL802154_ATTR_SCAN_RESULT_LIST_SIZE ] );
    len = sizeof( ed ) / sizeof( ed[ 0 ] );
    r = parse_nla_array_u8( tb[ NL802154_ATTR_SCAN_ENERGY_DETECT_LIST ], NL802154_ATTR_SCAN_ENERGY_DETECT_LIST_ENTRY, ed, &len );
    if ( 0 != r ) {
        goto protocol_error;
    }
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
    for( i=0, j=0; i < sizeof( ed ) / sizeof( ed[ 0 ] ) && j <= result_list_size; i++ ) {
        if ( scan_channels & ( 1 << i ) ) {
            printf( "%u:%u, ", i, ed[ j ]  );
            j++;
        }
    }
    printf( "}, detected_category: %u\n", detected_category );

    r = 0;
    goto out;

protocol_error:
    fprintf( stderr, "protocol error\n" );
    r = -EINVAL;

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

    int i;

    const uint8_t scan_type = 0; // XXX: define IEEE802154_MAC_SCAN_ED (FIXME: don't use magic numbers)
    uint32_t channel_page = 0;
    static uint32_t scan_channels;
    uint32_t scan_duration = 3;

    if ( argc >= 1 ) {
        if ( 1 != sscanf( argv[ 0 ], "%u", &channel_page ) ) {
            goto invalid_arg;
        }
    }
    // specify a sane default of scan_channels
    // if channel_page was specified or not
    switch( channel_page ) {
    case 0:
        scan_channels = 0x7fff800;
        /* no break */
    default:
        break;
    }
    if ( argc >= 2 ) {
        if ( ! (
            ( 0 == strncmp( "0x", argv[ 1], 2 ) && 1 == sscanf( argv[ 1 ] + 2, "%x", &scan_channels ) ) ||
            1 == sscanf( argv[ 1 ], "%u", &scan_channels )
        ) ) {
            goto invalid_arg;
        }
    }
    if ( argc == 3 ) {
        if ( 1 != sscanf( argv[ 2 ], "%u", &scan_duration ) ) {
            goto invalid_arg;
        }
    }
    if ( argc > 3 ) {
        goto invalid_arg;
    }

    NLA_PUT_U8(msg, NL802154_ATTR_SCAN_TYPE, scan_type);
    NLA_PUT_U32(msg, NL802154_ATTR_SUPPORTED_CHANNEL, scan_channels );
    NLA_PUT_U8(msg, NL802154_ATTR_SCAN_DURATION, scan_duration);
    NLA_PUT_U8(msg, NL802154_ATTR_PAGE, channel_page);

    r = nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_ed_scan_handler, &scan_channels );
    if ( 0 != r ) {
        goto out;
    }

    r = 0;
    goto out;

nla_put_failure:
    r = -ENOBUFS;
out:
    return r;
invalid_arg:
    r = 1;
    goto out;
}

COMMAND(get, ed_scan, "[<page> [<channels> [<duration>]]]",
    NL802154_CMD_ED_SCAN_REQ, 0, CIB_PHY, handle_ed_scan, NULL);
