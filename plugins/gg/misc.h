/* $Id$ */

#ifndef __EKG_GG_MISC_H
#define __EKG_GG_MISC_H

#include <ekg/sessions.h>
#include <ekg/userlist.h>

const char *gg_status_to_text(int status);
int gg_text_to_status(const char *text, const char *descr);
void gg_iso_to_cp(unsigned char *buf);
void gg_cp_to_iso(unsigned char *buf);
char gg_userlist_type(userlist_t *u);
int gg_blocked_add(session_t *s, const char *uid);
int gg_blocked_remove(session_t *s, const char *uid);
const char *gg_http_error_string(int h);
int gg_userlist_send(struct gg_session *s, list_t userlist);
	
#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
