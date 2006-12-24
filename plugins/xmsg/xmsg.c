
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
#endif /*HAVE_INOTIFY*/

/* some additional defines */
#define XMSG_UID_DIROFFSET 5
#define XMSG_MAXFS_DEF "16384"
/* if we have inotify, we don't need that timer */
#ifdef HAVE_INOTIFY
#define XMSG_TIMER_DEF "0"
#else
#define XMSG_TIMER_DEF "300"
#endif

/* debugs */
#define xerr(txt, ...) do { debug_error("[xmsg] " __FUNC__ ": " txt "\n", ##__VA_ARGS__); XERRADD return -1; } while (0)
#define xerrn(txt, ...) do { debug_error("[xmsg] " __FUNC__ ": " txt ": %s\n", ##__VA_ARGS__, strerror(errno)); XERRADD return -1; } while (0)
#define xdebug(txt, ...) do { debug("[xmsg] " __FUNC__ ": " txt "\n", ##__VA_ARGS__); } while (0)
#define xdebug2(lvl, txt, ...) do { debug_ext(lvl, "[xmsg] " __FUNC__ ": " txt "\n", ##__VA_ARGS__); } while (0)
#define XERRADD

/* global vars */
static int in_fd = 0;
static struct inotify_event *ev = NULL;

/* function declarations */
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
static COMMAND(xmsg_inline_msg);
static COMMAND(xmsg_msg);
static int xmsg_theme_init(void);
int xmsg_plugin_init(int prio);
static int xmsg_plugin_destroy(void);

/* constructor */
PLUGIN_DEFINE(xmsg, PLUGIN_PROTOCOL, xmsg_theme_init);

/* the code */

#ifdef HAVE_INOTIFY
/* watch inotify fd and parse incoming events */
static WATCHER(xmsg_handle_data)
{
#define __FUNC__ "xmsg_handle_data"
	int n;
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

			if (s && (s->priv == (void*) evp->wd))
				break;
		}
		
		xdebug("n = %d, wd = %d, str = %s", n, evp->wd, evp->name);
			
		if ((evp->mask & IN_IGNORED) || !s || !session_connected_get(s))
			return 0;
		else if (evp->mask & IN_UNMOUNT)
			xmsg_disconnect(NULL, NULL, s, NULL, -1);
		else if (evp->mask & IN_Q_OVERFLOW)
			xmsg_iterate_dir(0, (void*) s);

		xmsg_handle_file(s, evp->name);
	}

	return 0;
#undef __FUNC__
}
#endif /*HAVE_INOTIFY*/

/* iterate over all files existing in directory,
 * used on startup and queue overflow,
 * syntax changed to match TIMER(...) */
static TIMER(xmsg_iterate_dir)
{
#define __FUNC__ "xmsg_iterate_dir"
	const char *dir = session_uid_get((session_t*) data)+XMSG_UID_DIROFFSET;
	DIR *d = opendir(dir);
	struct dirent *de;

	if (type)
		return -1;
	
	if (!d) {
		xdebug("unable to open specified directory");
		return 0;
	}
	
	while ((de = readdir(d))) {
		if (!xstrcmp(de->d_name, ".") || !xstrcmp(de->d_name, ".."))
			continue;
		xmsg_handle_file((session_t*) data, de->d_name);
	}
	closedir(d);

	return 0;
#undef __FUNC__
}

/* try to submit, then unlink, incoming file */
static int xmsg_handle_file(session_t *s, const char *fn)
{
#define __FUNC__ "xmsg_handle_file"
	const char *dir = session_uid_get(s)+XMSG_UID_DIROFFSET;
	char *msg;
	char *f = xmalloc(xstrlen(dir) + xstrlen(fn) + 2);
	int fd, fs, maxfs;
#undef XERRADD
#define XERRADD xfree(f);
	strcpy(f, dir);
	strcat(f, "/"); /* double trailing slash shouldn't make any problems */
	strcat(f, fn);
	
	xdebug("s = %s, d = %s, fn = %s, f = %s", session_uid_get(s), dir, fn, f);
	
	{
		struct stat st;

		if (stat(f, &st) || !S_ISREG(st.st_mode)) {
			xfree(f);
			return 0;
		}
		fs = st.st_size;
	}
	
	maxfs = session_int_get(s, "max_filesize");
	if (maxfs && fs > maxfs) {
		print("xmsg_toobig", fn, session_name(s));
		XERRADD
		return 0; /* we don't want to unlink such files */
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
			time_t sent	= time(NULL);
			int class	= EKG_MSGCLASS_CHAT;
			char *seq	= NULL;
			int dobeep	= EKG_TRY_BEEP;
			int secure	= 0;
			char *p;

			xstrcpy(uid, "xmsg:");
			xstrcat(uid, fn);
			if ((p = xstrchr(uid+XMSG_UID_DIROFFSET, ':')))
				*p = '\0';

			query_emit_id(NULL, PROTOCOL_MESSAGE, &session, &uid, &rcpts, &msg, &format, &sent, &class, &seq, &dobeep, &secure);

			xfree(uid);
			xfree(session);
		}
		
		munmap(msg, fs);
		close(fd);
	}
	
	unlink(f);
	xfree(f);
