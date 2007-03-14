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

void xmlnode_handle_start(void *data, const char *name, const char **atts)
{
	session_t *s = (session_t *) data;
	xmlnode_t *n, *newnode;
	jabber_private_t *j;
	int arrcount;
	int i;

	if (!s || !(j = session_private_get(s)) || !name) {
		debug_error("[jabber] xmlnode_handle_end() invalid parameters\n");
		return;
	}

	newnode = xmalloc(sizeof(xmlnode_t));
	newnode->name = xstrdup(name);

	if ((n = j->node)) {
		newnode->parent = n;

		if (!n->children) 
			n->children = newnode;
		else {
			xmlnode_t *m = n->children;

			while (m->next)
				m = m->next;
			
			m->next = newnode;
/*			newnode->prev = m; */
			newnode->parent = n;
		}
	}
	arrcount = array_count((char **) atts);

	if (arrcount > 0) {
		newnode->atts = xmalloc((arrcount + 1) * sizeof(char *));
		for (i = 0; i < arrcount; i++)
			newnode->atts[i] = xstrdup(atts[i]);
	/*	newnode->atts[i] = NULL; */
	} else	newnode->atts = NULL; /* we don't need to allocate table if arrcount = 0 */

	j->node = newnode;
}
 
void xmlnode_handle_end(void *data, const char *name)
{
	session_t *s = (session_t *) data;
	xmlnode_t *n;
	jabber_private_t *j;

	if (!s || !(j = session_private_get(s)) || !name) {
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

	if (!s || !(j = session_private_get(s)) || !text) {
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

xmlnode_t *xmlnode_find_child(xmlnode_t *n, const char *name)
{
	if (!n || !n->children)
		return NULL;

	for (n = n->children; n; n = n->next)
		if (!xstrcmp(n->name, name))
			return n;

	return NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
