#ifndef __EKG_GG_GG_H
#define __EKG_GG_GG_H

#include <libgadu.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>

plugin_t gg_plugin;
int gg_userlist_put_config;
char *last_tokenid;

int config_display_token;

typedef enum {
	GG_QUIET_CHANGE = 0x0001
} gg_quiet_t;

typedef struct {
	struct gg_session *sess;	/* sesja */
	list_t searches;		/* operacje szukania */
	list_t passwds;			/* operacje zmiany has³a */
	gg_quiet_t quiet;		/* co ma byæ cicho */
} gg_private_t;

#endif /* __EKG_GG_GG_H */
