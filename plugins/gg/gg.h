/* $Id$ */

#ifndef __EKG_GG_GG_H
#define __EKG_GG_GG_H

#include <libgadu.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>

plugin_t gg_plugin;
int gg_userlist_put_config;
char *last_tokenid;

/* variables */
int gg_config_display_token;
int gg_config_dcc;
char *gg_config_dcc_dir;
char *gg_config_dcc_ip;
char *gg_config_dcc_limit;
int gg_config_dcc_port;
int gg_config_split_messages;

typedef enum {
	GG_QUIET_CHANGE = 0x0001
} gg_quiet_t;

typedef struct {
	struct gg_session *sess;	/* sesja */
	list_t searches;		/* operacje szukania */
	list_t passwds;			/* operacje zmiany has³a */
	gg_quiet_t quiet;		/* co ma byæ cicho */
} gg_private_t;

void gg_register_commands();

void gg_session_handler_msg(session_t *s, struct gg_event *e);
void gg_session_handler(int type, int fd, int watch, void *data);

COMMAND(gg_command_modify);

#endif /* __EKG_GG_GG_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
