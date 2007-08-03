
/* External message plugin for ekg2
 * (C) 2006 Michał Górny <peres@peres.int.pl>
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/queries.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#ifdef HAVE_INOTIFY
#include <termios.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#ifndef IN_ONLYDIR
/* stolen from sys/inotify.h */
#define IN_ONLYDIR       0x01000000
#endif
#endif /*HAVE_INOTIFY*/

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#ifndef NAME_MAX
#ifdef MAXNAMLEN /* BSD */
#define NAME_MAX MAXNAMLEN
#else /* fallback */
#define NAME_MAX 255
#endif
#endif /* NAME_MAX */

/* __func__ fallback (from 'info gcc') */
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ itoa(__LINE__)
# endif
#endif

/* some additional defines */
#define XMSG_UID_DIROFFSET 5
#define XMSG_NAMESEP_DEF "."
#define XMSG_MAXFS_DEF "16384"
#define XMSG_MAXFC_DEF "25"
#define XMSG_MAXFC_TIMER "3"
#define XMSG_TMPFILE_PATH "/tmp/xmsg.XXXXXX"

/* if we have inotify, we don't need that timer */
#ifdef HAVE_INOTIFY
#define XMSG_MAXFC_INOTIFY 25
#define XMSG_TIMER_DEF "0"
#else
#define XMSG_TIMER_DEF "300"
#endif

/* debugs */
#define xerr(txt, ...) do { debug_error("[xmsg:%s] " txt "\n", __func__, ##__VA_ARGS__); return -1; } while (0)
#define xerrn(txt, ...) do { debug_error("[xmsg:%s] " txt ": %s\n", __func__, ##__VA_ARGS__, strerror(errno)); return -1; } while (0)
#define xdebug(txt, ...) debug("[xmsg:%s] " txt "\n", __func__, ##__VA_ARGS__)
#define xdebug2(lvl, txt, ...) debug_ext(lvl, "[xmsg:%s] " txt "\n", __func__, ##__VA_ARGS__)

/* global vars */
static int in_fd = 0;
static struct inotify_event *ev = NULL;
#ifdef HAVE_INOTIFY
static int config_maxinotifycount = XMSG_MAXFC_INOTIFY;
#endif

/* constructor */
static int xmsg_theme_init(void); /* the only needed prototype */
PLUGIN_DEFINE(xmsg, PLUGIN_PROTOCOL, xmsg_theme_init);

/* the code */

/* like that in mine 'jogger' plugin, but slightly modified for xmsg
 * - quiet always 1, so removed all prints,
 * - hash not needed, giving timestamp instead.
 */
static int xmsg_checkoutfile(const char *file, char **data, int *len, time_t *ts, const int maxlen) {
	int fs, fd;

	const char *fn	= prepare_path_user(file);

	if (!fn)
		return EINVAL;

	if ((fd = open(fn, O_RDONLY|O_NONBLOCK)) == -1) /* we use O_NONBLOCK to get rid of FIFO problems */
		return errno;

	{
		struct stat st;

		if ((fstat(fd, &st) == -1) || !S_ISREG(st.st_mode)) {
			close(fd);
			return EISDIR; /* nearest, I think */
		}

		fs = st.st_size;
		/* mtime > ctime > atime > time(NULL) */
#define X(x,y) (x ? x : y)
		if (ts)
			*ts = X(st.st_mtime, X(st.st_ctime, X(st.st_atime, time(NULL))));
#undef X
	}

	int bufsize	= (fs ? (maxlen && fs > maxlen ? maxlen+1 : fs+1) : 0x4000); /* we leave 1 byte for additional NUL */
	char *out	= xmalloc(bufsize);
	void *p		= out;
	int _read = 0, res;

	{
		int cf	= fcntl(fd, F_GETFL);

		if (cf == -1) /* evil thing */
			cf = 0;
		else
			cf &= ~O_NONBLOCK;
		fcntl(fd, F_SETFL, cf);
	}

	while ((res = read(fd, p, bufsize-_read))) {
		if (res == -1) {
			const int err = errno;
			if (err != EINTR && err != EAGAIN) {
				close(fd);
				return err;
			}
		} else {
			_read += res;
			if (maxlen && _read > maxlen) {
				xfree(out);
				return EFBIG;
			} else if (_read == bufsize) { /* fs sucks? */
				bufsize += 0x4000;
				out	= xrealloc(out, bufsize);
				p	= out+_read;
			} else
				p	+= res;
		}
	}
	close(fd);

	if (_read == 0) {
		xfree(out);
		return EINVAL; /* like mmap() */
	} else if (_read+1 != bufsize) {
		out		= xrealloc(out, _read+1);
		out[_read]	= 0; /* add NUL */
	}

	if (len)
		*len = _read;

	if (data)
		*data = out;
	else
		xfree(out);

	return 0;
}


