/* $Id$ */

#ifndef __EKG_RC_RC_H
#define __EKG_RC_RC_H

typedef enum {
	RC_INPUT_PIPE = 1,		/* pipe:/home/user/.ekg/pipe */
	RC_INPUT_UDP,			/* udp:12345 */
	RC_INPUT_TCP,			/* tcp:12345 */
	RC_INPUT_UNIX,			/* unix:/home/user/.ekg/socket */
	RC_INPUT_TCP_CLIENT,
	RC_INPUT_UNIX_CLIENT
} rc_input_type_t;

typedef struct {
	rc_input_type_t type;		/* rodzaj wej¶cia */
	char *path;			/* ¶cie¿ka */
	int fd;				/* deskryptor */
	int mark;			/* do zaznaczania, wnêtrzno¶ci */
} rc_input_t;

int rc_input_new_tcp(const char *path);
int rc_input_new_udp(const char *path);
int rc_input_new_pipe(const char *path);
int rc_input_new_unix(const char *path);

#endif /* __EKG_RC_RC_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
