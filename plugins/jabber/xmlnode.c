/* $Id$ */

#include <string.h>

#include <ekg/debug.h>
#include <ekg/xmalloc.h>

#include "jabber.h"

static void xmlnode_free(xmlnode_t *n)
{
	xmlnode_t *m;

	if (!n)
		return;

	for (m = n->children; m;) {
		xmlnode_t *cur = m;
		m = m->next;
		xmlnode_free(cur);
	}

	xfree(n->name);
	xfree(n->data);
	array_free(n->atts);
	xfree(n);
}
 
void xmlnode_handle_end(void *data, const char *name)
{
	session_t *s = (session_t *) data;
	xmlnode_t *n;
	jabber_private_t *j;

	if (!s || !(j = s->priv) || !name) {
		debug_error("[jabber] xmlnode_handle_end() invalid parameters\n");
		return;
	}

	if (!(n = j->node)) {
		debug("[jabber] end tag within <stream>, ignoring\n");
		return;
	}

	if (!n->parent) {
		jabber_handle(data, n);
		xmlnode_free(n);
		j->node = NULL;
		return;
	} else {
		j->node = n->parent;
	}
}

void xmlnode_handle_cdata(void *data, const char *text, int len)
{
	session_t *s = (session_t *) data;
	jabber_private_t *j;
	xmlnode_t *n;
	int oldlen;

	if (!s || !(j = s->priv) || !text) {
		debug_error("[jabber] xmlnode_handle_cdata() invalid parameters\n");
		return;
	}

	if (!(n = j->node))
		return;

	oldlen = xstrlen(n->data);
	n->data = xrealloc(n->data, oldlen + len + 1);
	memcpy(n->data + oldlen, text, len);
	n->data[oldlen + len] = 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