static const char *xmsg_dirfix(const char *path)
{
	char *buf = (char*) prepare_pathf(NULL); /* steal the buffer */
	
	if (strlcpy(buf, path, PATH_MAX) >= PATH_MAX) { /* buffer too small */
		xdebug2(DEBUG_ERROR, "Buffer too small for: in = %s, len = %d, PATH_MAX = %d", path, xstrlen(path), PATH_MAX);
		return NULL;
	}

	/* if path starts with slash, we leave it as is,
	 * otherwise we convert # to / */
	if (*buf != '/') {
		char *p;

		for (p = xstrchr(buf, '#'); p; p = xstrchr(p+1, '#'))
			*p = '/';
	}

	xdebug("in: %s, out: %s", path, buf);

	return buf;
}

static int xmsg_handle_file(session_t *s, const char *fn)
{
	const int nounlink = !session_int_get(s, "unlink_sent");
	const int utb = session_int_get(s, "unlink_toobig");
	const int maxfs = session_int_get(s, "max_filesize");
	const char *dfsuffix = session_get(s, "dotfile_suffix");
	char *namesep = (char*) session_get(s, "name_separator");
	char *dir;
	int dirlen;

	char *msg = NULL;
	int err, fs;
	time_t ft;
	
	if (*fn == '.') /* we're skipping ALL dotfiles */
		return -1;
	dir = (char*) xmsg_dirfix(session_uid_get(s)+XMSG_UID_DIROFFSET);
	dirlen = xstrlen(dir);
		/* first check if buffer is long enough to fit the whole path for dotfile */
	if (strlcpy(dir+dirlen+1, fn, PATH_MAX-dirlen-2-xstrlen(dfsuffix)) >= PATH_MAX-dirlen-2-xstrlen(dfsuffix))
		xerr("Buffer too small for: fn = %s, len(fn) = %d, dirlen = %d, dfsuffixlen = %d", fn, xstrlen(fn), dirlen, xstrlen(dfsuffix));

		/* then fill in middle part of path */
	dir[dirlen] = '/';
		/* and take a much closer look the file */	
	xdebug("s = %s, d = %s, fn = %s", session_uid_get(s), dir, fn);
	if ((err = xmsg_checkoutfile(dir, &msg, &fs, &ft, maxfs))) {
		if (err == EFBIG) {
			print((utb ? "xmsg_toobigrm" : "xmsg_toobig"), fn, session_name(s));
			if (utb) {
				unlink(dir);
				return -1;
			} /* else we need to create the dotfile first */
		} else if (err != ENOENT && err != EINVAL)
			return -1;
	} else if (!nounlink && (utb == (err == EFBIG)))
		unlink(dir);

		/* here: dir = dotf */
	memmove(dir+dirlen+2, dir+dirlen+1, xstrlen(dir) - dirlen);
	dir[dirlen+1] = '.';
	xstrcpy(dir+xstrlen(dir), dfsuffix); /* we've already checked whether it fits */
	
	{
		struct stat st;
		int r;
		
		if (nounlink || !utb) {
			r = !(stat(dir, &st) || S_ISDIR(st.st_mode));
		} else
			r = 0;
		
		if (err == ENOENT) {
			if (r) /* clean up stale dotfile */
				unlink(dir);
			xfree(msg);
			return -1;
		} else if (r) {
			xfree(msg); /* XXX: I think that we rather shouldn't first read, then check if it is needed,
					at least for nounlink mode */
			return -1;
		} else if ((nounlink && !(utb && err == EFBIG)) || (!utb && err == EFBIG))
			close(open(dir, O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW, 0600));
	}
	
	if (err == EFBIG)
		return -1;
	else if (err == EINVAL)
		xdebug("empty file, not submitting");
	else {
		{
			char *session	= xstrdup(session_uid_get(s));
			char *uid	= xmalloc(strlen(fn) + 6);
			char **rcpts    = NULL;
			uint32_t *format= NULL;
			time_t sent	= ft;
			int class	= EKG_MSGCLASS_CHAT;
			char *seq	= NULL;
			int dobeep	= EKG_TRY_BEEP;
			int secure	= 0;
			char *msgx	= NULL;

			{
				const char *charset = session_get(s, "charset");

				if (charset && (msgx = ekg_convert_string(msg, charset, NULL)))
					xfree(msg);
				else
					msgx = msg;
			}

			xstrcpy(uid, "xmsg:");
			xstrcat(uid, fn);
			if (namesep) {
				char *p, *q = NULL;

				for (p = namesep; *p; p++) {
					char *r = xstrrchr(uid+XMSG_UID_DIROFFSET, *p);
					if (r > q)
						q = r;
				}
				if (q)
					*q = '\0';
			}

			query_emit_id(NULL, PROTOCOL_MESSAGE, &session, &uid, &rcpts, &msgx, &format, &sent, &class, &seq, &dobeep, &secure);

			xfree(msgx);
			xfree(uid);
			xfree(session);
		}
	}
	
	return 0;
}

