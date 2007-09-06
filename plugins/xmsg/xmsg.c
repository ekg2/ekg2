
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
/* direct from sys/inotify.h */
#define IN_ONLYDIR       0x01000000
#endif
#endif /*HAVE_INOTIFY*/

/* some additional defines */
#define XMSG_UID_DIROFFSET 5
#define XMSG_NAMESEP_DEF "."
#define XMSG_MAXFS_DEF "16384"
#define XMSG_MAXFC_DEF "25"
#define XMSG_MAXFC_TIMER "3"

/* if we have inotify, we don't need that timer */
#ifdef HAVE_INOTIFY
#define XMSG_MAXFC_INOTIFY 25
#define XMSG_TIMER_DEF "0"
#else
#define XMSG_TIMER_DEF "300"
#endif

/* debugs */
#define xerr(txt, ...) do { debug_error("[xmsg] " __FUNC__ ": " txt "\n", ##__VA_ARGS__); XERRADD return -1; } while (0)
#define xerrn(txt, ...) do { debug_error("[xmsg] " __FUNC__ ": " txt ": %s\n", ##__VA_ARGS__, strerror(errno)); XERRADD return -1; } while (0)
#define xdebug(txt, ...) debug("[xmsg] " __FUNC__ ": " txt "\n", ##__VA_ARGS__)
#define xdebug2(lvl, txt, ...) debug_ext(lvl, "[xmsg] " __FUNC__ ": " txt "\n", ##__VA_ARGS__)
#define XERRADD

/* global vars */
static int in_fd = 0;
static struct inotify_event *ev = NULL;
#ifdef HAVE_INOTIFY
static int config_maxinotifycount = XMSG_MAXFC_INOTIFY;
#endif

/* function declarations */
static char *xmsg_dirfix(const char *path);
#ifdef HAVE_INOTIFY
static WATCHER(xmsg_handle_data);
#endif /*HAVE_INOTIFY*/
static TIMER(xmsg_iterate_dir);
static int xmsg_handle_file(session_t *s, const char *fn);
static QUERY(xmsg_validate_uid);
static COMMAND(xmsg_connect);
static COMMAND(xmsg_disconnect);
static COMMAND(xmsg_reconnect);
static inline int xmsg_add_watch(session_t *s, const char *f);
static void xmsg_timer_change(session_t *s, const char *varname);
static void xmsg_unlink_dotfiles(session_t *s, const char *varname);
static COMMAND(xmsg_inline_msg);
static COMMAND(xmsg_msg);
static int xmsg_theme_init(void);
int xmsg_plugin_init(int prio);
static int xmsg_plugin_destroy(void);

/* constructor */
PLUGIN_DEFINE(xmsg, PLUGIN_PROTOCOL, xmsg_theme_init);

/* the code */

static char *xmsg_dirfix(const char *path)
{
#define __FUNC__ "xmsg_dirfix"
	char *tmp = xstrdup(path), *p;
	
	/* if paths starts with slash, we leave it as is,
	 * otherwise we convert # to / */
	if (*tmp != '/') {
		for (p = xstrchr(tmp, '#'); p; p = xstrchr(p+1, '#'))
			*p = '/';
	}

	xdebug("in: %s, out: %s", path, tmp);

	return tmp;
#undef __FUNC__
}

#ifdef HAVE_INOTIFY
static WATCHER(xmsg_handle_data)
{
#define __FUNC__ "xmsg_handle_data"
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
					/* to gain timer name different from that used with normal timers,
					 * we leave ':' in the beginning of UID */
					if (!timer_remove(&xmsg_plugin, session_uid_get(s)+XMSG_UID_DIROFFSET-1))
						xdebug("old oneshot resume timer removed");
					if ((i > 0) && timer_add(&xmsg_plugin, session_uid_get(s)+XMSG_UID_DIROFFSET-1, i, 0, xmsg_iterate_dir, s))
						xdebug("oneshot resume timer added");
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
#undef __FUNC__
}
#endif /*HAVE_INOTIFY*/

static TIMER(xmsg_iterate_dir)
{
#define __FUNC__ "xmsg_iterate_dir"
#define s (session_t*) data
	char *dir;
	DIR *d;
	struct dirent *de;
	int n = 0;
	const int maxn = session_int_get(s, "max_oneshot_files");

	if (type)
		return -1;
	
	dir = xmsg_dirfix(session_uid_get(s)+XMSG_UID_DIROFFSET);
	d = opendir(dir);
	xfree(dir);

	if (!d) {
		xdebug("unable to open specified directory");
		return 0;
	}
	
	while ((de = readdir(d))) {
		if (!xmsg_handle_file(s, de->d_name))
			n++;
		
		if ((maxn > 0) && n >= maxn) {
			const int i = session_int_get(s, "oneshot_resume_timer");
			/* to gain timer name different from that used with normal timers,
			 * we leave ':' in the beginning of UID */
			if ((i > 0) && timer_add(&xmsg_plugin, session_uid_get(s)+XMSG_UID_DIROFFSET-1, i, 0, xmsg_iterate_dir, s))
				xdebug("oneshot resume timer added");
			break;
		}
	}
	closedir(d);
	xdebug("processed %d files", n);

	return 0;
#undef s
#undef __FUNC__
}

