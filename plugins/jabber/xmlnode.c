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
	array_free(n->atts);
}

void xmlnode_handle_start(void *data, const char *name, const char **atts)
{
	jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
	session_t *s = jdh->session;
	xmlnode_t *n, *newnode;
	jabber_private_t *j;
	int i;
	int arrcount;

	if (!s || !(j = session_private_get(s)) || !name) {
		debug("[jabber] xmlnode_handle_end() invalid parameters\n");
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
			newnode->prev = m;
			newnode->parent = n;
		}
	}
	arrcount = array_count((char **) atts);

	newnode->atts = xmalloc((arrcount + 1) * sizeof(char *));

	for (i = 0; i < arrcount; i++)
		newnode->atts[i] = xstrdup(atts[i]);

/*	newnode->atts[i] = NULL; */

	j->node = newnode;
}
 
void xmlnode_handle_end(void *data, const char *name)
{
	jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
	session_t *s = jdh->session;
	xmlnode_t *n;
	jabber_private_t *j;

	if (!s || !(j = session_private_get(s)) || !name) {
		debug("[jabber] xmlnode_handle_end() invalid parameters\n");
		return;
	}

//	debug("[jabber] xmlnode_handle_end(\"%s\")\n", name);

	if (!(n = j->node)) {
		debug("[jabber] end tag within <stream>, ignoring\n");
		return;
	}

	if (!n->parent) {
//		debug("[jabber] finished parsing <%s></%s>\n", name, name);
		jabber_handle(data, n);
		xmlnode_free(n);
		j->node = NULL;
		return;
	} else {
//		debug("[jabber] finished parsing <%s></%s>, going up\n", name, name);
		j->node = n->parent;
	}

//	debug("[jabber] current node is now <%s>\n", j->node->name);
}

void xmlnode_handle_cdata(void *data, const char *text, int len)
{
	jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
	session_t *s = jdh->session;
	jabber_private_t *j;
	xmlnode_t *n;
	int oldlen;

	if (!s || !(j = session_private_get(s)) || !text) {
		debug("[jabber] xmlnode_handle_cdata() invalid parameters\n");
		return;
	}

	if (!(n = j->node)) {
		debug("[jabber] cdata within <stream>, ignoring\n");
		return;
	}

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