static TIMER_SESSION(xmsg_iterate_dir)
{
	const char *dir;
	DIR *d;
	struct dirent *de;
	int n = 0;
	const int maxn = session_int_get(s, "max_oneshot_files");

	if (type || !s || !session_connected_get(s))
		return -1;
	
	session_status_set(s, EKG_STATUS_AVAIL);
	if (!(dir = xmsg_dirfix(session_uid_get(s)+XMSG_UID_DIROFFSET))
			|| !(d = opendir(dir))) {

		xerr("unable to open specified directory");
		return 0;
	}
	
	while ((de = readdir(d))) {
		if (!xmsg_handle_file(s, de->d_name))
			n++;
		
		if ((maxn > 0) && n >= maxn) {
			const int i = session_int_get(s, "oneshot_resume_timer");
			if ((i > 0) && timer_add_session(s, "o", i, 0, xmsg_iterate_dir))
				xdebug("oneshot resume timer added");
			session_status_set(s, EKG_STATUS_AWAY);
			break;
		}
	}
	closedir(d);
	xdebug("processed %d files", n);

	return 0;
}

static void xmsg_timer_change(session_t *s, const char *varname)
{
	int n = (varname ? session_int_get(s, varname) : 0);
	
	xdebug("n = %d", n);
	if (!varname || session_connected_get(s)) {
		if (!timer_remove_session(s, "w"))
			xdebug("old timer removed");
		if (n > 0) {
			if (timer_add_session(s, "w", n, 1, xmsg_iterate_dir))
				xdebug("new timer added");
		}
	}
}

/* we return 0 even if rmwatch fails, because xmsg_handle_data checks
 * if our session is still connected, so it'll ignore unneeded events */
static COMMAND(xmsg_disconnect)
{
	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}
	
	xmsg_timer_change(session, NULL);
	if (!timer_remove_session(session, "o"))
		xdebug("old oneshot resume timer removed");
	session_status_set(session, EKG_STATUS_NA);
	{
		char *sess = xstrdup(session_uid_get(session));
		char *reason = (quiet == -1 ? xstrdup(format_find("xmsg_umount")) : NULL);
		int type = (quiet == -1 ? EKG_DISCONNECT_NETWORK : EKG_DISCONNECT_USER);
		
		query_emit_id(NULL, PROTOCOL_DISCONNECTED, &sess, &reason, &type, NULL);
		
		xfree(reason);
		xfree(sess);
	}

#ifdef HAVE_INOTIFY
	if (session->priv && inotify_rm_watch(in_fd, (uint32_t) session->priv))
		xdebug2(DEBUG_ERROR, "rmwatch failed");
	else
		xdebug("inotify watch removed: %d", (uint32_t) session->priv);
#endif /*HAVE_INOTIFY*/

	return 0;
}

