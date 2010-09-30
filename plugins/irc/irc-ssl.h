#ifndef __EKG_IRC_SSL_H
#define __EKG_IRC_SSL_H

#include <ekg2-config.h>

#ifdef IRC_HAVE_OPENSSL
# define IRC_HAVE_SSL 1
//# warning "support for ssl (using openssl) in irc plugin is in beta version, be prepared for unpredictable"
#endif

#ifdef IRC_HAVE_SSL

# include <openssl/ssl.h>
# include <openssl/err.h>

extern SSL_CTX *ircSslCtx; /* irc.c */

# define SSL_SESSION		SSL *

# define SSL_INIT(session)		!(session = SSL_new(ircSslCtx))

# define SSL_HELLO(session)		SSL_connect(session)
# define SSL_BYE(session)		SSL_shutdown(session)
# define SSL_DEINIT(session)		SSL_free(session)
# define SSL_GLOBAL_INIT()		SSL_library_init(); ircSslCtx = SSL_CTX_new(SSLv23_client_method())
# define SSL_GLOBAL_DEINIT()		SSL_CTX_free(ircSslCtx)
# define SSL_ERROR(retcode)		ERR_error_string(retcode, NULL)		/* retcode need be value from SSL_get_error(session, res) */
# define SSL_E_AGAIN(ret)		((ret == SSL_ERROR_WANT_READ || ret == SSL_ERROR_WANT_WRITE))

# define SSL_SEND(session, str, len)	SSL_write(session, str, len)
# define SSL_RECV(session, buf, size)	SSL_read(session, buf, size)

# define SSL_SET_FD(session, fd)	SSL_set_fd(session, fd)
# define SSL_GET_FD(session, fd)		fd
# define SSL_WRITE_DIRECTION(session, ret)	(ret != SSL_ERROR_WANT_READ)

#endif		/* JABBER_HAVE_SSL */

#endif		/* __EKG_JABBER_SSL_H */

