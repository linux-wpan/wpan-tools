#ifndef __IWPAN_H
#define __IWPAN_H

#include <stdbool.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <endian.h>

#include "nl802154.h"

/* TODO libnl1 compatibility */
//#define nl_sock nl_handle

struct nl802154_state {
	struct nl_sock *nl_sock;
	int nl802154_id;
};

enum command_identify_by {
	CIB_NONE,
	CIB_PHY,
	CIB_NETDEV,
	CIB_WPAN_DEV,
};

enum id_input {
	II_NONE,
	II_NETDEV,
	II_PHY_NAME,
	II_PHY_IDX,
	II_WPAN_DEV,
};

struct cmd {
	const char *name;
	const char *args;
	const char *help;
	const enum nl802154_commands cmd;
	int nl_msg_flags;
	int hidden;
	const enum command_identify_by idby;
	/* The handler should return a negative error code,
	 * zero on success, 1 if the arguments were wrong
	 * and the usage message should and 2 otherwise.
	 */
	int (*handler)(struct nl802154_state *state,
		       struct nl_cb *cb,
		       struct nl_msg *msg,
		       int argc, char **argv,
		       enum id_input id);
	const struct cmd *(*selector)(int argc, char **argv);
	const struct cmd *parent;
};

#define __COMMAND(_section, _symname, _name, _args, _nlcmd, _flags, _hidden, _idby, _handler, _help, _sel)\
	static struct cmd						\
	__cmd ## _ ## _symname ## _ ## _handler ## _ ## _nlcmd ## _ ## _idby ## _ ## _hidden\
	__attribute__((used)) __attribute__((section("__cmd")))	= {	\
		.name = (_name),					\
		.args = (_args),					\
		.cmd = (_nlcmd),					\
		.nl_msg_flags = (_flags),				\
		.hidden = (_hidden),					\
		.idby = (_idby),					\
		.handler = (_handler),					\
		.help = (_help),					\
		.parent = _section,					\
		.selector = (_sel),					\
	}
#define __ACMD(_section, _symname, _name, _args, _nlcmd, _flags, _hidden, _idby, _handler, _help, _sel, _alias)\
	__COMMAND(_section, _symname, _name, _args, _nlcmd, _flags, _hidden, _idby, _handler, _help, _sel);\
	static const struct cmd *_alias = &__cmd ## _ ## _symname ## _ ## _handler ## _ ## _nlcmd ## _ ## _idby ## _ ## _hidden
#define COMMAND(section, name, args, cmd, flags, idby, handler, help)	\
	__COMMAND(&(__section ## _ ## section), name, #name, args, cmd, flags, 0, idby, handler, help, NULL)
#define COMMAND_ALIAS(section, name, args, cmd, flags, idby, handler, help, selector, alias)\
	__ACMD(&(__section ## _ ## section), name, #name, args, cmd, flags, 0, idby, handler, help, selector, alias)
#define HIDDEN(section, name, args, cmd, flags, idby, handler)		\
	__COMMAND(&(__section ## _ ## section), name, #name, args, cmd, flags, 1, idby, handler, NULL, NULL)
#define TOPLEVEL(_name, _args, _nlcmd, _flags, _idby, _handler, _help)	\
	struct cmd							\
	__section ## _ ## _name						\
	__attribute__((used)) __attribute__((section("__cmd")))	= {	\
		.name = (#_name),					\
		.args = (_args),					\
		.cmd = (_nlcmd),					\
		.nl_msg_flags = (_flags),				\
		.idby = (_idby),					\
		.handler = (_handler),					\
		.help = (_help),					\
	 }
#define SECTION(_name)							\
	struct cmd __section ## _ ## _name				\
	__attribute__((used)) __attribute__((section("__cmd"))) = {	\
		.name = (#_name),					\
		.hidden = 1,						\
	}

#define SECTION(_name)							\
	struct cmd __section ## _ ## _name				\
	__attribute__((used)) __attribute__((section("__cmd"))) = {	\
		.name = (#_name),					\
		.hidden = 1,						\
	}

#define DECLARE_SECTION(_name)						\
	extern struct cmd __section ## _ ## _name;

#define DBM_TO_MBM(gain)						\
	((int)(((float)gain) * 100))
#define MBM_TO_DBM(gain)						\
	((float)(gain) / 100)

int handle_cmd(struct nl802154_state *state, enum id_input idby,
	       int argc, char **argv);

DECLARE_SECTION(set);
DECLARE_SECTION(get);

const char *iftype_name(enum nl802154_iftype iftype);

extern int iwpan_debug;

#endif /* __IWPAN_H */