#ifdef HAVE_INOTIFY
static WATCHER(xmsg_handle_data)
{
	int n;
	int c = 0;
	struct inotify_event *evp;

	if (type)
		return -1;

	ioctl(fd, FIONREAD, &n);
	if (n == 0)
		return 0;

	ev = xrealloc(ev, n);
	n = read(fd, ev, n);

	if (n < 0)
		xerrn("inotify read() failed");
	
	for (evp = ev; n > 0; n -= (evp->len + sizeof(struct inotify_event)), evp = (void*) evp + (evp->len + sizeof(struct inotify_event))) {
		list_t sp;
		session_t *s = NULL;

		for (sp = sessions; sp; sp = sp->next) {
			s = sp->data;

			if (s && (s->priv == (void*) evp->wd) && !xstrncasecmp(session_uid_get(s), "xmsg:", 5))
				break;
		}
		
		xdebug("n = %d, wd = %d, str = %s", n, evp->wd, evp->name);
			
		if ((evp->mask & IN_IGNORED) || !s || !session_connected_get(s))
			continue;
		else if (evp->mask & IN_UNMOUNT)
			xmsg_disconnect(NULL, NULL, s, NULL, -1);
		else if (!(evp->mask & IN_Q_OVERFLOW) && (c != -1) && (!xmsg_handle_file(s, evp->name))) 
			c++;
		
		if ((evp->mask & IN_Q_OVERFLOW) || ((config_maxinotifycount > 0) && c >= config_maxinotifycount)) {
			for (sp = sessions; sp; sp = sp->next) {
				s = sp->data;

				if (s && !xstrncasecmp(session_uid_get(s), "xmsg:", 5)) {
					const int i = session_int_get(s, "oneshot_resume_timer");
					if (!timer_remove_session(s, "o"))
						xdebug("old oneshot resume timer removed");
					if ((i > 0) && timer_add_session(s, "o", i, 0, xmsg_iterate_dir)) {
						xdebug("oneshot resume timer added");
						session_status_set(s, EKG_STATUS_AWAY);
					} else
						session_status_set(s, EKG_STATUS_AVAIL);
					c = -1;
				}
			}
		}
	}
	if (c >= 0)
		xdebug("processed %d files", c);
	else
		xdebug("reached max_inotifycount");

	return 0;
}
#endif /*HAVE_INOTIFY*/

static QUERY(xmsg_handle_sigusr)
{
	list_t sp;
	session_t *s;

	for (sp = sessions; sp; sp = sp->next) {
		s = sp->data;

		if (!timer_remove_session(s, "o"))
			xdebug("old oneshot resume timer removed");
		if (s && !xstrncasecmp(session_uid_get(s), "xmsg:", 5))
			xmsg_iterate_dir(0, (void*) s);
	}

	return 0;
}

static QUERY(xmsg_validate_uid)
{
	char *uid = *(va_arg(ap, char**));
	int *valid = va_arg(ap, int*);
	
	if (uid && !xstrncasecmp(uid, "xmsg:", XMSG_UID_DIROFFSET)) {
		(*valid)++;
		return -1;
	}

	return 0;
}

static inline int xmsg_add_watch(session_t *s, const char *f)
{
	struct stat fs;
	const char *dir = xmsg_dirfix(f);

	if (!dir)
		return 0;
	else if (!stat(dir, &fs)) {
		if (!S_ISDIR(fs.st_mode))
			xerr("given path is a file, not a directory");
	} else {
		if (mkdir(dir, 0777))
			xerrn("mkdir failed");
	}

#ifdef HAVE_INOTIFY
	if ((s->priv = (void*) inotify_add_watch(in_fd, dir, (IN_CLOSE_WRITE|IN_MOVED_TO|IN_ONLYDIR))) == (void*) -1)
		xerrn("unable to add inotify watch");
	
	xdebug("inotify watch added: %d", (uint32_t) s->priv);
#endif /*HAVE_INOTIFY*/
	
	return 0;
}

static COMMAND(xmsg_connect)
{
	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}
	if (command_exec(NULL, session, "/session --lock", 0) == -1)
		return -1;

	if (xmsg_add_watch(session, session_uid_get(session)+XMSG_UID_DIROFFSET)) {
		print("conn_failed", format_find("xmsg_addwatch_failed"), session_name(session));
		return -1;
	}
	
	session_status_set(session, EKG_STATUS_AVAIL);
	{
		char *sess = xstrdup(session_uid_get(session));

		query_emit_id(NULL, PROTOCOL_CONNECTED, &sess);

		xfree(sess);
	}

	xmsg_iterate_dir(0, (void*) session);
	xmsg_timer_change(session, "rescan_timer");

	return 0;
}

static COMMAND(xmsg_reconnect)
{
	if (session_connected_get(session)) {
		xmsg_disconnect(name, params, session, target, quiet);
	}

	return xmsg_connect(name, params, session, target, quiet);
}

