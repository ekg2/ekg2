#include "ekg2-config.h"
#include <ekg/plugins.h>
#include <ekg/sessions.h>

#define RSS_ONLY	 SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define RSS_FLAGS	 RSS_ONLY  | SESSION_MUSTBECONNECTED
#define RSS_FLAGS_TARGET RSS_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

#define feed_private(s) ((s && s->priv) ? ((feed_private_t *) s->priv)->priv_data : NULL)

extern plugin_t feed_plugin;

typedef struct {
#ifdef HAVE_EXPAT
	int isrss;
#endif
	void *priv_data;
} feed_private_t;

extern void *nntp_protocol_init();		/* nntp.c */
extern void nntp_protocol_deinit(void *);	/* nntp.c */
extern void nntp_init();			/* nntp.c */

#ifdef HAVE_EXPAT
extern void *rss_protocol_init();		/* rss.c */
extern void rss_protocol_deinit(void *);	/* rss.c */
extern void rss_init();				/* rss.c */
extern void rss_deinit();			/* rss.c */
#endif

/* some sad helpers */
void feed_set_statusdescr(userlist_t *u, int status, char *descr);
void feed_set_descr(userlist_t *u, char *descr);
void feed_set_status(userlist_t *u, int status);