static int xmsg_handle_file(session_t *s, const char *fn)
{
#define __FUNC__ "xmsg_handle_file"
	char *dir;
#undef XERRADD
#define XERRADD xfree(dir);
	const int nounlink = !session_int_get(s, "unlink_sent");
	const int utb = session_int_get(s, "unlink_toobig");
	const int maxfs = session_int_get(s, "max_filesize");
	const char *dfsuffix = session_get(s, "dotfile_suffix");
	char *namesep = (char*) session_get(s, "name_separator");

	char *msg;
	char *f;
	int fd, fs;
	time_t ft;
	
	if (*fn == '.') /* we're skipping ALL dotfiles */
		return -1;
	dir = xmsg_dirfix(session_uid_get(s)+XMSG_UID_DIROFFSET);
	f = xmalloc(xstrlen(dir) + xstrlen(fn) + 2);
#undef XERRADD
#define XERRADD xfree(f); xfree(dir);
	xstrcpy(f, dir);
	xstrcat(f, "/"); /* double trailing slash shouldn't make any problems */
	xstrcat(f, fn);
	
	xdebug("s = %s, d = %s, fn = %s, f = %s", session_uid_get(s), dir, fn, f);
	
	{
		struct stat st, std;
		char *dotf = NULL;
		int r;
		
		if (nounlink || !utb) {
			dotf = xmalloc(xstrlen(f) + 2 + xstrlen(dfsuffix));
#undef XERRADD
#define XERRADD xfree(f); xfree(dotf); xfree(dir);
			xstrcpy(dotf, dir);
			xstrcat(dotf, "/.");
			xstrcat(dotf, fn);
			xstrcat(dotf, dfsuffix);
			
			r = !(stat(dotf, &std) || S_ISDIR(std.st_mode));
		} else
			r = 0;
		
		if (stat(f, &st) || !S_ISREG(st.st_mode)) {
			if (r) /* clean up stale dotfile */
				unlink(dotf);
			XERRADD
			return -1;
		} else if (r) {
			XERRADD
			return -1;
		} else if (nounlink || (!utb && maxfs && (st.st_size > maxfs))) {
			if (!(maxfs && (st.st_size > maxfs) && utb))
				close(open(dotf, O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW, 0600));
		}
		
		fs = st.st_size;
		/* mtime > ctime > atime > time(NULL) */
#define X(x,y) (x ? x : y)
		ft = X(st.st_mtime, X(st.st_ctime, X(st.st_atime, time(NULL))));
#undef X
		xfree(dotf);
#undef XERRADD
#define XERRADD xfree(f);
	}
	xfree(dir);
	
	if (maxfs && (fs > maxfs)) {
		print((utb ? "xmsg_toobigrm" : "xmsg_toobig"), fn, session_name(s));
		if (utb)
			unlink(f);
		XERRADD
		return -1; /* we don't want to unlink such files */
	} else if (fs == 0)
		xdebug("empty file found, not submitting");
	else {
		fd = open(f, O_RDONLY);
		if (fd < 0)
			xerrn("unable to open given file");
#undef XERRADD
#define XERRADD close(fd); xfree(f);
		
		if ((msg = mmap(NULL, fs, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
			xerrn("mmap failed");
		
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

			query_emit_id(NULL, PROTOCOL_MESSAGE, &session, &uid, &rcpts, &msg, &format, &sent, &class, &seq, &dobeep, &secure);

			xfree(uid);
			xfree(session);
		}
		
		munmap(msg, fs);
		close(fd);
	}
	
	if (!nounlink)
		unlink(f);
	xfree(f);
#undef XERRADD
#define XERRADD

	return 0;
#undef __FUNC__
}

static QUERY(xmsg_validate_uid)
{
#define __FUNC__ "xmsg_validate_uid"
	char *uid = *(va_arg(ap, char**));
	int *valid = va_arg(ap, int*);
	
	if (uid && !xstrncasecmp(uid, "xmsg:", XMSG_UID_DIROFFSET)) {
		(*valid)++;
		return -1;
	}

	return 0;
#undef __FUNC__
}

static inline int xmsg_add_watch(session_t *s, const char *f)
{
#define __FUNC__ "xmsg_add_watch"
	struct stat fs;
	char *dir = xmsg_dirfix(f);
#undef XERRADD
#define XERRADD xfree(dir);

	if (!stat(dir, &fs)) {
		if (!S_ISDIR(fs.st_mode))
			xerr("given path is a file, not a directory");
	} else {
		if (mkdir(f, 0777))
			xerrn("mkdir failed");
	}

#ifdef HAVE_INOTIFY
	if ((s->priv = (void*) inotify_add_watch(in_fd, dir, (IN_CLOSE_WRITE|IN_MOVED_TO|IN_ONLYDIR))) == (void*) -1)
		xerrn("unable to add inotify watch");
	
	xdebug("inotify watch added: %d", (uint32_t) s->priv);
#endif /*HAVE_INOTIFY*/
	
	xfree(dir);
#undef XERRADD
#define XERRADD
	return 0;
#undef __FUNC__
}

static COMMAND(xmsg_connect)
{
#define __FUNC__ "xmsg_connect"
	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (xmsg_add_watch(session, session_uid_get(session)+XMSG_UID_DIROFFSET)) {
		print("conn_failed", format_find("xmsg_addwatch_failed"), session_name(session));
		return -1;
	}
	
	session_connected_set(session, 1);
	session_status_set(session, EKG_STATUS_AVAIL);
	{
		char *sess = xstrdup(session_uid_get(session));

		query_emit_id(NULL, PROTOCOL_CONNECTED, &sess);

		xfree(sess);
	}

	xmsg_iterate_dir(0, (void*) session);
	xmsg_timer_change(session, "rescan_timer");

	return 0;
#undef __FUNC__
}

/* we return 0 even if rmwatch fails, because xmsg_handle_data checks
 * if our session is still connected, so it'll ignore unneeded events */
static COMMAND(xmsg_disconnect)
{
#define __FUNC__ "xmsg_disconnect"
	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}
	
	xmsg_timer_change(session, NULL);
	if (!timer_remove(&xmsg_plugin, session_uid_get(session)+XMSG_UID_DIROFFSET-1))
		xdebug("old oneshot resume timer removed");
	session_status_set(session, EKG_STATUS_NA);
	session_connected_set(session, 0);
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
#undef __FUNC__
}

static COMMAND(xmsg_reconnect)
{
#define __FUNC__ "xmsg_reconnect"
	if (session_connected_get(session)) {
		xmsg_disconnect(name, params, session, target, quiet);
	}

	return xmsg_connect(name, params, session, target, quiet);
#undef __FUNC__
}

static void xmsg_timer_change(session_t *s, const char *varname)
{
#define __FUNC__ "xmsg_timer_change"
	int n = (varname ? session_int_get(s, varname) : 0);
	/* we simplify the whole process by using our UID as timer name */
	
	xdebug("n = %d", n);
	if (!varname || session_connected_get(s)) {
		if (!timer_remove(&xmsg_plugin, session_uid_get(s)+XMSG_UID_DIROFFSET))
			xdebug("old timer removed");
		if (n > 0) {
			if (timer_add(&xmsg_plugin, session_uid_get(s)+XMSG_UID_DIROFFSET, n, 1, xmsg_iterate_dir, s))
				xdebug("new timer added");
		}
	}
#undef __FUNC__
}

/* kind = 0 for sent, 1 for toobig */
static void xmsg_unlink_dotfiles(session_t *s, const char *varname)
{
#define __FUNC__ "xmsg_unlink_dotfiles"
	if (session_int_get(s, varname)) {
		const int kind = !xstrcasecmp(varname, "unlink_sent");
		const int maxfs = session_int_get(s, "max_filesize");
		const char *dfsuffix = session_get(s, "dotfile_suffix");
		char *dir = xmsg_dirfix(session_uid_get(s)+XMSG_UID_DIROFFSET);
		DIR *d = opendir(dir);
		struct dirent *de;
		struct stat st, std;
		char *df, *dfd, *dp, *dpd;
		
		if (!d) {
			xdebug("unable to open specified directory");
			return;
		}
		
		df = xmalloc(xstrlen(dir) + NAME_MAX + 2);
		dfd = xmalloc(xstrlen(dir) + NAME_MAX + 3 + xstrlen(dfsuffix));
		xstrcpy(df, dir);
		xfree(dir);
		dp = df + xstrlen(df);
		*(dp++) = '/';
		xstrcpy(dfd, df);
		dpd = dfd + xstrlen(dfd);
		*(dpd++) = '.';
		
		while ((de = readdir(d))) {
			if (de->d_name[0] == '.')
				continue;
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
#undef __FUNC__
}

static COMMAND(xmsg_inline_msg)
{
#define __FUNC__ "xmsg_inline_msg"
	const char *par[2] = {NULL, params[0]};
	if (!params[0] || !target)
		return -1;
	
	return xmsg_msg(("chat"), par, session, target, quiet);
#undef __FUNC__
}

static COMMAND(xmsg_msg)
{
#define __FUNC__ "xmsg_msg"
	char *fn;
	int fd;
	char *msg = (char*) params[1];
	char *uid;
	int fs = xstrlen(msg);
	int n;
	const char *msgcmd = session_get(session, "send_cmd");
	
	if (!(uid = get_uid(session, target)))
		uid = (char*) target;
	if (xstrncasecmp(uid, "xmsg:", 5)) {
		printq("invalid_session");
		return -1;
	}

	if (!msgcmd || *msgcmd == '\0') {
		printq("xmsg_nosendcmd", session_name(session));
		return -1;
	}
	
	fn = xstrdup("/tmp/xmsg.XXXXXX");
#undef XERRADD
#define XERRADD xfree(fn);
	
	fd = mkstemp(fn); /* XXX: we are including the correct file, so wtf?! */
	if (fd == -1)
		xerrn("Unable to create temp file");
#undef XERRADD
#define XERRADD close(fd); unlink(fn); xfree(fn);

	while (fs > 0) {
		if ((n = write(fd, msg, fs)) == -1)
			xerrn("Unable to write message into temp file");
		fs -= n;
		msg += n;
	}
	
	close(fd);
#undef XERRADD
#define XERRADD xfree(fn);
	if ((command_exec_format(NULL, session, 1, "!^%s \"%s\" \"%s\"", msgcmd, target+XMSG_UID_DIROFFSET, fn)))
		xerr("msgcmd exec failed");
	
	xfree(fn);
#undef XERRADD
#define XERRADD
	
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
#undef __FUNC__
}

static int xmsg_theme_init(void)
{
#define __FUNC__ "xmsg_theme_init"
#ifndef NO_DEFAULT_THEME
	format_add("xmsg_addwatch_failed", _("Unable to add inotify watch (wrong path?)"), 1);
	format_add("xmsg_nosendcmd", _("%> (%1) You need to set %csend_cmd%n to be able to send msgs"), 1);
	format_add("xmsg_toobig", _("%> (%2) File %T%1%n is larger than %cmax_filesize%n, skipping"), 1);
	format_add("xmsg_toobigrm", _("%> (%2) File %T%1%n was larger than %cmax_filesize%n, removed"), 1);
	format_add("xmsg_umount", _("volume containing watched directory was unmounted"), 1);
#endif
	return 0;
#undef __FUNC__
}

int xmsg_plugin_init(int prio)
{
#define __FUNC__ "xmsg_plugin_init"
#ifdef HAVE_INOTIFY
	if ((in_fd = inotify_init()) == -1)
		xerrn("unable to init inotify");
#endif /*HAVE_INOTIFY*/
	
	xdebug("inotify fd = %d", in_fd);
	
	plugin_register(&xmsg_plugin, prio);

	query_connect_id(&xmsg_plugin, PROTOCOL_VALIDATE_UID, xmsg_validate_uid, NULL);

	plugin_var_add(&xmsg_plugin, "auto_connect", VAR_BOOL, "1", 0, NULL);
	plugin_var_add(&xmsg_plugin, "dotfile_suffix", VAR_STR, "", 0, NULL);
	plugin_var_add(&xmsg_plugin, "log_formats", VAR_STR, "simple", 0, NULL);
	plugin_var_add(&xmsg_plugin, "max_filesize", VAR_INT, XMSG_MAXFS_DEF, 0, NULL);
	plugin_var_add(&xmsg_plugin, "max_oneshot_files", VAR_INT, XMSG_MAXFC_DEF, 0, NULL);
	plugin_var_add(&xmsg_plugin, "name_separator", VAR_STR, XMSG_NAMESEP_DEF, 0, NULL);
	plugin_var_add(&xmsg_plugin, "oneshot_resume_timer", VAR_INT, XMSG_MAXFC_TIMER, 0, NULL);
	plugin_var_add(&xmsg_plugin, "send_cmd", VAR_STR, NULL, 0, NULL);
	plugin_var_add(&xmsg_plugin, "rescan_timer", VAR_INT, XMSG_TIMER_DEF, 0, xmsg_timer_change);
	plugin_var_add(&xmsg_plugin, "unlink_sent", VAR_BOOL, "1", 0, xmsg_unlink_dotfiles);
	plugin_var_add(&xmsg_plugin, "unlink_toobig", VAR_BOOL, "0", 0, xmsg_unlink_dotfiles);

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
#undef __FUNC__
}

static int xmsg_plugin_destroy(void)
{
#define __FUNC__ "xmsg_plugin_destroy"
	plugin_unregister(&xmsg_plugin);

	close(in_fd);
	xfree(ev);

	return 0;
#undef __FUNC__
}