/* kind = 0 for sent, 1 for toobig */
static void xmsg_unlink_dotfiles(session_t *s, const char *varname)
{
	if (session_int_get(s, varname)) {
		const int kind = !xstrcasecmp(varname, "unlink_sent");
		const int maxfs = session_int_get(s, "max_filesize");
		const char *dfsuffix = session_get(s, "dotfile_suffix");
		const char *dir = xmsg_dirfix(session_uid_get(s)+XMSG_UID_DIROFFSET);
		DIR *d;
		struct dirent *de;
		struct stat st, std;
		char *df, *dfd, *dp, *dpd;
		
		if (!dir || !(d = opendir(dir))) {
			xdebug("unable to open specified directory");
			return;
		}
		
		df = xmalloc(xstrlen(dir) + NAME_MAX + 2);
		dfd = xmalloc(xstrlen(dir) + NAME_MAX + 3 + xstrlen(dfsuffix));
		xstrcpy(df, dir);
		dp = df + xstrlen(df);
		*(dp++) = '/';
		xstrcpy(dfd, df);
		dpd = dfd + xstrlen(dfd);
		*(dpd++) = '.';
		
		while ((de = readdir(d))) {
			if (de->d_name[0] == '.')
				continue;
			if (xstrlen(de->d_name) > NAME_MAX) {
				xdebug2(DEBUG_ERROR, "Filename longer than NAME_MAX (%s), skipping.", de->d_name);
				continue;
			}
			xstrcpy(dp, de->d_name);
			xstrcpy(dpd, de->d_name);
			xstrcat(dpd, dfsuffix);
			if (!stat(df, &st) && !stat(dfd, &std)
					&& ((!maxfs || (st.st_size < maxfs)) == kind)) {
				xdebug("removing %s", de->d_name);
				unlink(df);
				unlink(dfd);
			}
		}

		closedir(d);
		xfree(df);
		xfree(dfd);
	}
}