#undef XERRADD
#define XERRADD

	return 0;
#undef __FUNC__
}

/* check if or session id starts with xmsg: */
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

/* add inotify watch (used within connect) */
static inline int xmsg_add_watch(session_t *s, const char *f)
{
#define __FUNC__ "xmsg_add_watch"
	struct stat fs;

	if (!stat(f, &fs)) {
		if (!S_ISDIR(fs.st_mode))
			xerr("given path is a file, not a directory");
	} else {
		if (mkdir(f, 0777))
			xerrn("mkdir failed");
	}

#ifdef HAVE_INOTIFY
	if ((s->priv = (void*) inotify_add_watch(in_fd, f, (IN_CLOSE_WRITE|IN_MOVED_TO|IN_ONLYDIR))) == (void*) -1)
		xerrn("unable to add inotify watch");
	
	xdebug("inotify watch added: %d", (uint32_t) s->priv);
#endif /*HAVE_INOTIFY*/
	
	return 0;
#undef __FUNC__
}

/* connect = add inotify watch */
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

/* disconnect = remove inotify watch
 * we return 0 even if rmwatch fails, because xmsg_handle_data checks
 * if our session is still connected, so it'll ignore unneeded events */
static COMMAND(xmsg_disconnect)
{
#define __FUNC__ "xmsg_disconnect"
	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}
	
	xmsg_timer_change(session, NULL);
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

/* funny thing called reconnect */
static COMMAND(xmsg_reconnect)
{
#define __FUNC__ "xmsg_reconnect"
	if (session_connected_get(session)) {
		xmsg_disconnect(name, params, session, target, quiet);
	}

	return xmsg_connect(name, params, session, target, quiet);
#undef __FUNC__
}

/* update our timer */
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

/* handle inline messages */
static COMMAND(xmsg_inline_msg)
{
#define __FUNC__ "xmsg_inline_msg"
	const char *par[2] = {NULL, params[0]};
	if (!params[0] || !target)
		return -1;
	
	return xmsg_msg(("chat"), par, session, target, quiet);
#undef __FUNC__
}

/* main send message handler */
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

/* init our lovely formats */
static int xmsg_theme_init(void)
{
#define __FUNC__ "xmsg_theme_init"
#ifndef NO_DEFAULT_THEME
	format_add("xmsg_addwatch_failed", _("Unable to add inotify watch (wrong path?)"), 1);
	format_add("xmsg_nosendcmd", _("%> (%1) You need to set %csend_cmd%n to be able to send msgs"), 1);
	format_add("xmsg_toobig", _("%> (%2) File %T%1%n is larger than %cmax_filesize%n, skipping"), 1);
	format_add("xmsg_umount", _("volume containing watched directory was unmounted"), 1);
#endif
	return 0;
#undef __FUNC__
}

/* init plugin and inotify */
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
	plugin_var_add(&xmsg_plugin, "max_filesize", VAR_INT, XMSG_MAXFS_DEF, 0, NULL);
	plugin_var_add(&xmsg_plugin, "send_cmd", VAR_STR, NULL, 0, NULL);
	plugin_var_add(&xmsg_plugin, "rescan_timer", VAR_INT, XMSG_TIMER_DEF, 0, xmsg_timer_change);

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
	watch_add(&xmsg_plugin, in_fd, WATCH_READ, xmsg_handle_data, NULL);
#endif /*HAVE_INOTIFY*/
	
	return 0;
#undef __FUNC__
}

/* cleanup */ 
static int xmsg_plugin_destroy(void)
{
#define __FUNC__ "xmsg_plugin_destroy"
	plugin_unregister(&xmsg_plugin);

	close(in_fd);
	xfree(ev);

	return 0;
#undef __FUNC__
}