static COMMAND(xmsg_msg)
{
	char fn[sizeof(XMSG_TMPFILE_PATH)];
	int fd;
	char *msg = (char*) params[1];
	char *uid;
	int fs;
	int n;
	const char *msgcmd = session_get(session, "send_cmd");
	char *msgx = NULL, *mymsg;
	
	if (!(uid = get_uid(session, target))) {
		printq("invalid_session");
		return -1;
	}

	if (!msgcmd || *msgcmd == '\0') {
		printq("xmsg_nosendcmd", session_name(session));
		return -1;
	}
	
	xstrcpy(fn, XMSG_TMPFILE_PATH);
	
	fd = mkstemp(fn);
	if (fd == -1)
		xerrn("Unable to create temp file");
	{
		const char *charset = session_get(session, "charset");

		if (charset)
			msgx = ekg_convert_string(msg, NULL, charset);
		mymsg = (msgx ? msgx : msg);
	}
	fs = xstrlen(mymsg);

	while (fs > 0) {
		if ((n = write(fd, mymsg, fs)) == -1) {
			unlink(fn);
			close(fd);
			xfree(msgx);
			xerrn("Unable to write message into temp file");
		}
		fs -= n;
		mymsg += n;
	}

	xfree(msgx);	
	close(fd);
	if ((command_exec_format(NULL, session, 1, "!^%s \"%s\" \"%s\"", msgcmd, target+XMSG_UID_DIROFFSET, fn)))
		xerr("msgcmd exec failed");
	
	{
		char *sess	= xstrdup(session_uid_get(session));
		char *me	= xstrdup(sess);
		char **rcpts	= xcalloc(2, sizeof(char *));
		char *msg	= xstrdup(params[1]);
		time_t sent	= time(NULL);
		int class	= (xstrcmp(name, "chat") ? EKG_MSGCLASS_SENT : EKG_MSGCLASS_SENT_CHAT);
		int ekgbeep	= EKG_NO_BEEP;
		char *format	= NULL;
		char *seq	= NULL;
		int secure	= 0;

		rcpts[0]	= xstrdup(uid);
		rcpts[1]	= NULL;

		query_emit_id(NULL, PROTOCOL_MESSAGE, &sess, &me, &rcpts, &msg, &format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(msg);
		xfree(me);
		xfree(sess);
		array_free(rcpts);
	}
			
	return 0;
}

static COMMAND(xmsg_inline_msg)
{
	const char *par[2] = {NULL, params[0]};
	if (!params[0] || !target)
		return -1;
	
	return xmsg_msg(("chat"), par, session, target, quiet);
}

static int xmsg_theme_init(void)
{
#ifndef NO_DEFAULT_THEME
	format_add("xmsg_addwatch_failed", _("Unable to add inotify watch (wrong path?)"), 1);
	format_add("xmsg_nosendcmd", _("%> (%1) You need to set %csend_cmd%n to be able to send msgs"), 1);
	format_add("xmsg_toobig", _("%> (%2) File %T%1%n is larger than %cmax_filesize%n, skipping"), 1);
	format_add("xmsg_toobigrm", _("%> (%2) File %T%1%n was larger than %cmax_filesize%n, removed"), 1);
	format_add("xmsg_umount", _("volume containing watched directory was unmounted"), 1);
#endif
	return 0;
}

static plugins_params_t xmsg_plugin_vars[] = {
	PLUGIN_VAR_ADD("auto_connect",		SESSION_VAR_AUTO_CONNECT, VAR_BOOL, "1", 0, NULL),
	PLUGIN_VAR_ADD("charset",		0, VAR_STR, "", 0, NULL),
	PLUGIN_VAR_ADD("dotfile_suffix",	0, VAR_STR, "", 0, NULL),
	PLUGIN_VAR_ADD("log_formats", 		SESSION_VAR_LOG_FORMATS, VAR_STR, "simple", 0, NULL),
	PLUGIN_VAR_ADD("max_filesize", 		0, VAR_INT, XMSG_MAXFS_DEF, 0, NULL),
	PLUGIN_VAR_ADD("max_oneshot_files",	0, VAR_INT, XMSG_MAXFC_DEF, 0, NULL),
	PLUGIN_VAR_ADD("name_separator", 	0, VAR_STR, XMSG_NAMESEP_DEF, 0, NULL),
	PLUGIN_VAR_ADD("oneshot_resume_timer",	0, VAR_INT, XMSG_MAXFC_TIMER, 0, NULL),
	PLUGIN_VAR_ADD("send_cmd", 		0, VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("rescan_timer",		0, VAR_INT, XMSG_TIMER_DEF, 0, xmsg_timer_change),
	PLUGIN_VAR_ADD("unlink_sent",		0, VAR_BOOL, "1", 0, xmsg_unlink_dotfiles),
	PLUGIN_VAR_ADD("unlink_toobig",		0, VAR_BOOL, "0", 0, xmsg_unlink_dotfiles),

	PLUGIN_VAR_END()
};

int xmsg_plugin_init(int prio)
{
#ifdef HAVE_INOTIFY
	if ((in_fd = inotify_init()) == -1)
		xerrn("unable to init inotify");
#endif /*HAVE_INOTIFY*/
	
	xdebug("inotify fd = %d", in_fd);
	
	xmsg_plugin.params = xmsg_plugin_vars;
	plugin_register(&xmsg_plugin, prio);

	query_connect_id(&xmsg_plugin, PROTOCOL_VALIDATE_UID, xmsg_validate_uid, NULL);
	query_connect_id(&xmsg_plugin, EKG_SIGUSR1, xmsg_handle_sigusr, NULL);

#define XMSG_CMDFLAGS SESSION_MUSTBELONG
#define XMSG_CMDFLAGS_TARGET SESSION_MUSTBELONG|COMMAND_ENABLEREQPARAMS|COMMAND_PARAMASTARGET|SESSION_MUSTBECONNECTED
	command_add(&xmsg_plugin, "xmsg:", "?", xmsg_inline_msg, XMSG_CMDFLAGS, NULL);
	command_add(&xmsg_plugin, "xmsg:chat", "!uU !", xmsg_msg, XMSG_CMDFLAGS_TARGET, NULL);
	command_add(&xmsg_plugin, "xmsg:connect", NULL, xmsg_connect, XMSG_CMDFLAGS, NULL);
	command_add(&xmsg_plugin, "xmsg:disconnect", NULL, xmsg_disconnect, XMSG_CMDFLAGS, NULL);
	command_add(&xmsg_plugin, "xmsg:msg", "!uU !", xmsg_msg, XMSG_CMDFLAGS_TARGET, NULL);
	command_add(&xmsg_plugin, "xmsg:reconnect", NULL, xmsg_reconnect, XMSG_CMDFLAGS, NULL);
#undef XMSG_CMDFLAGS_TARGET
#undef XMSG_CMDFLAGS

#ifdef HAVE_INOTIFY
	variable_add(&xmsg_plugin, "max_inotifycount", VAR_INT, 1, &config_maxinotifycount, NULL, NULL, NULL);
	watch_add(&xmsg_plugin, in_fd, WATCH_READ, xmsg_handle_data, NULL);
#endif /*HAVE_INOTIFY*/
	
	return 0;
}

static int xmsg_plugin_destroy(void)
{
	plugin_unregister(&xmsg_plugin);

	close(in_fd);
	xfree(ev);

	return 0;
}
