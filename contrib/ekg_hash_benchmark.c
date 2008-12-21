#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

typedef int hash_t;

hash_t no_prompt_cache_hash = 0x139dcbd6;	/* hash value of "no_promp_cache" 2261954 it's default one. i hope good one.. for 32 bit x86 sure. */

hash_t ekg_hash(const char *name);

struct list {
	void *data;
	/*struct list *prev;*/
	struct list *next;
};

typedef struct list *list_t;

int hashes[256];

void ekg_oom_handler() { printf("braklo pamieci\n"); exit(1); }
void *xmalloc(size_t size) { void *tmp = malloc(size); if (!tmp) ekg_oom_handler(); memset(tmp, 0, size); return tmp; }
#define fix(s) ((s) ? (s) : "")
int xstrcmp(const char *s1, const char *s2) { return strcmp(fix(s1), fix(s2)); }
char *xstrdup(const char *s) { char *tmp; if (!s) return NULL; if (!(tmp = (char *) strdup(s))) ekg_oom_handler(); return tmp; }
void xfree(void *ptr) { if (ptr) free(ptr); }

void *list_add_beginning(list_t *list, void *data) {
	list_t new;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new = xmalloc(sizeof(struct list));
	new->next = *list;
	new->data = data;
	*list	  = new;

	return new->data;
}

struct format {
	char *name;
	hash_t name_hash;
	char *value;
};
list_t formats = NULL;

void format_add(const char *name, const char *value, int replace) {
	struct format *f;
	list_t l;
	hash_t hash;

	if (!name || !value)
		return;

	hash = ekg_hash(name);

	if (hash == no_prompt_cache_hash) {
		if (!xstrcmp(name, "no_prompt_cache")) {
			no_prompt_cache_hash = no_prompt_cache_hash;
			return;
		}
		printf("nothit_add0: %s vs no_prompt_cache\n", name);
	}

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;
		if (hash == f->name_hash) {
			if (!xstrcmp(name, f->name)) {
				if (replace) {
					xfree(f->value);
					f->value = xstrdup(value);
				}
				return;
			}
			printf("nothit_add: %s vs %s | %08x\n", name, f->name, hash);
		}
	}

	f = xmalloc(sizeof(struct format));
	f->name		= xstrdup(name);
	f->name_hash	= hash;
	f->value	= xstrdup(value);

	hashes[hash & 0xff]++;

	list_add_beginning(&formats, f);
	return;
}


#define ROL(x) (((x>>25)&0x7f)|((x<<7)&0xffffff80))
hash_t ekg_hash(const char *name) {	/* ekg_hash() from stuff.c (rev: 1.1 to 1.203, and later) */
	hash_t hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash = ROL(hash);
	}

	return hash;
}

int i = 0;

const char *format_find(const char *name) {
	const char *tmp;
	hash_t hash;
	list_t l;

	if (!name)
		return "";

	/* speech app */
	if (!strchr(name, ',')) {
		static char buf[1024];
		const char *tmp;

		snprintf(buf, sizeof(buf), "%s,speech", name);
		tmp = format_find(buf);
		if (tmp[0] != '\0')
			return tmp;
	}

	hash = ekg_hash(name);

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (hash == f->name_hash) {
			if (!xstrcmp(f->name, name))
				return f->value;

			printf("nothit_find: %s vs %s\n", name, f->name);
		}
	}
	return "";
}

int main() {
	no_prompt_cache_hash = ekg_hash("no_prompt_cache");
	fprintf(stderr, "no_prompt_cache %08x\n", no_prompt_cache_hash);

	/* first of all we add all formats to list */
#define _(x) x
	format_add("prompt", "%K:%g:%G:%n", 1);
	format_add("prompt,speech", " ", 1);
	format_add("prompt2", "%K:%c:%C:%n", 1);
	format_add("prompt2,speech", " ", 1);
	format_add("error", "%K:%r:%R:%n", 1);
	format_add("error,speech", "błąd!", 1);
	format_add("timestamp", "%T", 1);
	format_add("timestamp,speech", " ", 1);
	format_add("ncurses_prompt_none", "", 1);
	format_add("ncurses_prompt_query", "[%1] ", 1);
	format_add("statusbar", " %c(%w%{time}%c)%w %c(%w%{?session %{?away %G}%{?avail %Y}%{?chat %W}%{?dnd %K}%{?xa %g}%{?invisible %C}%{?notavail %r}%{session}}%{?!session ---}%c) %{?window (%wwin%c/%w%{?typing %C}%{window}}%{?query %c:%W%{query}}%{?debug %c(%Cdebug}%c)%w%{?activity  %c(%wact%c/%W}%{activity}%{?activity %c)%w}%{?mail  %c(%wmail%c/%w}%{mail}%{?mail %c)}%{?more  %c(%Gmore%c)}", 1);
	format_add("header", " %{?query %c(%{?query_away %w}%{?query_avail %W}%{?query_invisible %K}%{?query_notavail %k}%{query}%{?query_descr %c/%w%{query_descr}}%c) %{?query_ip (%wip%c/%w%{query_ip}%c)} %{irctopic}}%{?!query %c(%wekg2%c/%w%{version}%c) (%w%{url}%c)}", 1);
	format_add("statusbar_act_important", "%W", 1);
	format_add("statusbar_act", "%K", 1);
	format_add("statusbar_act_typing", "%c", 1);
	format_add("statusbar_act_important_typing", "%C", 1);
	format_add("statusbar_timestamp", "%H:%M", 1);
	format_add("known_user", "%T%1%n/%2", 1);
	format_add("known_user,speech", "%1", 1);
	format_add("unknown_user", "%T%1%n", 1);
	format_add("none", "%1\n", 1);
	format_add("generic", "%> %1\n", 1);
	format_add("generic_bold", "%> %T%1%n\n", 1);
	format_add("generic2", "%) %1\n", 1);
	format_add("generic2_bold", "%) %T%1%n\n", 1);
	format_add("generic_error", "%! %1\n", 1);
	format_add("debug", 	"%n%1\n", 1);
	format_add("fdebug",	"%b%1\n", 1);
	format_add("iodebug",	"%y%1\n", 1);
	format_add("iorecvdebug", "%Y%1\n", 1);
	format_add("edebug",	"%R%1\n", 1);
	format_add("value_none", _("(none)"), 1);
	format_add("not_enough_params", _("%! Too few parameters. Try %Thelp %1%n\n"), 1);
	format_add("invalid_params", _("%! Invalid parameters. Try %Thelp %1%n\n"), 1);
	format_add("var_not_set", _("%! Required variable %T%2%n by %T%1%n is unset\n"), 1);
	format_add("invalid_uid", _("%! Invalid user id\n"), 1);
	format_add("invalid_session", _("%! Invalid session\n"), 1);
	format_add("invalid_nick", _("%! Invalid username\n"), 1);
	format_add("user_not_found", _("%! User %T%1%n not found\n"), 1);
	format_add("not_implemented", _("%! This function isn't ready yet\n"), 1);
	format_add("unknown_command", _("%! Unknown command: %T%1%n\n"), 1);
	format_add("welcome", _("%> %Tekg2-%1%n (%ge%Gk%gg %Gr%ge%Gl%go%Ga%gd%Ge%gd%n)\n%> Software licensed on GPL v2 terms\n\n"), 1);
	format_add("welcome,speech", _("welcome in e k g 2."), 1);
	format_add("ekg_version", _("%) %Tekg2-%1%n (compiled %2)\n"), 1);
	format_add("secure", _("%Y(encrypted)%n"), 1);
	format_add("day_changed", _("%) Day changed to: %W%1"), 1);
	format_add("user_added", _("%> (%2) Added %T%1%n to roster\n"), 1);
	format_add("user_deleted", _("%) (%2) Removed %T%1%n from roster\n"), 1);
	format_add("user_cleared_list", _("%) (%1) Roster cleared\n"), 1);
	format_add("user_exists", _("%! (%2) %T%1%n already in roster\n"), 1);
	format_add("user_exists_other", _("%! (%3) %T%1%n already in roster as %2\n"), 1);
	format_add("away", _("%> (%1) Status changed to %Gaway%n\n"), 1);
	format_add("away_descr", _("%> (%3) Status changed to %Gaway%n: %T%1%n%2\n"), 1);
	format_add("back", _("%> (%1) Status changed to %Yavailable%n\n"), 1);
	format_add("back_descr", _("%> (%3) Status changed to %Yavailable%n: %T%1%n%2%n\n"), 1);
	format_add("invisible", _("%> (%1) Status changed to %cinvisible%n\n"), 1);
	format_add("invisible_descr", _("%> (%3) Status changed to %cinvisible%n: %T%1%n%2\n"), 1);
	format_add("dnd", _("%> (%1) Status changed to %Bdo not disturb%n\n"), 1);
	format_add("dnd_descr", _("%> (%3) Status changed to %Bdo not disturb%n: %T%1%n%2\n"), 1);
	format_add("ffc", _("%> (%1) Status changed to %Wfree for chat%n\n"), 1);
	format_add("ffc_descr", _("%> (%3) Status changed to %Wfree for chat%n: %T%1%n%2%n\n"), 1);
	format_add("xa", _("%> (%1) Status changed to %gextended away%n\n"), 1);
	format_add("xa_descr", _("%> (%3) Status changed to %gextended away%n: %T%1%n%2%n%n\n"), 1);
	format_add("private_mode_is_on", _("% (%1) Friends only mode is on\n"), 1);
	format_add("private_mode_is_off", _("%> (%1) Friends only mode is off\n"), 1);
	format_add("private_mode_on", _("%) (%1) Turned on ,,friends only'' mode\n"), 1);
	format_add("private_mode_off", _("%> (%1) Turned off ,,friends only'' mode\n"), 1);
	format_add("private_mode_invalid", _("%! Invalid value'\n"), 1);
	format_add("descr_too_long", _("%! Description longer than maximum %T%1%n characters\nDescr: %B%2%b%3%n\n"), 1);
	format_add("auto_away", _("%> (%1) Auto %Gaway%n\n"), 1);
	format_add("auto_away_descr", _("%> (%3) Auto %Gaway%n: %T%1%n%2%n\n"), 1);
	format_add("auto_xa", _("%> (%1) Auto %gextended away%n\n"), 1);
	format_add("auto_xa_descr", _("%> (%3) Auto %gextended away%n: %T%1%n%2%n\n"), 1);
	format_add("auto_back", _("%> (%1) Auto back%n\n"), 1);
	format_add("auto_back_descr", _("%> (%3) Auto back: %T%1%n%2%n\n"), 1);
	format_add("help", "%> %T%1%n %2 - %3\n", 1);
	format_add("help_no_params", "%> %T%1%n - %2\n", 1);
	format_add("help_more", "%) %|%1\n", 1);
	format_add("help_alias", _("%) %T%1%n is an alias and don't have description\n"), 1);
	format_add("help_footer", _("\n%> %|%Thelp <command>%n will show more details about command. Prepending %T^%n to command name will hide it's result. Instead of <uid/alias> one can use %T$%n, which means current query user.\n\n"), 1);
	format_add("help_quick", _("%> %|Before using consult the brochure. File %Tdocs/ULOTKA.en%n is a short guide on included documentation. If you don't have it, you can visit %Thttp://www.ekg2.org/%n\n"), 1);
	format_add("help_set_file_not_found", _("%! Can't find variables descriptions (incomplete installation)\n"), 1);
	format_add("help_set_file_not_found_plugin", _("%! Can't find variables descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
	format_add("help_set_var_not_found", _("%! Cant find description of %T%1%n variable\n"), 1);
	format_add("help_set_header", _("%> %T%1%n (%2, default value: %3)\n%>\n"), 1);
	format_add("help_set_body", "%> %|%1\n", 1);
	format_add("help_set_footer", "", 1);
	format_add("help_command_body", "%> %|%1\n", 1);
	format_add("help_command_file_not_found", _("%! Can't find commands descriptions (incomplete installation)\n"), 1);
	format_add("help_command_file_not_found_plugin", _("%! Can't find commands descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
	format_add("help_command_not_found", _("%! Can't find command description: %T%1%n\n"), 1);
	format_add("help_script", _("%) %T%1%n is an script command and don't have description. Try /%1 help\n"), 1);
	format_add("help_session_body", "%> %|%1\n", 1);
	format_add("help_session_file_not_found", _("%! Can't find variables descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
	format_add("help_session_var_not_found", _("%! Cant find description of %T%1%n variable\n"), 1);
	format_add("help_session_header", _("%> %1->%T%2%n (%3, default value: %4)\n%>\n"), 1);
	format_add("help_session_footer", "", 1);
	format_add("ignored_added", _("%> Ignoring %T%1%n\n"), 1);
	format_add("ignored_modified", _("%> Modified ignore level of %T%1%n\n"), 1);
	format_add("ignored_deleted", _("%) Unignored %1\n"), 1);
	format_add("ignored_deleted_all", _("%) Ignore list cleared up\n"), 1);
	format_add("ignored_exist", _("%! %1 already beeing ignored\n"), 1);
	format_add("ignored_list", "%> %1 %2\n", 1);
	format_add("ignored_list_empty", _("%! Ignore list ist empty\n"), 1);
	format_add("error_not_ignored", _("%! %1 is not beeing ignored\n"), 1);
	format_add("blocked_added", _("%> Blocking %T%1%n\n"), 1);
	format_add("blocked_deleted", _("%) Unblocking %1\n"), 1);
	format_add("blocked_deleted_all", _("%) Block list cleared up\n"), 1);
	format_add("blocked_exist", _("%! %1 already beeing blocked\n"), 1);
	format_add("blocked_list", "%> %1\n", 1);
	format_add("blocked_list_empty", _("%! Block list is empty\n"), 1);
	format_add("error_not_blocked", _("%! %1 is not beeing blocked\n"), 1);
	format_add("list_empty", _("%! Roster is empty\n"), 1);
	format_add("list_avail", _("%> %1 %Y(available)%n %b%3:%4%n\n"), 1);
	format_add("list_avail_descr", _("%> %1 %Y(available: %n%5%Y)%n %b%3:%4%n\n"), 1);
	format_add("list_away", _("%> %1 %G(away)%n %b%3:%4%n\n"), 1);
	format_add("list_away_descr", _("%> %1 %G(away: %n%5%G)%n %b%3:%4%n\n"), 1);
	format_add("list_dnd", _("%> %1 %B(do not disturb)%n %b%3:%4%n\n"), 1);
	format_add("list_dnd_descr", _("%> %1 %G(do not disturb:%n %5%G)%n %b%3:%4%n\n"), 1);
	format_add("list_chat", _("%> %1 %W(free for chat)%n %b%3:%4%n\n"), 1);
	format_add("list_chat_descr", _("%> %1 %W(free for chat%n: %5%W)%n %b%3:%4%n\n"), 1);
	format_add("list_error", _("%> %1 %m(error) %b%3:%4%n\n"), 1);
	format_add("list_error", _("%> %1 %m(error%n: %5%m)%n %b%3:%4%n\n"), 1);
	format_add("list_xa", _("%> %1 %g(extended away)%n %b%3:%4%n\n"), 1);
	format_add("list_xa_descr", _("%> %1 %g(extended away: %n%5%g)%n %b%3:%4%n\n"), 1);
	format_add("list_notavail", _("%> %1 %r(offline)%n\n"), 1);
	format_add("list_notavail_descr", _("%> %1 %r(offline: %n%5%r)%n\n"), 1);
	format_add("list_invisible", _("%> %1 %c(invisible)%n %b%3:%4%n\n"), 1);
	format_add("list_invisible_descr", _("%> %1 %c(invisible: %n%5%c)%n %b%3:%4%n\n"), 1);
	format_add("list_blocking", _("%> %1 %m(blocking)%n\n"), 1);
	format_add("list_unknown", "%> %1\n", 1);
	format_add("modify_offline", _("%> %1 will not see your status\n"), 1);
	format_add("modify_online", _("%> %1 will see your status\n"), 1);
	format_add("modify_done", _("%> Modified item in roster\n"), 1);
	format_add("contacts_header", "", 1);
	format_add("contacts_header_group", "%K %1%n", 1);
	format_add("contacts_metacontacts_header", "", 1);
	format_add("contacts_avail_header", "", 1);
	format_add("contacts_avail", " %Y%1%n", 1);
	format_add("contacts_avail_descr", "%Ki%Y%1%n", 1);
	format_add("contacts_avail_descr_full", "%Ki%Y%1%n %2", 1);
	format_add("contacts_avail_blink", " %Y%i%1%n", 1);
	format_add("contacts_avail_descr_blink", "%K%ii%Y%i%1%n", 1);
	format_add("contacts_avail_descr_full_blink", "%K%ii%Y%i%1%n %2", 1);
	format_add("contacts_avail_footer", "", 1);
	format_add("contacts_away_header", "", 1);
	format_add("contacts_away", " %G%1%n", 1);
	format_add("contacts_away_descr", "%Ki%G%1%n", 1);
	format_add("contacts_away_descr_full", "%Ki%G%1%n %2", 1);
	format_add("contacts_away_blink", " %G%i%1%n", 1);
	format_add("contacts_away_descr_blink", "%K%ii%G%i%1%n", 1);
	format_add("contacts_away_descr_full_blink", "%K%ii%G%i%1%n %2", 1);
	format_add("contacts_away_footer", "", 1);
	format_add("contacts_dnd_header", "", 1);
	format_add("contacts_dnd", " %B%1%n", 1);
	format_add("contacts_dnd_descr", "%Ki%B%1%n", 1);
	format_add("contacts_dnd_descr_full", "%Ki%B%1%n %2", 1);
	format_add("contacts_dnd_blink", " %B%i%1%n", 1);
	format_add("contacts_dnd_descr_blink", "%K%ii%B%i%1%n", 1);
	format_add("contacts_dnd_descr_full_blink", "%K%ii%B%i%1%n %2", 1);
	format_add("contacts_dnd_footer", "", 1);
	format_add("contacts_chat_header", "", 1);
	format_add("contacts_chat", " %W%1%n", 1);
	format_add("contacts_chat_descr", "%Ki%W%1%n", 1);
	format_add("contacts_chat_descr_full", "%Ki%W%1%n %2", 1);
	format_add("contacts_chat_blink", " %W%i%1%n", 1);
	format_add("contacts_chat_descr_blink", "%K%ii%W%i%1%n", 1);
	format_add("contacts_chat_descr_full_blink", "%K%ii%W%i%1%n %2", 1);
	format_add("contacts_chat_footer", "", 1);
	format_add("contacts_error_header", "", 1);
	format_add("contacts_error", " %m%1%n", 1);
	format_add("contacts_error_descr", "%Ki%m%1%n", 1);
	format_add("contacts_error_descr_full", "%Ki%m%1%n %2", 1);
	format_add("contacts_error_blink", " %m%i%1%n", 1);
	format_add("contacts_error_descr_blink", "%K%ii%m%i%1%n", 1);
	format_add("contacts_error_descr_full_blink", "%K%ii%m%i%1%n %2", 1);
	format_add("contacts_error_footer", "", 1);
	format_add("contacts_xa_header", "", 1);
	format_add("contacts_xa", " %g%1%n", 1);
	format_add("contacts_xa_descr", "%Ki%g%1%n", 1);
	format_add("contacts_xa_descr_full", "%Ki%g%1%n %2", 1);
	format_add("contacts_xa_blink", " %g%i%1%n", 1);
	format_add("contacts_xa_descr_blink", "%K%ii%g%i%1%n", 1);
	format_add("contacts_xa_descr_full_blink", "%K%ii%g%i%1%n %2", 1);
	format_add("contacts_xa_footer", "", 1);
	format_add("contacts_notavail_header", "", 1);
	format_add("contacts_notavail", " %r%1%n", 1);
	format_add("contacts_notavail_descr", "%Ki%r%1%n", 1);
	format_add("contacts_notavail_descr_full", "%Ki%r%1%n %2", 1);
	format_add("contacts_notavail_blink", " %r%i%1%n", 1);
	format_add("contacts_notavail_descr_blink", "%K%ii%r%i%1%n", 1);
	format_add("contacts_notavail_descr_full_blink", "%K%ii%r%i%1%n %2", 1);
	format_add("contacts_notavail_footer", "", 1);
	format_add("contacts_invisible_header", "", 1);
	format_add("contacts_invisible", " %c%1%n", 1);
	format_add("contacts_invisible_descr", "%Ki%c%1%n", 1);
	format_add("contacts_invisible_descr_full", "%Ki%c%1%n %2", 1);
	format_add("contacts_invisible_blink", " %c%i%1%n", 1);
	format_add("contacts_invisible_descr_blink", "%K%ii%c%i%1%n", 1);
	format_add("contacts_invisible_descr_full_blink", "%K%ii%c%i%1%n %2", 1);
	format_add("contacts_invisible_footer", "", 1);
	format_add("contacts_blocking_header", "", 1);
	format_add("contacts_blocking", " %m%1%n", 1);
	format_add("contacts_blocking_footer", "", 1);
	format_add("contacts_unknown_header", "", 1);
	format_add("contacts_unknown", " %M%1%n", 1);
	format_add("contacts_unknown_descr", "%Ki%M%1%n", 1);
	format_add("contacts_unknown_descr_full", "%Ki%M%1%n %2", 1);
	format_add("contacts_unknown_blink", " %M%i%1%n", 1);
	format_add("contacts_unknown_descr_blink", "%K%ii%M%i%1%n", 1);
	format_add("contacts_unknown_descr_full_blink", "%K%ii%M%i%1%n %2", 1);
	format_add("contacts_unknown_footer", "", 1);
	format_add("contacts_footer", "", 1);
	format_add("contacts_footer_group", "", 1);
	format_add("contacts_metacontacts_footer", "", 1);
	format_add("contacts_vertical_line_char", "|", 1);
	format_add("contacts_horizontal_line_char", "-", 1);
	format_add("contacts_avail_blink_typing", "%W%i*%Y%i%1%n", 1);
	format_add("contacts_avail_descr_blink_typing", "%W%i*%Y%i%1%n", 1);
	format_add("contacts_avail_descr_full_blink_typing", "%W%i*%Y%i%1%n %2", 1);
	format_add("contacts_away_blink_typing", "%W%i*%G%i%1%n", 1);
	format_add("contacts_away_descr_blink_typing", "%W%i*%G%i%1%n", 1);
	format_add("contacts_away_descr_full_blink_typing", "%W%i*%G%i%1%n %2", 1);
	format_add("contacts_dnd_blink_typing", "%W%i*%B%i%1%n", 1);
	format_add("contacts_dnd_descr_blink_typing", "%W%i*%B%i%1%n", 1);
	format_add("contacts_dnd_descr_full_blink_typing", "%W%i*%B%i%1%n %2", 1);
	format_add("contacts_chat_blink_typing", "%W%i*%W%i%1%n", 1);
	format_add("contacts_chat_descr_blink_typing", "%W%i*%W%i%1%n", 1);
	format_add("contacts_chat_descr_full_blink_typing", "%W%i*%W%i%1%n %2", 1);
	format_add("contacts_error_blink_typing", "%W%i*%m%i%1%n", 1);
	format_add("contacts_error_descr_blink_typing", "%W%i*%m%i%1%n", 1);
	format_add("contacts_error_descr_full_blink_typing", "%W%i*%m%i%1%n %2", 1);
	format_add("contacts_xa_blink_typing", "%W%i*%g%i%1%n", 1);
	format_add("contacts_xa_descr_blink_typing", "%W%i*%g%i%1%n", 1);
	format_add("contacts_xa_descr_full_blink_typing", "%W%i*%g%i%1%n %2", 1);
	format_add("contacts_notavail_blink_typing", "%W%i*%r%i%1%n", 1);
	format_add("contacts_notavail_descr_blink_typing", "%W%i*%r%i%1%n", 1);
	format_add("contacts_notavail_descr_full_blink_typing", "%W%i*%r%i%1%n %2", 1);
	format_add("contacts_invisible_blink_typing", "%W%i*%c%i%1%n", 1);
	format_add("contacts_invisible_descr_blink_typing", "%W%i*%c%i%1%n", 1);
	format_add("contacts_invisible_descr_full_blink_typing", "%W%i*%c%i%1%n %2", 1);
	format_add("contacts_avail_typing", "%W*%Y%1%n", 1);
	format_add("contacts_avail_descr_typing", "%W*%Y%1%n", 1);
	format_add("contacts_avail_descr_full_typing", "%W*%Y%1%n %2", 1);
	format_add("contacts_away_typing", "%W*%G%1%n", 1);
	format_add("contacts_away_descr_typing", "%W*%G%1%n", 1);
	format_add("contacts_away_descr_full_typing", "%W*%G%1%n %2", 1);
	format_add("contacts_dnd_typing", "%W*%B%1%n", 1);
	format_add("contacts_dnd_descr_typing", "%W*%B%1%n", 1);
	format_add("contacts_dnd_descr_full_typing", "%W*%B%1%n %2", 1);
	format_add("contacts_chat_typing", "%W*%W%1%n", 1);
	format_add("contacts_chat_descr_typing", "%W*%W%1%n", 1);
	format_add("contacts_chat_descr_full_typing", "%W*%W%1%n %2", 1);
	format_add("contacts_error_typing", "%W*%m%1%n", 1);
	format_add("contacts_error_descr_typing", "%W*%m%1%n", 1);
	format_add("contacts_error_descr_full_typing", "%W*%m%1%n %2", 1);
	format_add("contacts_xa_typing", "%W*%g%1%n", 1);
	format_add("contacts_xa_descr_typing", "%W*%g%1%n", 1);
	format_add("contacts_xa_descr_full_typing", "%W*%g%1%n %2", 1);
	format_add("contacts_notavail_typing", "%W*%r%1%n", 1);
	format_add("contacts_notavail_descr_typing", "%W*%r%1%n", 1);
	format_add("contacts_notavail_descr_full_typing", "%W*%r%1%n %2", 1);
	format_add("contacts_invisible_typing", "%W*%c%1%n", 1);
	format_add("contacts_invisible_descr_typing", "%W*%c%1%n", 1);
	format_add("contacts_invisible_descr_full_typing", "%W*%c%1%n %2", 1);
	format_add("contacts_unknown_typing", "%W*%M%1%n", 1);
	format_add("contacts_unknown_descr_typing", "%W*%M%1%n", 1);
	format_add("contacts_unknown_descr_full_typing", "%W*%M%1%n %2", 1);
	format_add("quit", _("%> Bye\n"), 1);
	format_add("quit_descr", _("%> Bye: %T%1%n%2\n"), 1);
	format_add("config_changed", _("Save new configuration ? (t-yes/n-no) "), 1);
	format_add("config_must_reconnect", _("%) You must reconnect for the changes to take effect\n"), 2);
	format_add("quit_keep_reason", _("You've set keep_reason to save status.\nDo you want to save current description to file (it will be restored upon next EKG exec)? (t-yes/n-no) "), 1);
	format_add("saved", _("%> Configuration saved\n"), 1);
	format_add("error_saving", _("%! There was some error during save\n"), 1);
	format_add("message", "%g.-- %n%1 %c%2%n%6%n%g--- -- -%n\n%g|%n %|%3%n\n%|%g`----- ---- --- -- -%n\n", 1);
	format_add("message_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("message_timestamp_today", "(%H:%M) ", 1);
	format_add("message_timestamp_now", "", 1);
	format_add("message,speech", _("message from %1: %3."), 1);
	format_add("empty", "%3\n", 1);
	format_add("conference", "%g.-- %n%1 %c%2%n%6%n%g--- -- -%n\n%g|%n %|%3%n\n%|%g`----- ---- --- -- -%n\n", 1);
	format_add("conference_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("conference_timestamp_today", "(%H:%M) ", 1);
	format_add("conference_timestamp_now", "", 1);
	format_add("confrence,speech", _("message from %1: %3."), 1);
	format_add("chat", "%c.-- %n%1 %c%2%n%6%n%c--- -- -%n\n%c|%n %|%3%n\n%|%c`----- ---- --- -- -%n\n", 1);
	format_add("chat_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("chat_timestamp_today", "(%H:%M) ", 1);
	format_add("chat_timestamp_now", "", 1);
	format_add("chat,speech", _("message from %1: %3."), 1);
	format_add("sent", "%b.-- %n%1 %c%2%n%6%n%b--- -- -%n\n%b|%n %|%3%n\n%|%b`----- ---- --- -- -%n\n", 1);
	format_add("sent_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("sent_timestamp_today", "(%H:%M) ", 1);
	format_add("sent_timestamp_now", "", 1);
	format_add("sent,speech", "", 1);
	format_add("system", _("%m.-- %TSystem message%m --- -- -%n\n%m|%n %|%3%n\n%|%m`----- ---- --- -- -%n\n"), 1);
	format_add("system,speech", _("system message: %3."), 1);
	format_add("ack_queued", _("%> Message to %1 will be delivered later\n"), 1);
	format_add("ack_delivered", _("%> Message to %1 delivered\n"), 1);
	format_add("ack_unknown", _("%> Not clear what happened to message to %1\n"), 1);
	format_add("ack_tempfail", _("%! %|Message to %1 encountered temporary delivery failure (e.g. message queue full). Please try again later.\n"), 1);
	format_add("ack_filtered", _("%! %|Message to %1 encountered permament delivery failure (e.g. forbidden content). Before retrying, try to fix the problem yourself (e.g. ask second side to add us to userlist).\n"), 1);
	format_add("message_too_long", _("%! Message was too long and got shortened\n"), 1);
	format_add("status_avail", _("%> (%3) %1 is %Yavailable%n\n"), 1);
	format_add("status_avail_descr", _("%> (%3) %1 is %Yavailable%n: %T%4%n\n"), 1);
	format_add("status_away", _("%> (%3) %1 is %Gaway%n\n"), 1);
	format_add("status_away_descr", _("%> (%3) %1 is %Gaway%n: %T%4%n\n"), 1);
	format_add("status_notavail", _("%> (%3) %1 is %roffline%n\n"), 1);
	format_add("status_notavail_descr", _("%> (%3) %1 is %roffline%n: %T%4%n\n"), 1);
	format_add("status_invisible", _("%> (%3) %1 is %cinvisible%n\n"), 1);
	format_add("status_invisible_descr", _("%> (%3) %1 is %cinvisible%n: %T%4%n\n"), 1);
	format_add("status_xa", _("%> (%3) %1 is %gextended away%n\n"), 1);
	format_add("status_xa_descr", _("%> (%3) %1 is %gextended away%n: %T%4%n\n"), 1);
	format_add("status_dnd", _("%> (%3) %1 %Bdo not disturb%n\n"), 1);
	format_add("status_dnd_descr", _("%> (%3) %1 %Bdo not disturb%n: %T%4%n\n"), 1);
	format_add("status_error", _("%> (%3) %1 %merror fetching status%n\n"), 1);
	format_add("status_error_descr", _("%> (%3) %1 %merror fetching status%n: %T%4%n\n"), 1);
	format_add("status_chat", _("%> (%3) %1 is %Wfree for chat%n\n"), 1);
	format_add("status_chat_descr", _("%> (%3) %1 is %Wfree for chat%n: %T%4%n\n"), 1);
	format_add("connecting", _("%> (%1) Connecting to server %n\n"), 1);
	format_add("conn_failed", _("%! (%2) Connection failure: %1%n\n"), 1);
	format_add("conn_failed_resolving", _("Server not found"), 1);
	format_add("conn_failed_connecting", _("Can't connect to server"), 1);
	format_add("conn_failed_invalid", _("Invalid server response"), 1);
	format_add("conn_failed_disconnected", _("Server disconnected"), 1);
	format_add("conn_failed_password", _("Invalid password"), 1);
	format_add("conn_failed_404", _("HTTP server error"), 1);
	format_add("conn_failed_tls", _("Error negotiating TLS"), 1);
	format_add("conn_failed_memory", _("No memory"), 1);
	format_add("conn_stopped", _("%! (%1) Connection interrupted %n\n"), 1);
	format_add("conn_timeout", _("%! (%1) Connection timed out%n\n"), 1);
	format_add("connected", _("%> (%1) Connected%n\n"), 1);
	format_add("connected_descr", _("%> (%2) Connected: %T%1%n\n"), 1);
	format_add("disconnected", _("%> (%1) Disconnected%n\n"), 1);
	format_add("disconnected_descr", _("%> (%2) Disconnected: %T%1%n\n"), 1);
	format_add("already_connected", _("%! (%1) Already connected. Use %Treconnect%n to reconnect%n\n"), 1);
	format_add("during_connect", _("%! (%1) Connecting in progress. Use %Tdisconnect%n to abort%n\n"), 1);
	format_add("conn_broken", _("%! (%1) Connection broken: %2%n\n"), 1);
	format_add("conn_disconnected", _("%! (%1) Server disconnected%n\n"), 1);
	format_add("not_connected", _("%! (%1) Not connected.%n\n"), 1);
	format_add("not_connected_msg_queued", _("%! (%1) Not connected. Message will be delivered when connected.%n\n"), 1);
	format_add("wrong_id", _("%! (%1) Wrong session id.%n\n"), 1);
	format_add("inet_addr_failed", _("%! (%1) Invalid \"server\".%n\n"), 1);
	format_add("invalid_local_ip", _("%! (%1) Invalid local address. I'm clearing %Tlocal_ip%n session variable\n"), 1);
	format_add("auto_reconnect_removed", _("%! (%1) EKG2 won't try to connect anymore - use /connect.%n\n"), 1);
	format_add("theme_loaded", "%> Loaded theme %T%1%n\n", 1);
	format_add("theme_default", "%> Default theme selected\n", 1);
	format_add("error_loading_theme", "%! Error loading theme: %1\n", 1);
	format_add("variable", "%> %1 = %2\n", 1);
	format_add("variable_not_found", _("%! Unknown variable: %T%1%n\n"), 1);
	format_add("variable_invalid", _("%! Invalid session variable value\n"), 1);
	format_add("no_config", _("%! Incomplete configuration. Use:\n%!   %Tsession -a <gg:gg-number/jid:jabber-id>%n\n%!   %Tsession password <password>%n\n%!   %Tsave%n\n%! And then:\n%!   %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister <e-mail> <password>%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add someone to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Remember about prefixes before UID, for example %Tgg:<no>%n. \n\n"), 2);
	format_add("no_config,speech", _("incomplete configuration. enter session -a, and then gg: gg-number, or jid: jabber id, then session password and your password. enter save to save. enter connect to connect. if you dont have UID enter register, space, e-mail and password. Query windows will be created automatically. To switch windows press Alt and window number or Escape and then number. To start conversation use query command. To add someone to roster use add command. All key shortcuts are described in README file. There is also help command."), 1);
	format_add("no_config_gg_not_loaded", _("%! Incomplete configuration. Use:\n%!   %T/plugin +gg%n - to load gg plugin\n%!   %Tsession -a <gg:gg-number/jid:jabber-id>%n\n%!   %Tsession password <password>%n\n%!   %Tsave%n\n%! And then:\n%!   %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister <e-mail> <password>%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add someone to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Remember about prefixes before UID, for example %Tgg:<no>%n. \n\n"), 2);
	format_add("no_config_no_libgadu", _("%! Incomplete configuration. %TBIG FAT WARNING:%n\n%!    %Tgg plugin has not been compiled, probably there is no libgadu library in the system\n%! Use:\n%!   %Tsession -a <gg:gg-number/jid:jabber-id>%n\n%!   %Tsession password <password>%n\n%!   %Tsave%n\n%! And then:\n%!   %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister <e-mail> <password>%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add someone to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Remember about prefixes before UID, for example %Tgg:<no>%n. \n\n"), 2);
	format_add("error_reading_config", _("%! Error reading configuration file: %1\n"), 1);
	format_add("config_read_success", _("%> Configuratin read correctly.%n\n"), 1);
	format_add("config_line_incorrect", _("%! Invalid line '%T%1%n', skipping\n"), 1);
	format_add("autosaved", _("%> Automatically saved settings\n"), 1);
	format_add("config_upgrade_begin", _("%) EKG2 upgrade detected. In the meantime, following changes were made:\n"), 1);
	format_add("config_upgrade_important",	"%) %W%2) %y*%n %1\n", 1);
	format_add("config_upgrade_major",	"%) %W%2) %Y*%n %1\n", 1);
	format_add("config_upgrade_minor",	"%) %W%2) %c*%n %1\n", 1);
	format_add("config_upgrade_end", _("%) To make configuration upgrade permament, please save your configuration: %c/save%n\n"), 1);
	format_add("register", _("%> Registration successful. Your number: %T%1%n\n"), 1);
	format_add("register_failed", _("%! Error during registration: %1\n"), 1);
	format_add("register_pending", _("%! Registration in progress\n"), 1);
	format_add("register_timeout", _("%! Registration timed out\n"), 1);
	format_add("registered_today", _("%! Already registered. Do not abuse\n"), 1);
	format_add("unregister", _("%> Account removed\n"), 1);
	format_add("unregister_timeout", _("%! Account removal timed out\n"), 1);
	format_add("unregister_bad_uin", _("%! Unknown uin: %T%1%n\n"), 1);
	format_add("unregister_failed", _("%! Error while deleting account: %1\n"), 1);
	format_add("remind", _("%> Password sent\n"), 1);
	format_add("remind_failed", _("%! Error while sending password: %1\n"), 1);
	format_add("remind_timeout", _("%! Password sending timed out\n"), 1);
	format_add("passwd", _("%> Password changed\n"), 1);
	format_add("passwd_failed", _("%! Error while changing password: %1\n"), 1);
	format_add("passwd_timeout", _("%! Password changing timed out\n"), 1);
	format_add("passwd_possible_abuse", "%> (%1) Password reply send by wrong uid: %2, if this is good server uid please report this to developers and manually "
						"change your session password using /session password", 1);
	format_add("passwd_abuse", "%! (%1) Somebody want to clear our password (%2)", 1);
	format_add("change", _("%> Informations in public directory chenged\n"), 1);
	format_add("change_failed", _("%! Error while changing informations in public directory\n"), 1);
	format_add("search_failed", _("%! Error while search: %1\n"), 1);
	format_add("search_not_found", _("%! Not found\n"), 1);
	format_add("search_no_last", _("%! Last search returned no result\n"), 1);
	format_add("search_no_last_nickname", _("%! No nickname in last search\n"), 1);
	format_add("search_stopped", _("%> Search stopped\n"), 1);
	format_add("search_results_multi_avail", "%Y<>%n", 1);
	format_add("search_results_multi_away", "%G<>%n", 1);
	format_add("search_results_multi_invisible", "%c<>%n", 1);
	format_add("search_results_multi_notavail", "  ", 1);
	format_add("search_results_multi_unknown", "-", 1);
/*      format_add("search_results_multi_female", "k", 1); */
/*      format_add("search_results_multi_male", "m", 1); */
	format_add("search_results_multi", "%7 %[-7]1 %K|%n %[12]3 %K|%n %[12]2 %K|%n %[4]5 %K|%n %[12]4\n", 1);

	format_add("search_results_single_avail", _("%Y(available)%N"), 1);
	format_add("search_results_single_away", _("%G(away)%n"), 1);
	format_add("search_results_single_notavail", _("%r(offline)%n"), 1);
	format_add("search_results_single_invisible", _("%c(invisible)%n"), 1);
	format_add("search_results_single_unknown", "%T-%n", 1);
	format_add("search_results_single", _("%) Nickname:  %T%3%n\n%) Number: %T%1%n %7\n%) Name: %T%2%n\n%) City: %T%4%n\n%) Birth year: %T%5%n\n"), 1);
	format_add("process", "%> %(-5)1 %2\n", 1);
	format_add("no_processes", _("%! There are no running procesees\n"), 1);
	format_add("process_exit", _("%> Proces %1 (%2) exited with %3 status\n"), 1);
	format_add("exec", "%1\n",1);   /* lines are ended by \n */
	format_add("exec_error", _("%! Error running process : %1\n"), 1);
	format_add("exec_prompt", "$ %1\n", 1);
	format_add("user_info_header", "%K.--%n %T%1%n/%2 %K--- -- -%n\n", 1);
	format_add("user_info_nickname", _("%K| %nNickname: %T%1%n\n"), 1);
	format_add("user_info_status", _("%K| %nStatus: %T%1%n\n"), 1);
	format_add("user_info_status_time_format", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_status_time", _("%K| %nCurrent status since: %T%1%n\n"), 1);
	format_add("user_info_block", _("%K| %nBlocked\n"), 1);
	format_add("user_info_offline", _("%K| %nCan't see our status\n"), 1);
	format_add("user_info_groups", _("%K| %nGroups: %T%1%n\n"), 1);
	format_add("user_info_never_seen", _("%K| %nNever seen\n"), 1);
	format_add("user_info_last_seen", _("%K| %nLast seen: %T%1%n\n"), 1);
	format_add("user_info_last_seen_time", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_last_status", _("%K| %nLast status: %T%1%n\n"), 1);
	format_add("user_info_footer", "%K`----- ---- --- -- -%n\n", 1);
	format_add("user_info_avail", _("%Yavailable%n"), 1);
	format_add("user_info_avail_descr", _("%Yavailable%n %K(%n%2%K)%n"), 1);
	format_add("user_info_away", _("%Gaway%n"), 1);
	format_add("user_info_away_descr", _("%Gaway%n %K(%n%2%K)%n"), 1);
	format_add("user_info_notavail", _("%roffline%n"), 1);
	format_add("user_info_notavail_descr", _("%roffline%n %K(%n%2%K)%n"), 1);
	format_add("user_info_invisible", _("%cinvisible%n"), 1);
	format_add("user_info_invisible_descr", _("%cinvisible%n %K(%n%2%K)%n"), 1);
	format_add("user_info_dnd", _("%Bdo not disturb%n"), 1);
	format_add("user_info_dnd_descr", _("%Bdo not disturb%n %K(%n%2%K)%n"), 1);
	format_add("resource_info_status", _("%K| %nResource: %W%1%n Status: %T%2 Prio: %g%3%n"), 1);
	format_add("group_members", _("%> %|Group %T%1%n: %2\n"), 1);
	format_add("group_member_already", _("%! %1 already in group %T%2%n\n"), 1);
	format_add("group_member_not_yet", _("%! %1 not in group %T%2%n\n"), 1);
	format_add("group_empty", _("%! Group %T%1%n is empty\n"), 1);
	format_add("show_status_profile", _("%) Profile: %T%1%n\n"), 1);
	format_add("show_status_uid", "%) UID: %T%1%n\n", 1);
	format_add("show_status_uid_nick", "%) UID: %T%1%n (%T%2%n)\n", 1);
	format_add("show_status_status", _("%) Current status: %T%1%2%n\n"), 1);
	format_add("show_status_status_simple", _("%) Current status: %T%1%n\n"), 1);
	format_add("show_status_server", _("%) Current server: %T%1%n:%T%2%n\n"), 1);
	format_add("show_status_server_tls", _("%) Current server: %T%1%n:%T%2%Y (connection encrypted)%n\n"), 1);
	format_add("show_status_connecting", _("%) Connecting ..."), 1);
	format_add("show_status_avail", _("%Yavailable%n"), 1);
	format_add("show_status_avail_descr", _("%Yavailable%n (%T%1%n%2)"), 1);
	format_add("show_status_away", _("%Gaway%n"), 1);
	format_add("show_status_away_descr", _("%Gaway%n (%T%1%n%2)"), 1);
	format_add("show_status_invisible", _("%cinvisible%n"), 1);
	format_add("show_status_invisible_descr", _("%cinvisible%n (%T%1%n%2)"), 1);
	format_add("show_status_xa", _("%gextended away%n"), 1);
	format_add("show_status_xa_descr", _("%gextended away%n (%T%1%n%2)"), 1);
	format_add("show_status_dnd", _("%cdo not disturb%n"), 1);
	format_add("show_status_dnd_descr", _("%cdo not disturb%n (%T%1%n%2)"), 1);
	format_add("show_status_chat", _("%Wfree for chat%n"), 1);
	format_add("show_status_chat_descr", _("%Wfree for chat%n (%T%1%n%2)"), 1);
	format_add("show_status_notavail", _("%roffline%n"), 1);
	format_add("show_status_private_on", _(", for friends only"), 1);
	format_add("show_status_private_off", "", 1);
	format_add("show_status_connected_since", _("%) Connected since: %T%1%n\n"), 1);
	format_add("show_status_disconnected_since", _("%) Disconnected since: %T%1%n\n"), 1);
	format_add("show_status_last_conn_event", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_last_conn_event_today", "%H:%M", 1);
	format_add("show_status_ekg_started_since", _("%) Program started: %T%1%n\n"), 1);
	format_add("show_status_ekg_started", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_ekg_started_today", "%H:%M", 1);
	format_add("show_status_msg_queue", _("%) Messages queued for delivery: %T%1%n\n"), 1);
	format_add("aliases_list_empty", _("%! No aliases\n"), 1);
	format_add("aliases_list", "%> %T%1%n: %2\n", 1);
	format_add("aliases_list_next", "%> %3  %2\n", 1);
	format_add("aliases_add", _("%> Created alias %T%1%n\n"), 1);
	format_add("aliases_append", _("%> Added to alias %T%1%n\n"), 1);
	format_add("aliases_del", _("%) Removed alias %T%1%n\n"), 1);
	format_add("aliases_del_all", _("%) Removed all aliases\n"), 1);
	format_add("aliases_exist", _("%! Alias %T%1%n already exists\n"), 1);
	format_add("aliases_noexist", _("%! Alias %T%1%n doesn't exist\n"), 1);
	format_add("aliases_command", _("%! %T%1%n is internal command\n"), 1);
	format_add("aliases_not_enough_params", _("%! Alias %T%1%n requires more parameters\n"), 1);
	format_add("dcc_attack", _("%! To many direct connections, last from %1\n"), 1);
	format_add("dcc_limit", _("%! %|Direct connections count over limit, so they got disabled. To enable them use %Tset dcc 1% and reconnect. Limit is controlled by %Tdcc_limit%n variable.\n"), 1);
	format_add("dcc_create_error", _("%! Can't turn on direct connections: %1\n"), 1);
	format_add("dcc_error_network", _("%! Error transmitting with %1\n"), 1);
	format_add("dcc_error_refused", _("%! Connection to %1 refused\n"), 1);
	format_add("dcc_error_unknown", _("%! Uknown direct connection error\n"), 1);
	format_add("dcc_error_handshake", _("%! Can't connect with %1\n"), 1);
	format_add("dcc_user_aint_dcc", _("%! %1 doesn't have direct connections enabled\n"), 1);
	format_add("dcc_timeout", _("%! Direct connection to %1 timed out\n"), 1);
	format_add("dcc_not_supported", _("%! Operation %T%1%n isn't supported yet\n"), 1);
	format_add("dcc_open_error", _("%! Can't open %T%1%n: %2\n"), 1);
	format_add("dcc_show_pending_header", _("%> Pending connections:\n"), 1);
	format_add("dcc_show_pending_send", _("%) #%1, %2, sending %T%3%n\n"), 1);
	format_add("dcc_show_pending_get", _("%) #%1, %2, receiving %T%3%n\n"), 1);
	format_add("dcc_show_pending_voice", _("%) #%1, %2, chat\n"), 1);
	format_add("dcc_show_active_header", _("%> Active connections:\n"), 1);
	format_add("dcc_show_active_send", _("%) #%1, %2, sending %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n"), 1);
	format_add("dcc_show_active_get", _("%) #%1, %2, receiving %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n"), 1);
	format_add("dcc_show_active_voice", _("%) #%1, %2, chat\n"), 1);
	format_add("dcc_show_empty", _("%! No direct connections\n"), 1);
	format_add("dcc_receiving_already", _("%! File %T%1%n from %2 is being received\n"), 1);
	format_add("dcc_done_get", _("%> Finished receiving file %T%2%n from %1\n"), 1);
	format_add("dcc_done_send", _("%> Finished sending file %T%2%n to %1\n"), 1);
	format_add("dcc_close", _("%) Connection with %1 closed\n"), 1);
	format_add("dcc_voice_offer", _("%) %1 wants to chat\n%) Use  %Tdcc voice #%2%n to start chat or %Tdcc close #%2%n to refuse\n"), 1);
	format_add("dcc_voice_running", _("%! Only one simultanous voice chat possible\n"), 1);
	format_add("dcc_voice_unsupported", _("%! Voice chat not compiled in. See %Tdocs/voip.txt%n\n"), 1);
	format_add("dcc_get_offer", _("%) %1 sends %T%2%n (size %T%3b%n)\n%) Use %Tdcc get #%4%n to receive or %Tdcc close #%4%n to refuse\n"), 1);
	format_add("dcc_get_offer_resume", _("%) File exist, you can resume with %Tdcc resume #%4%n\n"), 1);
	format_add("dcc_get_getting", _("%) Started receiving %T%2%n from %1\n"), 1);
	format_add("dcc_get_cant_create", _("%! Can't open file %T%1%n\n"), 1);
	format_add("dcc_not_found", _("%! Connection not found: %T%1%n\n"), 1);
	format_add("dcc_invalid_ip", _("%! Invalid IP address\n"), 1);
	format_add("dcc_user_notavail", _("%! %1 has to available to connect\n"), 1);
	format_add("query_started", _("%) (%2) Query with %T%1%n started\n"), 1);
	format_add("query_started_window", _("%) Press %TAlt-G%n to ignore, %TAlt-K%n to close window\n"), 1);
	format_add("query_finished", _("%) (%2) Finished query with %T%1%n\n"), 1);
	format_add("query_exist", _("%! (%3) Query with %T%1%n already in window no %T%2%n\n"), 1);
	format_add("events_list_empty", _("%! No events\n"), 1);
	format_add("events_list_header", "", 1);
	format_add("events_list", "%> %5 on %1 %3 %4 - prio %2\n", 1);
	format_add("events_add", _("%> Added event %T%1%n\n"), 1);
	format_add("events_del", _("%) Removed event %T%1%n\n"), 1);
	format_add("events_del_all", _("%) Removed all events\n"), 1);
	format_add("events_exist", _("%! Event %T%1%n exist for %2\n"), 1);
	format_add("events_del_noexist", _("%! Event %T%1%n do not exist\n"), 1);
	format_add("userlist_put_ok", _("%> Roster saved on server\n"), 1);
	format_add("userlist_put_error", _("%! Error sending roster\n"), 1);
	format_add("userlist_get_ok", _("%> Roster read from server\n"), 1);
	format_add("userlist_get_error", _("%! Error getting roster\n"), 1);
	format_add("userlist_clear_ok", _("%) Removed roster from server\n"), 1);
	format_add("userlist_clear_error", _("%! Error removing roster from server\n"), 1);
	format_add("quick_list", "%)%1\n", 1);
	format_add("quick_list,speech", _("roster:"), 1);
	format_add("quick_list_avail", " %Y%1%n", 1);
	format_add("quick_list_avail,speech", _("%1 is available"), 1);
	format_add("quick_list_away", " %G%1%n", 1);
	format_add("quick_list_away,speech", _("%1 is away"), 1);
	format_add("quick_list_invisible", " %c%1%n", 1);
	format_add("window_add", _("%) New window created\n"), 1);
	format_add("window_noexist", _("%! Choosen window do not exist\n"), 1);
	format_add("window_doesnt_exist", _("%! Window %T%1%n does not exist\n"), 1);
	format_add("window_no_windows", _("%! Can't close last window\n"), 1);
	format_add("window_del", _("%) Window closed\n"), 1);
	format_add("windows_max", _("%! Window limit exhausted\n"), 1);
	format_add("window_list_query", _("%) %1: query with %T%2%n\n"), 1);
	format_add("window_list_nothing", _("%) %1 no query\n"), 1);
	format_add("window_list_floating", _("%) %1: floating %4x%5 in %2,%3 %T%6%n\n"), 1);
	format_add("window_id_query_started", _("%) (%3) Query with %T%2%n started in %T%1%n\n"), 1);
	format_add("window_kill_status", _("%! Can't close status window!\n"), 1);
	format_add("window_cannot_move_status", _("%! Can't move status window!\n"), 1);
	format_add("window_invalid_move", _("%! Window %T%1%n can't be moved\n"), 1);
	format_add("cant_kill_irc_window", _("Can't kill window. Use /window kill"), 1);
	format_add("file_doesnt_exist", _("%! Can't open file %T%1%n\n"), 1);
	format_add("bind_seq_incorrect", _("%! Sequence %T%1%n is invalid\n"), 1);
	format_add("bind_seq_add", _("%> Sequence %T%1%n added\n"), 1);
	format_add("bind_seq_remove", _("%) Sequence %T%1%n removed\n"), 1);
	format_add("bind_seq_list", "%> %1: %T%2%n\n", 1);
	format_add("bind_seq_exist", _("%! Sequence %T%1%n is already bound\n"), 1);
	format_add("bind_seq_list_empty", _("%! No bound actions\n"), 1);
	format_add("bind_doesnt_exist", _("%! Can't find sequence %T%1%n\n"), 1);
	format_add("bind_press_key", _("%! Press key(s) which should be bound\n"), 1);
	format_add("bind_added", _("%> Binding added\n"), 1);
	format_add("at_list", "%> %1, %2, %3 %K(%4)%n %5\n", 1);
	format_add("at_added", _("%> Created plan %T%1%n\n"), 1);
	format_add("at_deleted", _("%) Removed plan %T%1%n\n"), 1);
	format_add("at_deleted_all", _("%) Removed user's plans\n"), 1);
	format_add("at_exist", _("%! Plan %T%1%n already exists\n"), 1);
	format_add("at_noexist", _("%! Plan %T%1%n do not exists\n"), 1);
	format_add("at_empty", _("%! No plans\n"), 1);
	format_add("at_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("at_back_to_past", _("%! If time travels were possible...\n"), 1);
	format_add("timer_list", "%> %1, %2s, %3 %K(%4)%n %T%5%n\n", 1);
	format_add("timer_added", _("%> Created timer %T%1%n\n"), 1);
	format_add("timer_deleted", _("%) Removed timer  %T%1%n\n"), 1);
	format_add("timer_deleted_all", _("%) Removed user's timers\n"), 1);
	format_add("timer_exist", _("%! Timer %T%1%n already exists\n"), 1);
	format_add("timer_noexist", _("%! Timer %T%1%n does not exists\n"), 1);
	format_add("timer_empty", _("%! No timers\n"), 1);
	format_add("last_list_in", "%) %Y <<%n [%1] %2 %3\n", 1);
	format_add("last_list_out", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("last_list_empty", _("%! No messages logged\n"), 1);
	format_add("last_list_empty_nick", _("%! No messages from %T%1%n logged\n"), 1);
	format_add("last_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("last_list_timestamp_today", "%H:%M", 1);
	format_add("last_clear_uin", _("%) Messages from %T%1%n cleared\n"), 1);
	format_add("last_clear", _("%) All messages cleared\n"), 1);
	format_add("last_begin_uin", _("%) Lastlog from %T%1%n begins\n"), 1);
	format_add("last_begin", _("%) Lastlog begin\n"), 1);
	format_add("last_end", _("%) Lastlog end\n"), 1);
	format_add("lastlog_title", 	_("%) %gLastlog [%B%2%n%g] from window: %W%T%1%n"), 1);
	format_add("lastlog_title_cur", _("%) %gLastlog [%B%2%n%g] from window: %W%T%1 (*)%n"), 1);
	format_add("away_log_begin", _("%) Logged messages for session %1:\n"), 1);
	format_add("away_log_end", _("%) Away log end\n"), 1);
	format_add("away_log_msg", "%) [%Y%1%n] [%G%2%n] <%W%3%n> %4\n", 1);
	format_add("away_log_timestamp", "%d-%m-%Y %H:%M:%S", 1);
	format_add("queue_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("queue_list_message", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("queue_clear", _("%) Message queue cleared\n"), 1);
	format_add("queue_clear_uid", _("%) Message queue for %T%1%n cleared\n"), 1);
	format_add("queue_wrong_use", _("%! Command works only when disconected\n"), 1);
	format_add("queue_empty", _("%! Messaged queue is empty\n"), 1);
	format_add("queue_empty_uid", _("%! No messages to %T%1%n in queue\n"), 1);
	format_add("queue_flush", _("%> (%1) Sent messages from queue\n"), 1);
	format_add("conferences_list_empty", _("%! No conference\n"), 1);
	format_add("conferences_list", "%> %T%1%n: %2\n", 1);
	format_add("conferences_list_ignored", _("%> %T%1%n: %2 (%yignored%n)\n"), 1);
	format_add("conferences_add", _("%> Created conference %T%1%n\n"), 1);
	format_add("conferences_not_added", _("%! Conference not created %T%1%n\n"), 1);
	format_add("conferences_del", _("%) Removed conference %T%1%n\n"), 1);
	format_add("conferences_del_all", _("%) Removed all conferences\n"), 1);
	format_add("conferences_exist", _("%! Conference %T%1%n already exists\n"), 1);
	format_add("conferences_noexist", _("%! Conference %T%1%n do not exists\n"), 1);
	format_add("conferences_name_error", _("%! Conference name should start with %T#%n\n"), 1);
	format_add("conferences_rename", _("%> Conference renamed: %T%1%n --> %T%2%n\n"), 1);
	format_add("conferences_ignore", _("%> Konference %T%1%n will be ignored\n"), 1);
	format_add("conferences_unignore", _("%> Conference %T%1%n won't be ignored\n"), 1);
	format_add("conferences_joined", _("%> Joined %1 to conference %T%2%n\n"), 1);
	format_add("conferences_already_joined", _("%> %1 already in conference %T%2%n\n"), 1);
	format_add("http_failed_resolving", _("Server not found"), 1);
	format_add("http_failed_connecting", _("Can not connect ro server"), 1);
	format_add("http_failed_reading", _("Server disconnected"), 1);
	format_add("http_failed_writing", _("Server disconnected"), 1);
	format_add("http_failed_memory", _("No memory"), 1);
	format_add("session_name", "%B%1%n", 1);
	format_add("session_variable", "%> %T%1->%2 = %R%3%n\n", 1); /* uid, var, new_value*/
	format_add("session_variable_removed", _("%> Removed  %T%1->%2%n\n"), 1); /* uid, var */
	format_add("session_variable_doesnt_exist", _("%! Unknown variable: %T%1->%2%n\n"), 1); /* uid, var */
	format_add("session_list", "%> %T%1%n %3\n", 1); /* uid, uid, %{user_info_*} */
	format_add("session_list_alias", "%> %T%2%n/%1 %3\n", 1); /* uid, alias, %{user_info_*} */
	format_add("session_list_empty", _("%! Session list is empty\n"), 1);
	format_add("session_info_header", "%) %T%1%n %3\n", 1); /* uid, uid, %{user_info_*} */
	format_add("session_info_header_alias", "%) %T%2%n/%1 %3\n", 1); /* uid, alias, %{user_info_*} */
	format_add("session_info_param", "%)    %1 = %T%2%n\n", 1); /* key, value */
	format_add("session_info_footer", "", 1); /* uid */
	format_add("session_exists", _("%! Session %T%1%n already exists\n"), 1); /* uid */
	format_add("session_doesnt_exist", _("%! Sesion %T%1%n does not exist\n"), 1); /* uid */
	format_add("session_added", _("%> Created session %T%1%n\n"), 1); /* uid */
	format_add("session_removed", _("%> Removed session %T%1%n\n"), 1); /* uid */
	format_add("session_format", "%T%1%n", 1);
	format_add("session_format_alias", "%T%1%n/%2", 1);
	format_add("session_cannot_change", _("%! Can't change session in query window%n\n"), 1);
	format_add("session_password_changed", _("%> %|(%1) Looks like you're changing password in connected session. This does only set password on the client-side. If you want you change your account password, please use dedicated function (e.g. /passwd)."), 1);
	format_add("session_locked", _("%! %|Session %T%1%n is currently locked. If there aren't any other copy of EKG2 using it, please call: %c/session --unlock%n to unlock it.\n"), 1);
	format_add("session_not_locked", _("%! Session %T%1%n is not locked"), 1);
	format_add("metacontact_list", "%> %T%1%n", 1);
	format_add("metacontact_list_empty", "%! Metacontact list is empty\n", 1);
	format_add("metacontact_exists", "%! Metacontact %T%1%n already exists\n", 1);
	format_add("metacontact_added", "%> Metacontact %T%1%n added\n", 1);
	format_add("metacontact_removed", "%> Metacontact %T%1%n removed\n", 1);
	format_add("metacontact_doesnt_exist", "%! Metacontact %T%1%n doesn't exist\n", 1);
	format_add("metacontact_added_item", "%> Added %T%1/%2%n to metacontact %T%3%n\n", 1);
	format_add("metacontact_removed_item", "%> Removed %T%1/%2%n from metacontact %T%3%n\n", 1);
	format_add("metacontact_item_list_header", "", 1);
	format_add("metacontact_item_list", "%> %T%1/%2 (%3)%n - prio %T%4%n\n", 1);
	format_add("metacontact_item_list_empty", "%! Metacontact is empty\n", 1);
	format_add("metacontact_item_list_footer", "", 1);
	format_add("metacontact_item_doesnt_exist", "%! Contact %T%1/%2%n doesn't exiet\n", 1);
	format_add("metacontact_info_header", "%K.--%n Metacontact %T%1%n %K--- -- -%n\n", 1);
	format_add("metacontact_info_status", "%K| %nStatus: %T%1%n\n", 1);
	format_add("metacontact_info_footer", "%K`----- ---- --- -- -%n\n", 1);
	format_add("metacontact_info_avail", _("%Yavailable%n"), 1);
	format_add("metacontact_info_avail_descr", _("%Yavailable%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_away", _("%Gaway%n"), 1);
	format_add("metacontact_info_away_descr", _("%Gaway%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_notavail", _("%roffline%n"), 1);
	format_add("metacontact_info_notavail_descr", _("%roffline%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_invisible", _("%cinvisible%n"), 1);
	format_add("metacontact_info_invisible_descr", _("%cinvisible%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_dnd", _("%Bdo not disturb%n"), 1);
	format_add("metacontact_info_dnd_descr", _("%Bdo not disturb%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_chat", _("%Wfree for chat%n"), 1);
	format_add("metacontact_info_chat_descr", _("%Wfree for chat%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_error", _("%merror%n"), 1);
	format_add("metacontact_info_error_descr", _("%merror%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_xa", _("%gextended away%n"), 1);
	format_add("metacontact_info_xa_descr", _("%gextended away%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_blocking", _("%mblocking%n"), 1);
	format_add("metacontact_info_blocking_descr", _("%mblocking%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_unknown", _("%Munknown%n"), 1);
	format_add("plugin_already_loaded", _("%! Plugin %T%1%n already loaded%n.\n"), 1);
	format_add("plugin_doesnt_exist", _("%! Plugin %T%1%n can not be found%n\n"), 1);
	format_add("plugin_incorrect", _("%! Plugin %T%1%n is not correct EKG2 plugin%n\n"), 1);
	format_add("plugin_not_initialized", _("%! Plugin %T%1%n not initialized correctly, check debug window%n\n"), 1);
	format_add("plugin_unload_ui", _("%! Plugin %T%1%n is an UI plugin and can't be unloaded%n\n"), 1);
	format_add("plugin_loaded", _("%> Plugin %T%1%n loaded%n\n"), 1);
	format_add("plugin_unloaded", _("%> Plugin %T%1%n unloaded%n\n"), 1);
	format_add("plugin_list", _("%> %T%1%n - %2%n\n"), 1);
	format_add("plugin_prio_set", _("%> Plugin %T%1%n prio has been changed to %T%2%n\n"), 1);
	format_add("plugin_default", _("%> Plugins prio setted to default\n"), 1);
	format_add("script_autorun_succ",	_("%> Script %W%1%n successful %G%2%n autorun dir"), 1);		/* XXX sciezka by sie przydala */
	format_add("script_autorun_fail", 	_("%! Script %W%1%n failed %R%2%n autorun dir %r(%3)"), 1);
	format_add("script_autorun_unkn", 	_("%! Error adding/removing script %W%1%n from autorundir %r(%3)"), 1);
	format_add("script_loaded",		_("%) Script %W%1%n %g(%2)%n %Gloaded %b(%3)"), 1);
	format_add("script_incorrect",		_("%! Script %W%1%n %g(%2)%n %rNOT LOADED%n %R[incorrect %3 script or you've got syntax errors]"), 1);
	format_add("script_incorrect2",		_("%! Script %W%1%n %g(%2)%n %rNOT LOADED%n %R[script has no handler or error in getting handlers]"), 1);
	format_add("script_removed",		_("%) Script %W%1%n %g(%2)%n %Rremoved %b(%3)"), 1);
	format_add("script_need_name",		_("%! No filename given\n"), 1);
	format_add("script_not_found",		_("%! Can't find script %W%1"), 1);
	format_add("script_wrong_location",	_("%! Script have to be in %g%1%n (don't add path)"), 1);
	format_add("script_error", 		_("%! %rScript error: %|%1"), 1);
	format_add("script_autorun_list", "%) Script %1 -> %2\n", 1);
	format_add("script_eval_error", _("%! Error running code\n"), 1);
	format_add("script_list", _("%> %1 (%2, %3)\n"), 1);
	format_add("script_list_empty", _("%! No scripts loaded\n"), 1);
	format_add("script_generic", "%> [script,%2] (%1) %3\n", 1);
	format_add("script_varlist", _("%> %1 = %2 (%3)\n"), 1);
	format_add("script_varlist_empty", _("%! No script vars!\n"), 1);
	format_add("directory_cant_create", 	_("%! Can't create directory: %1 (%2)"), 1);
	format_add("console_charset_using",	_("%) EKG2 detected that your console works under: %W%1%n Please verify and eventually change %Gconsole_charset%n variable"), 1);
	format_add("console_charset_bad",	_("%! EKG2 detected that your console works under: %W%1%n, but in %Gconsole_charset%n variable you've got: %W%2%n Please verify."), 1);
	format_add("iconv_fail",		_("%! iconv_open() fail to initialize charset conversion between %W%1%n and %W%2%n. Check %Gconsole_charset%n variable, if it won't help inform ekg2 dev team and/or upgrade iconv"), 1);
	format_add("aspell_init", "%> Czekaj, inicjuję moduł sprawdzania pisowni...\n", 1);
	format_add("aspell_init_success", "%> Zainicjowano moduł sprawdzania pisowni\n", 1);
	format_add("aspell_init_error", "%! Błąd modułu sprawdzania pisowni: %T%1%n\n", 1);
	format_add("io_cantopen", _("%! Unable to open file!"), 1);
	format_add("io_nonfile", _("%! Given path doesn't appear to be regular file!"), 1);
	format_add("io_cantread", _("%! Unable to read file!"), 1);
	format_add("io_truncated", _("%! %|WARNING: Filesize smaller than before. File probably truncated!"), 1);
	format_add("io_truncated", _("%! %|WARNING: EOF before reaching filesize. File probably truncated (somehow)!"), 1);
	format_add("io_expanded", _("%! %|WARNING: Filesize larger than before. File probably got expanded!"), 1);
	format_add("io_emptyfile", _("%! File is empty!"), 1);
	format_add("io_toobig", _("%! File size exceeds maximum allowed length!"), 1);
	format_add("io_binaryfile", _("%! %|WARNING: The file probably contains NULs (is binary), so it can't be properly handled. It will be read until first encountered NUL, i.e. to offset %g%1%n (in bytes)!"), 1);
	format_add("feed_status",		_("%> Newstatus: %1 (%2) %3"), 1);	/* XXX */
	format_add("feed_added", 		_("%> (%2) Added %T%1%n to subscription\n"), 1);
	format_add("feed_exists_other", 	_("%! (%3) %T%1%n already subscribed as %2\n"), 1);
	format_add("feed_not_found",		_("%) Subscription %1 not found, cannot unsubscribe"), 1);
	format_add("feed_deleted", 		_("%) (%2) Removed from subscription %T%1%n\n"), 1);
	format_add("feed_message_header",	_("%g,+=%G-----%W  %1 %n(ID: %W%2%n)"), 1);
	format_add("feed_message_body",		_("%g||%n %|%1"), 1);
	format_add("feed_message_footer",	_("%g|+=%G----- End of message...%n\n"), 1);
	format_add("feed_message_header_generic",	_("%r %1 %W%2"), 1);
	format_add("feed_message_header_pubDate:",	_("%r Napisano: %W%2"), 1);
	format_add("feed_message_header_author:",	_("%r Autor: %W%2"), 1);
	format_add("feed_message_header_dc:date:",	_("%r Napisano: %W%2"), 1);
	format_add("feed_message_header_dc:creator:",	_("%r Autor: %W%2"), 1);
	format_add("feed_server_header_generic",	_("%m %1 %W%2"), 1);
	format_add("nntp_command_help_header",	_("%g,+=%G----- %2 %n(%T%1%n)"), 1);
	format_add("nntp_command_help_item",	_("%g|| %W%1: %n%2"), 1);
	format_add("nntp_command_help_footer",	_("%g`+=%G----- End of 100%n\n"), 1);
	format_add("nntp_message_quote_level1",	"%g%1", 1);
	format_add("nntp_message_quote_level2", "%y%1", 1);
	format_add("nntp_message_quote_level",	"%B%1", 1);	/* upper levels.. */
	format_add("nntp_message_signature",	"%B%1", 1);
	format_add("nntp_posting_failed",	_("(%1) Posting to group: %2 failed: %3 (post saved in: %4)"), 1);
	format_add("nntp_posting",		_("(%1) Posting to group: %2 Subject: %3...."), 1);
	format_add("user_info_name", _("%K| %nName: %T%1 %2%n\n"), 1);
	format_add("gg_token", _("%> Token was written to the file %T%1%n\n"), 1);
	format_add("gg_token_ocr", _("%> Token: %T%1%n\n"), 1);
	format_add("gg_token_body", "%1\n", 1);
	format_add("gg_token_failed", _("%! Error getting token: %1\n"), 1);
	format_add("gg_token_failed_saved", _("%! Error reading token: %1 (saved@%2)\n"), 1);
	format_add("gg_token_timeout", _("%! Token getting timeout\n"), 1);
	format_add("gg_token_unsupported", _("%! Your operating system doesn't support tokens\n"), 1);
	format_add("gg_token_missing", _("%! First get token by function %Ttoken%n\n"), 1);
	format_add("gg_user_is_connected", _("%> (%1) User %T%2%n is connected\n"), 1);
	format_add("gg_user_is_not_connected", _("%> (%1) User %T%2%n is not connected\n"), 1);
	format_add("gg_image_cant_open_file", _("%! Can't open file for image %1 (%2)\n"), 1);
	format_add("gg_image_error_send", _("%! Error sending image\n"), 1);
	format_add("gg_image_ok_send", _("%> Image sent properly\n"), 1);
	format_add("gg_image_ok_get", _("%> Image <%3> saved in %1\n"), 1);	/* %1 - path, %2 - uid, %3 - name of picture */
	format_add("gg_we_are_being_checked", _("%> (%1) We are being checked by %T%2%n\n"), 1);
	format_add("gg_version", _("%> %TGadu-Gadu%n: libgadu %g%1%n (headers %c%2%n), protocol %g%3%n (%c0x%4%n)"), 1);
	format_add("gpg_key_unset",	_("%) GPGKEY for uid: %W%1%n UNSET!"), 1);
	format_add("gpg_key_not_found", _("%> GPGKEY for uid: %W%1%n NOT FOUND!"), 1);
	format_add("gpg_key_set_new",	_("%) You've set up new key for uid: %W%1%n keyid: %W%2%n\n"
					"%) Encryption will be disabled until you force key (gpg:key --forcekey) NOT RECOMENDED or we verify key (signed presence is enough)"), 1);
	format_add("gpg_key_set_newf",	_("%) You've forced setting new key for uid: %W%1%n keyid: %W%2%n\n"
					"%! Forcing key is not good idea... Please rather use /gpg:key --setkey coz key will be verified before encryption..."), 1);
	format_add("gpg_key_set_ok",		_("%> Keys you've set up for uid: %W%1%n match with our internal DB. Happy encrypted talk. F**k echelon"), 1);
	format_add("gpg_key_set_okf",		_("%> Keys you've set up for uid: %W%1%n match with our internal DB. Happy encrypted talk. F**k echelon (Forcing key is not nessesary here!)"), 1);
	format_add("gpg_key_set_okbutver",	_("%! Keys matched, but lasttime we fail to verify key. Encryption won't work until forced."), 1);
	format_add("gpg_key_set_okfbutver",	_("%! Keys matched, but lasttime we fail to verify key. Encryption forced."), 1);
	format_add("gpg_key_set_okbutmish",	_("%! Keys mishmash. Encryption won't work until forced or user change his keyid."), 1);
	format_add("gpg_key_set_okfbutmish",	_("%! Keys mishmash. Encryption forced."), 1);
	format_add("gpg_key_set_okbutunk",	_("%! We didn't verify this key, if you're sure it's ok force key (gpg:key --forcekey) however it's NOT RECOMENDED.. or wait until we verify key"), 1);
	format_add("gpg_key_set_okfbutunk",	_("%! We didn't verify this key, You've forced encryption. NOT RECOMENDED."), 1);
	format_add("gpg_keys_list",		"%> %W%1%n/%W%2%n %3", 1);		/* uid, keyid, key status */
	format_add("user_info_gpg_key", 	_("%K| %nGPGKEY: %T%1%n (%2)%n"), 1);	/* keyid, key status */
	format_add("irc_msg_sent",	"%P<%n%3/%5%P>%n %6", 1);
	format_add("irc_msg_sent_n",	"%P<%n%3%P>%n %6", 1);
	format_add("irc_msg_sent_chan",	"%P<%w%{2@%+gcp}X%2%3%P>%n %6", 1);
	format_add("irc_msg_sent_chanh","%P<%W%{2@%+GCP}X%2%3%P>%n %6", 1);
	format_add("irc_not_sent",	"%P(%n%3/%5%P)%n %6", 1);
	format_add("irc_not_sent_n",	"%P(%n%3%P)%n %6", 1);
	format_add("irc_not_sent_chan",	"%P(%w%{2@%+gcp}X%2%3%P)%n %6", 1);
	format_add("irc_not_sent_chanh","%P(%W%{2@%+GCP}X%2%3%P)%n %6", 1);
//	format_add("irc_msg_f_chan",	"%B<%w%{2@%+gcp}X%2%3/%5%B>%n %6", 1); /* NOT USED */
//	format_add("irc_msg_f_chanh",	"%B<%W%{2@%+GCP}X%2%3/%5%B>%n %6", 1); /* NOT USED */
	format_add("irc_msg_f_chan_n",	"%B<%w%{2@%+gcp}X%2%3%B>%n %6", 1);
	format_add("irc_msg_f_chan_nh",	"%B<%W%{2@%+GCP}X%2%3%B>%n %6", 1);
	format_add("irc_msg_f_some",	"%b<%n%3%b>%n %6", 1);
//	format_add("irc_not_f_chan",	"%B(%w%{2@%+gcp}X%2%3/%5%B)%n %6", 1); /* NOT USED */
//	format_add("irc_not_f_chanh",	"%B(%W%{2@%+GCP}X%2%3/%5%B)%n %6", 1); /* NOT USED */
	format_add("irc_not_f_chan_n",	"%B(%w%{2@%+gcp}X%2%3%B)%n %6", 1);
	format_add("irc_not_f_chan_nh",	"%B(%W%{2@%+GCP}X%2%3%B)%n %6", 1);
	format_add("irc_not_f_some",	"%b(%n%3%b)%n %6", 1);
	format_add("irc_not_f_server",	"%g!%3%n %6", 1);
	format_add("IRC_NAMES_NAME",	_("[%gUsers %G%2%n]"), 1);
	format_add("IRC_NAMES",		"%K[%W%1%w%2%3%K]%n ", 1);
	format_add("IRC_NAMES_TOTAL_H",	_("%> %WEKG2: %2%n: Total of %W%3%n nicks [%W%4%n ops, %W%5%n halfops, %W%6%n voices, %W%7%n normal]\n"), 1);
	format_add("IRC_NAMES_TOTAL",	"%> %WEKG2: %2%n: Total of %W%3%n nicks [%W%4%n ops, %W%5%n voices, %W%6%n normal]\n", 1);
	format_add("irc_joined",	_("%> %Y%2%n has joined %4\n"), 1);
	format_add("irc_joined_you",	_("%> %RYou%n have joined %4\n"), 1);
	format_add("irc_left",		_("%> %g%2%n has left %4 (%5)\n"), 1);
	format_add("irc_left_you",	_("%> %RYou%n have left %4 (%5)\n"), 1);
	format_add("irc_kicked",	_("%> %Y%2%n has been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_kicked_you",	_("%> You have been kicked out by %R%3%n from %5 (%6)\n"), 1);
	format_add("irc_quit",		_("%> %Y%2%n has quit irc (%4)\n"), 1);
	format_add("irc_split",		"%> ", 1);
	format_add("irc_unknown_ctcp",	_("%> %Y%2%n sent unknown CTCP %3: (%4)\n"), 1);
	format_add("irc_ctcp_action_y_pub",	"%> %y%e* %2%n %4\n", 1);
	format_add("irc_ctcp_action_y",		"%> %Y%e* %2%n %4\n", 1);
	format_add("irc_ctcp_action_pub",	"%> %y%h* %2%n %5\n", 1);
	format_add("irc_ctcp_action",		"%> %Y%h* %2%n %5\n", 1);
	format_add("irc_ctcp_request_pub",	_("%> %Y%2%n requested ctcp %5 from %4\n"), 1);
	format_add("irc_ctcp_request",		_("%> %Y%2%n requested ctcp %5\n"), 1);
	format_add("irc_ctcp_reply",		_("%> %Y%2%n CTCP reply from %3: %5\n"), 1);
	format_add("IRC_ERR_CANNOTSENDTOCHAN",	"%! %2: %1\n", 1);
	format_add("IRC_RPL_FIRSTSECOND",	"%> (%1) %2 %3\n", 1);
	format_add("IRC_RPL_SECONDFIRST",	"%> (%1) %3 %2\n", 1);
	format_add("IRC_RPL_JUSTONE",		"%> (%1) %2\n", 1);
	format_add("IRC_RPL_NEWONE",		"%> (%1,%2) 1:%3 2:%4 3:%5 4:%6\n", 1);
	format_add("IRC_ERR_FIRSTSECOND",	"%! (%1) %2 %3\n", 1);
	format_add("IRC_ERR_SECONDFIRST",	"%! (%1) %3 %2\n", 1);
	format_add("IRC_ERR_JUSTONE",		"%! (%1) %2\n", 1);
	format_add("IRC_ERR_NEWONE",		"%! (%1,%2) 1:%3 2:%4 3:%5 4:%6\n", 1);
	format_add("IRC_RPL_CANTSEND",	_("%> Cannot send to channel %T%2%n\n"), 1);
	format_add("RPL_MOTDSTART",	"%g,+=%G-----\n", 1);
	format_add("RPL_MOTD",		"%g|| %n%2\n", 1);
	format_add("RPL_ENDOFMOTD",	"%g`+=%G-----\n", 1);
	format_add("RPL_INVITE",	_("%> Inviting %W%2%n to %W%3%n\n"), 1);
	format_add("RPL_LISTSTART",	"%g,+=%G-----\n", 1);
	format_add("RPL_EXCEPTLIST",	_("%g|| %n %5 - %W%2%n: except %c%3\n"), 1);
	format_add("RPL_BANLIST",	_("%g|| %n %5 - %W%2%n: ban %c%3\n"), 1);
	format_add("RPL_INVITELIST",	_("%g|| %n %5 - %W%2%n: invite %c%3\n"), 1);;
	format_add("RPL_EMPTYLIST" ,	_("%g|| %n Empty list \n"), 1);
	format_add("RPL_LINKS",		"%g|| %n %5 - %2  %3  %4\n", 1);
	format_add("RPL_ENDOFLIST", 	"%g`+=%G----- %2%n\n", 1);
	format_add("RPL_STATS",		"%g|| %3 %n %4 %5 %6 %7 %8\n", 1);
	format_add("RPL_STATS_EXT",	"%g|| %3 %n %2 %4 %5 %6 %7 %8\n", 1);
	format_add("RPL_STATSEND",	"%g`+=%G--%3--- %2\n", 1);
//	format_add("RPL_CHLISTSTART",  "%g,+=%G lp %2\t%3\t%4\n", 1);
//	format_add("RPL_CHLIST",       "%g|| %n %5 %2\t%3\t%4\n", 1);
	format_add("RPL_CHLISTSTART",	"%g,+=%G lp %2\t%3\t%4\n", 1);
	format_add("RPL_LIST",		"%g|| %n %5 %2\t%3\t%4\n", 1);
//	format_add("RPL_WHOREPLY",   "%g|| %c%3 %W%7 %n%8 %6 %4@%5 %W%9\n", 1);
	format_add("RPL_WHOREPLY",	"%g|| %c%3 %W%7 %n%8 %6 %4@%5 %W%9\n", 1);
	format_add("RPL_AWAY",		_("%G||%n away     : %2 - %3\n"), 1);
	format_add("RPL_WHOISUSER",	_("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
				"%G||%n realname : %6\n"), 1);
	format_add("RPL_WHOWASUSER",	_("%G.+===%g-----\n%G||%n (%T%2%n) (%3@%4)\n"
				"%G||%n realname : %6\n"), 1);
//	format_add("IRC_WHOERROR", _("%G.+===%g-----\n%G||%n %3 (%2)\n"), 1);
//	format_add("IRC_ERR_NOSUCHNICK", _("%n %3 (%2)\n"), 1);
	format_add("RPL_WHOISCHANNELS",	_("%G||%n %|channels : %3\n"), 1);
	format_add("RPL_WHOISSERVER",	_("%G||%n %|server   : %3 (%4)\n"), 1);
	format_add("RPL_WHOISOPERATOR",	_("%G||%n %|ircOp    : %3\n"), 1);
	format_add("RPL_WHOISIDLE",	_("%G||%n %|idle     : %3 (signon: %4)\n"), 1);
	format_add("RPL_ENDOFWHOIS",	_("%G`+===%g-----\n"), 1);
	format_add("RPL_ENDOFWHOWAS",	_("%G`+===%g-----\n"), 1);
	format_add("RPL_TOPIC",		_("%> Topic %2: %3\n"), 1);
	format_add("IRC_RPL_TOPICBY",	_("%> set by %2 on %4"), 1);
	format_add("IRC_TOPIC_CHANGE",	_("%> %T%2%n changed topic on %T%4%n: %5\n"), 1);
	format_add("IRC_TOPIC_UNSET",	_("%> %T%2%n unset topic on %T%4%n\n"), 1);
	format_add("IRC_MODE_CHAN_NEW",	_("%> %2/%4 sets mode [%5]\n"), 1);
	format_add("IRC_MODE_CHAN",	_("%> %2 mode is [%3]\n"), 1);
	format_add("IRC_MODE",		_("%> (%1) %2 set mode %3 on You\n"), 1);
	format_add("IRC_INVITE",	_("%> %W%2%n invites you to %W%5%n\n"), 1);
	format_add("IRC_PINGPONG",	_("%) (%1) ping/pong %c%2%n\n"), 1);
	format_add("IRC_YOUNEWNICK",	_("%> You are now known as %G%3%n\n"), 1);
	format_add("IRC_NEWNICK",	_("%> %g%2%n is now known as %G%4%n\n"), 1);
	format_add("IRC_TRYNICK",	_("%> Will try to use %G%2%n instead\n"), 1);
	format_add("IRC_CHANNEL_SYNCED", "%> Join to %W%2%n was synced in %W%3.%4%n secs", 1);
	format_add("IRC_TEST",		"%> (%1) %2 to %W%3%n [%4] port %W%5%n (%6)", 1);
	format_add("IRC_CONN_ESTAB",	"%> (%1) Connection to %W%3%n estabilished", 1);
	format_add("IRC_TEST_FAIL",	"%! (%1) Error: %2 to %W%3%n [%4] port %W%5%n (%7)", 1);
	format_add("irc_channel_secure",	"%) (%1) Echelon can kiss our ass on %2 *g*", 1); 
	format_add("irc_channel_unsecure",	"%! (%1) warning no plugin protect us on %2 :( install sim plugin now or at least rot13..", 1); 
	format_add("irc_access_added",	_("%> (%1) %3 [#%2] was added to accesslist chan: %4 (flags: %5)"), 1);
	format_add("irc_access_known", "a-> %2!%3@%4", 1);	/* %2 is nickname, not uid ! */
	format_add("user_info_auth_type", _("%K| %nSubscription type: %T%1%n\n"), 1);
	format_add("jabber_auth_subscribe", _("%> (%2) %T%1%n asks for authorisation. Use \"/auth -a %1\" to accept, \"/auth -d %1\" to refuse.%n\n"), 1);
	format_add("jabber_auth_unsubscribe", _("%> (%2) %T%1%n asks for removal. Use \"/auth -d %1\" to delete.%n\n"), 1);
	format_add("jabber_xmlerror_disconnect", _("Error parsing XML: %R%1%n"), 1);
	format_add("jabber_auth_request", _("%> (%2) Sent authorisation request to %T%1%n.\n"), 1);
	format_add("jabber_auth_accept", _("%> (%2) Authorised %T%1%n.\n"), 1);
	format_add("jabber_auth_unsubscribed", _("%> (%2) Asked %T%1%n to remove authorisation.\n"), 1);
	format_add("jabber_auth_cancel", _("%> (%2) Authorisation for %T%1%n revoked.\n"), 1);
	format_add("jabber_auth_denied", _("%> (%2) Authorisation for %T%1%n denied.\n"), 1);
	format_add("jabber_auth_probe", _("%> (%2) Sent presence probe to %T%1%n.\n"), 1);
	format_add("jabber_msg_failed", _("%! Message to %T%1%n can't be delivered: %R(%2) %r%3%n\n"),1);
	format_add("jabber_msg_failed_long", _("%! Message to %T%1%n %y(%n%K%4(...)%y)%n can't be delivered: %R(%2) %r%3%n\n"),1);
	format_add("jabber_version_response", _("%> Jabber ID: %T%1%n\n%> Client name: %T%2%n\n%> Client version: %T%3%n\n%> Operating system: %T%4%n\n"), 1);
	format_add("jabber_userinfo_response", _("%> Jabber ID: %T%1%n\n%> Full Name: %T%2%n\n%> Nickname: %T%3%n\n%> Birthday: %T%4%n\n%> City: %T%5%n\n%> Desc: %T%6%n\n"), 1);
	format_add("jabber_lastseen_response",	_("%> Jabber ID:  %T%1%n\n%> Logged out: %T%2 ago%n\n"), 1);
	format_add("jabber_lastseen_uptime",	_("%> Jabber ID: %T%1%n\n%> Server up: %T%2 ago%n\n"), 1);
	format_add("jabber_lastseen_idle",      _("%> Jabber ID: %T%1%n\n%> Idle for:  %T%2%n\n"), 1);
	format_add("jabber_unknown_resource", _("%! (%1) User's resource unknown%n\n\n"), 1);
	format_add("jabber_status_notavail", _("%! (%1) Unable to check version, because %2 is unavailable%n\n"), 1);
	format_add("jabber_charset_init_error", _("%! Error initialising charset conversion (%1->%2): %3"), 1);
	format_add("register_change_passwd", _("%> Your password for acount %T%1 is '%T%2%n' change it as fast as you can using command /jid:passwd <newpassword>"), 1);
	format_add("jabber_privacy_list_begin",   _("%g,+=%G----- Privacy list on %T%2%n"), 1);
	format_add("jabber_privacy_list_item",	  _("%g|| %n %3 - %W%4%n"), 1);					/* %3 - lp %4 - itemname */
	format_add("jabber_privacy_list_item_def",_("%g|| %g Default:%n %W%4%n"), 1);
	format_add("jabber_privacy_list_item_act",_("%g|| %r  Active:%n %W%4%n"), 1); 
	format_add("jabber_privacy_list_end",	  _("%g`+=%G----- End of the privacy list%n"), 1);
	format_add("jabber_privacy_list_noitem",  _("%! No privacy list in %T%2%n"), 1);
	format_add("jabber_privacy_item_header", _("%g,+=%G----- Details for: %T%3%n\n%g||%n JID\t\t\t\t\t  MSG  PIN POUT IQ%n"), 1);
	format_add("jabber_privacy_item",	   "%g||%n %[-44]4 \t%K|%n %[2]5 %K|%n %[2]6 %K|%n %[2]7 %K|%n %[2]8\n", 1);
	format_add("jabber_privacy_item_footer", _("%g`+=%G----- Legend: %n[%3] [%4]%n"), 1);
	format_add("jabber_privacy_item_allow",  "%G%1%n", 1);
	format_add("jabber_privacy_item_deny",   "%R%1%n", 1);
	format_add("jabber_private_list_header",  _("%g,+=%G----- Private list: %T%2/%3%n"), 1);
	format_add("jabber_bookmark_url",	_("%g|| %n URL: %W%3%n (%2)"), 1);		/* %1 - session_name, bookmark  url item: %2 - name %3 - url */
	format_add("jabber_bookmark_conf",	_("%g|| %n MUC: %W%3%n (%2)"), 1);	/* %1 - session_name, bookmark conf item: %2 - name %3 - jid %4 - autojoin %5 - nick %6 - password */
	format_add("jabber_private_list_item",	    "%g|| %n %4: %W%5%n",  1);			/* %4 - item %5 - value */
	format_add("jabber_private_list_session",   "%g|| + %n Session: %W%4%n",  1);		/* %4 - uid */
	format_add("jabber_private_list_plugin",    "%g|| + %n Plugin: %W%4 (%5)%n",  1);	/* %4 - name %5 - prio*/
	format_add("jabber_private_list_subitem",   "%g||  - %n %4: %W%5%n",  1);               /* %4 - item %5 - value */
	format_add("jabber_private_list_footer",  _("%g`+=%G----- End of the private list%n"), 1);
	format_add("jabber_private_list_empty",	  _("%! No list: %T%2/%3%n"), 1);
	format_add("jabber_transport_list_begin", _("%g,+=%G----- Avalible agents on: %T%2%n"), 1);
	format_add("jabber_transport_list_item",  _("%g|| %n %6 - %W%3%n (%5)"), 1);
	format_add("jabber_transport_list_item_node",("%g|| %n %6 - %W%3%n node: %g%4%n (%5)"), 1);
	format_add("jabber_transport_list_end",   _("%g`+=%G----- End of the agents list%n\n"), 1);
	format_add("jabber_transport_list_nolist", _("%! No agents @ %T%2%n"), 1);
	format_add("jabber_remotecontrols_list_begin", _("%g,+=%G----- Avalible remote controls on: %T%2%n"), 1);
	format_add("jabber_remotecontrols_list_item",  _("%g|| %n %6 - %W%4%n (%5)"), 1);		/* %3 - jid %4 - node %5 - descr %6 - seqid */
	format_add("jabber_remotecontrols_list_end",   _("%g`+=%G----- End of the remote controls list%n\n"), 1);
	format_add("jabber_remotecontrols_list_nolist", _("%! No remote controls @ %T%2%n"), 1);
	format_add("jabber_remotecontrols_executing",	_("%> (%1) Executing command: %W%3%n @ %W%2%n (%4)"), 1);
	format_add("jabber_remotecontrols_completed",	_("%> (%1) Command: %W%3%n @ %W%2 %gcompleted"), 1);
	format_add("jabber_remotecontrols_preparing",	_("%> (%1) Remote client: %W%2%n is preparing to execute command @node: %W%3"), 1);	/* %2 - uid %3 - node */
	format_add("jabber_remotecontrols_commited",	_("%> (%1) Remote client: %W%2%n executed command @node: %W%3"), 1);			/* %2 - uid %3 - node */
	format_add("jabber_remotecontrols_commited_status", _("%> (%1) RC %W%2%n: requested changing status to: %3 %4 with priority: %5"), 1);	/* %3 - status %4 - descr %5 - prio */
	format_add("jabber_remotecontrols_commited_command",_("%> (%1) RC %W%2%n: requested command: %W%3%n @ session: %4 window: %5 quiet: %6"), 1);	
	format_add("jabber_transinfo_begin",	_("%g,+=%G----- Information about: %T%2%n"), 1);
	format_add("jabber_transinfo_begin_node",_("%g,+=%G----- Information about: %T%2%n (%3)"), 1);
	format_add("jabber_transinfo_identify",	_("%g|| %G --== %g%3 %G==--%n"), 1);
	format_add("jabber_transinfo_feature",	_("%g|| %n %W%2%n feature: %n%3"), 1);
	format_add("jabber_transinfo_comm_ser",	_("%g|| %n %W%2%n can: %n%3 %2 (%4)"), 1);
	format_add("jabber_transinfo_comm_use",	_("%g|| %n %W%2%n can: %n%3 $uid (%4)"), 1);
	format_add("jabber_transinfo_comm_not",	_("%g|| %n %W%2%n can: %n%3 (%4)"), 1);
	format_add("jabber_transinfo_end",	_("%g`+=%G----- End of the infomations%n\n"), 1);
	format_add("jabber_search_item",	_("%) JID: %T%3%n\n%) Nickname:  %T%4%n\n%) Name: %T%5 %6%n\n%) Email: %T%7%n\n"), 1);	/* like gg-search_results_single */
	format_add("jabber_search_begin",	_("%g,+=%G----- Search on %T%2%n"), 1);
//	format_add("jabber_search_items", 	  "%g||%n %[-24]3 %K|%n %[10]5 %K|%n %[10]6 %K|%n %[12]4 %K|%n %[16]7\n", 1);		/* like gg-search_results_multi. TODO */
	format_add("jabber_search_items",	  "%g||%n %3 - %5 '%4' %6 <%7>", 1);
	format_add("jabber_search_end",		_("%g`+=%G-----"), 1);
	format_add("jabber_search_error",	_("%! Error while searching: %3\n"), 1);
	format_add("jabber_form_title",		  "%g,+=%G----- %3 %n(%T%2%n)", 1);
	format_add("jabber_form_item",		  "%g|| %n%(21)3 (%6) %K|%n --%4 %(20)5", 1); 	/* %3 - label %4 - keyname %5 - value %6 - req; optional */
	format_add("jabber_form_item_beg",	  "%g|| ,+=%G-----%n", 1);
	format_add("jabber_form_item_plain",	  "%g|| | %n %3: %5", 1);			/* %3 - label %4 - keyname %5 - value */
	format_add("jabber_form_item_end",	  "%g|| `+=%G-----%n", 1);
	format_add("jabber_form_item_val",	  "%K[%b%3%n %g%4%K]%n", 1);			/* %3 - value %4 - label */
	format_add("jabber_form_item_sub",        "%g|| %|%n\t%3", 1);			/* %3 formated jabber_form_item_val */
	format_add("jabber_form_command",	_("%g|| %nType %W/%3 %g%2 %W%4%n"), 1); 
	format_add("jabber_form_instructions", 	  "%g|| %n%|%3", 1);
	format_add("jabber_form_end",		_("%g`+=%G----- End of this %3 form ;)%n"), 1);
	format_add("jabber_registration_item", 	  "%g|| %n            --%3 %4%n", 1); /* %3 - keyname %4 - value */ /* XXX, merge */
	format_add("jabber_vacation", _("%> You'd set up your vacation status: %g%2%n (since: %3 expires@%4)"), 1);
	format_add("jabber_muc_recv", 	"%B<%w%X%5%3%B>%n %4", 1);
	format_add("jabber_muc_send",	"%B<%n%X%5%W%3%B>%n %4", 1);
	format_add("jabber_muc_room_created", 
		_("%> Room %W%2%n created, now to configure it: type %W/admin %g%2%n to get configuration form, or type %W/admin %g%2%n --instant to create instant one"), 1);
	format_add("jabber_muc_banlist", _("%g|| %n %5 - %W%2%n: ban %c%3%n [%4]"), 1);	/* %1 sesja %2 kanal %3 kto %4 reason %5 numerek */
//	format_add("jabber_send_chan", _("%B<%W%2%B>%n %5"), 1);
//	format_add("jabber_send_chan_n", _("%B<%W%2%B>%n %5"), 1);
//	format_add("jabber_recv_chan", _("%b<%w%2%b>%n %5"), 1);
//	format_add("jabber_recv_chan_n", _("%b<%w%2%b>%n %5"), 1);
	format_add("muc_joined", 	_("%> %C%2%n %B[%c%3%B]%n has joined %W%4%n as a %g%6%n and a %g%7%n"), 1);
	format_add("muc_left",		_("%> %c%2%n [%c%3%n] has left %W%4 %n[%5]\n"), 1);
	format_add("xmpp_feature_header", 	_("%g,+=%G----- XMPP features %n(%T%2%n%3%n)"), 1);	/* %3 - todo */
	format_add("xmpp_feature",	  	_("%g|| %n %W%2%n can: %5 [%G%3%g,%4%n]"), 1);
	format_add("xmpp_feature_sub",	  	_("%g|| %n     %W%3%n: %5 [%G%4%n]"), 1);
	format_add("xmpp_feature_sub_unknown",	_("%g|| %n     %W%3%n: Unknown, report to devs [%G%4%n]"), 1);
	format_add("xmpp_feature_unknown",	_("%g|| %n %W%2%n feature: %r%3 %n[%G%3%g,%4%n]"), 1);
	format_add("xmpp_feature_footer", 	_("%g`+=%G----- %n Turn it off using: /session display_server_features 0\n"), 1);
	format_add("gmail_new_mail", 	  _("%> (%1) Content of your mailbox have changed or new mail arrived."), 1);	/* sesja */
	format_add("gmail_count", 	  _("%> (%1) You have %T%2%n new thread(s) on your gmail account."), 1);	/* sesja, mail count */
	format_add("gmail_mail", 	  "%>    %|%T%2%n - %g%3%n\n", 1);						/* sesja, from, topic, [UNUSED messages count in thread (?1)] */
	format_add("gmail_thread",	  "%>    %|%T%2 [%4]%n - %g%3%n\n", 1);						/* sesja, from, topic, messages count in thread */
	format_add("tlen_mail",		_("%> (%1) New mail from %T%2%n, with subject: %G%3%n"), 1); 			/* sesja, from, topic */
	format_add("tlen_alert", 	_("%> (%1) %T%2%n sent us an alert ...%n"), 1); 				/* sesja, from */

	format_add("jabber_gpg_plugin",	_("%> (%1) To use OpenGPG support in jabber, first load gpg plugin!"), 1);	/* sesja */
	format_add("jabber_gpg_config",	_("%> (%1) First set gpg_key and gpg_password before turning on gpg_active!"), 1); /* sesja */
	format_add("jabber_gpg_ok",	_("%) (%1) GPG support: %gENABLED%n using key: %W%2%n"), 1);			/* sesja, klucz */
	format_add("jabber_gpg_sok",	_("%) GPG key: %W%2%n"), 1);							/* sesja, klucz for /status */
	format_add("jabber_gpg_fail", 	_("%> (%1) We didn't manage to sign testdata using key: %W%2%n (%R%3%n)\n"	/* sesja, klucz, error */
					"OpenGPG support for this session disabled."), 1);
	format_add("jabber_msg_xmlsyntaxerr",	_("%! Expat syntax-checking failed on your message: %T%1%n. Please correct your code or use double ^R to disable syntax-checking."), 1);
	format_add("jabber_conversations_begin",	_("%g,+=%G--%n (%1) %GAvailable Reply-IDs:%n"), 1);
	format_add("jabber_conversations_item",		_("%g|| %n %1 - %W%2%n (%g%3%n [%c%4%n])"), 1);		/* %1 - n, %2 - user, %3 - subject, %4 - thread */
	format_add("jabber_conversations_end",		_("%g`+=%G-- End of the available Reply-ID list%n"), 1);
	format_add("jabber_conversations_nothread",	_("non-threaded"), 1);
	format_add("jabber_conversations_nosubject",	_("[no subject]"), 1);
	format_add("jogger_noentry", _("%> (%1) No thread with id %c%2%n found."), 1);
	format_add("jogger_subscribed", _("%> %|(%1) The thread %T%2%n has been subscribed."), 1);
	format_add("jogger_unsubscribed", _("%> %|(%1) The thread %T%2%n has been unsubscribed."), 1);
	format_add("jogger_subscription_denied", _("%! (%1) Subscription denied because of no permission."), 1);
	format_add("jogger_unsubscribed_earlier", _("%> (%1) The thread weren't subscribed."), 1);
	format_add("jogger_comment_added", _("%) %|(%1) Your comment was added to entry %c%2%n."), 1);
	format_add("jogger_modified", _("%> %|(%1) Subscribed entry has been modified: %c%2%n."), 1);
	format_add("jogger_published", _("%) %|(%1) Your new entry has been published as: %c%2%n."), 1);
	format_add("jogger_version", _("%> %TJogger:%n match data %g%1%n."), 1);

	format_add("jogger_prepared", _("%) File %T%1%n is ready for submission."), 1);
	format_add("jogger_notprepared", _("%! No filename given and no entry prepared!"), 1);
	format_add("jogger_hashdiffers", _("%! %|File contents (checksum) differs from the time it was prepared. If you changed anything in the entry file, please run %Tprepare%n again. If you want to force submission, please use %Tpublish%n again."), 1);
	format_add("jogger_warning", _("%) %|During QA check of the entry, following warnings have been issued:"), 1);
	format_add("jogger_warning_brokenheader", _("%> %|* Header with broken syntax found at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_key", _("%> %|* Header contains unknown/wrong key at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_key_spaces", _("%> %|* Key in header mustn't be followed or preceeded by spaces at: %c%1%n"), 1);
	format_add("jogger_warning_deprecated_miniblog", _("%> %|* Key %Tminiblog%n is deprecated in favor of such category at: %c%1%n"), 1);
	format_add("jogger_warning_miniblog_techblog", _("%> %|* Miniblog entry will be posted to Techblog at: %c%1%n"), 1);
	format_add("jogger_warning_techblog_only", _("%> * Entries posted to Techblog should have also some normal category: %c%1%n"), 1);
	format_add("jogger_warning_malformed_url", _("%> %|* Malformed URL found at: %c%1%n"), 1);
	format_add("jogger_warning_spacesep", _("%> %|* Possibility of accidentially using space as a separator instead of commas: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value", _("%> %|* Incorrect value found at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value_level", _("%> %|* Wrong %Tlevel%n found (level %Tnumber%n should be used), entry would be published on %Tzeroth%n level (not default) at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value_spaces", _("%> %|* Incorrent value found (try to remove leading&trailing spaces) at: %c%1%n"), 1);
	format_add("jogger_warning_wrong_value_empty", _("%> %|* Empty value found in header at: %c%1%n"), 1);
	format_add("jogger_warning_duplicated_header", _("%> %|* Duplicated header found at: %c%1%n"), 1);
	format_add("jogger_warning_mislocated_header", _("%> %|* Mislocated header (?) at: %c%1%n"), 1);
	format_add("jogger_warning_noexcerpt", _("%> %|* Entry text size exceeds 4096 bytes, but no <EXCERPT> tag has been found. It will be probably cut by Jogger near: ...%c%1%n..."), 1);
	format_add("logsoracle_status", _("%> logsoracle status\n"), 1);
	format_add("logsoracle_status_con", _("%>  connected: %T%1%n\n"), 1);
	format_add("logsoracle_status_sta", _("%>  status changes: %T%1%n\n"), 1);
	format_add("logsoracle_status_msg", _("%>  messages: %T%1%n\n"), 1);
	format_add("logsoracle_error", _("%! oracle error message:\n\n%W%1%n\n\n%! end of message\n"), 1);
	format_add("logsoracle_connected", _("%> connected to Oracle\n"), 1);
	format_add("logsoracle_disconnected", _("%> disconnected from Oracle\n"), 1);
        format_add("logsoracle_disconn_not_needed", _("%> not connected to database\n"), 1);
	format_add("logsoracle_already_connected", _("%> already connected to Oracle. use 'disconnect' first\n"), 1); 
	format_add("logsqlite_open_error", "%! Can't open database: %1\n", 1);
	format_add("new_mail_one", _("%) You got one email\n"), 1);
	format_add("new_mail_two_four", _("%) You got %1 new emails\n"), 1);
	format_add("new_mail_more", _("%) You got %1 new emails\n"), 1);
	format_add("readline_prompt", "% ", 1);
	format_add("readline_prompt_away", "/ ", 1);
	format_add("readline_prompt_invisible", ". ", 1);
	format_add("readline_prompt_query", "%1> ", 1);
	format_add("readline_prompt_win", "%1%% ", 1);
	format_add("readline_prompt_away_win", "%1/ ", 1);
	format_add("readline_prompt_invisible_win", "%1. ", 1);
	format_add("readline_prompt_query_win", "%2:%1> ", 1);
	format_add("readline_prompt_win_act", "%1 (act/%2)%% ", 1);
	format_add("readline_prompt_away_win_act", "%1 (act/%2)/ ", 1);
	format_add("readline_prompt_invisible_win_act", "%1 (act/%2). ", 1);
	format_add("readline_prompt_query_win_act", "%2:%1 (act/%3)> ", 1);
	format_add("readline_more", _("-- Press Enter to continue or Ctrl-D to break --"), 1);
	format_add("rot_generic", _("%> Text: %1 roted: %2"), 1);
	format_add("rot_list", _("%> Sesja: %1 target: %2 (rot%3 +%4)"), 1);
	format_add("key_generating", _("%> Please wait, generating keys...\n"), 1);
	format_add("key_generating_success", _("%> Keys generated and saved\n"), 1);
	format_add("key_generating_error", _("%! Error while generating keys: %1\n"), 1);
	format_add("key_private_exist", _("%! You already own a key pair\n"), 1);
	format_add("key_public_deleted", _("%) Public key %1 removew\n"), 1);
	format_add("key_public_not_found", _("%! Can find %1's public key\n"), 1);
	format_add("key_public_noexist", _("%! No public keys\n"), 1);
	format_add("key_public_received", _("%> Received public key from %1\n"), 1);
	format_add("key_public_write_failed", _("%! Error while saving public key: %1\n"), 1);
	format_add("key_send_success", _("%> Sent public key to %1\n"), 1);
	format_add("key_send_error", _("%! Error sending public key\n"), 1);
	format_add("key_list", "%> %r%1%n (%3)\n%) fingerprint: %y%2\n", 1);
	format_add("key_list_timestamp", "%Y-%m-%d %H:%M", 1);
        format_add("sms_error", _("%! Error sending SMS: %1\n"), 1);
        format_add("sms_unknown", _("%! %1 not a cellphone number\n"), 1);
        format_add("sms_sent", _("%> SMS to %T%1%n sent\n"), 1);
        format_add("sms_failed", _("%! SMS to %T%1%n not sent\n"), 1);
        format_add("sms_away", "<ekg:%1> %2", 1);
	format_add("sniff_gg_welcome",	_("%) %b[GG_WELCOME] %gSEED: %W%1"), 1);
	format_add("sniff_gg_login60",	_("%) %b[GG_LOGIN60] %gUIN: %W%1 %gHASH: %W%2"), 1);
	format_add("sniff_gg_login70_sha1",	_("%) %b[GG_LOGIN70] %gUIN: %W%1 %gSHA1: %W%2"), 1);
	format_add("sniff_gg_login70_hash",	_("%) %b[GG_LOGIN70] %gUIN: %W%1 %gHASH: %W%2"), 1);
	format_add("sniff_gg_login70_unknown",	_("%) %b[GG_LOGIN70] %gUIN: %W%1 %gTYPE: %W%2"), 1);
	format_add("sniff_gg_status60", _("%) %b[GG_STATUS60] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_status77", _("%) %b[GG_STATUS77] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_notify77", _("%) %b[GG_NOTIFY77] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_addnotify",_("%) %b[GG_ADD_NOTIFY] %gUIN: %W%1 %gDATA: %W%2"), 1);
	format_add("sniff_gg_delnotify",_("%) %b[GG_REMOVE_NOTIFY] %gUIN: %W%1 %gDATA: %W%2"), 1);
	format_add("sniff_pkt_rcv", _("%) %2 packets captured"), 1);
	format_add("sniff_pkt_drop",_("%) %2 packets dropped"), 1);
	format_add("sniff_conn_db", 		_("%) %2 connections founded"), 1);
	format_add("sniff_tcp_connection",	"TCP %1:%2 <==> %3:%4", 1);
	format_add("xmsg_addwatch_failed", _("Unable to add inotify watch (wrong path?)"), 1);
	format_add("xmsg_nosendcmd", _("%> (%1) You need to set %csend_cmd%n to be able to send msgs"), 1);
	format_add("xmsg_toobig", _("%> (%2) File %T%1%n is larger than %cmax_filesize%n, skipping"), 1);
	format_add("xmsg_toobigrm", _("%> (%2) File %T%1%n was larger than %cmax_filesize%n, removed"), 1);
	format_add("xmsg_umount", _("volume containing watched directory was unmounted"), 1);
	format_add("xosd_new_message_irc", _("new message from %1 at %2"), 1);
	format_add("xosd_new_message_line_1", _("new message from %1"), 1);
	format_add("xosd_new_message_line_2_long", "%1...", 1);
	format_add("xosd_new_message_line_2", "%1", 1);
	format_add("xosd_status_change_avail", _("%1 is online,"), 1);
	format_add("xosd_status_change_away", _("%1 is away,"), 1);
	format_add("xosd_status_change_dnd", _("%1: do not disturb,"), 1);
	format_add("xosd_status_change_xa", _("%1 is extended away,"), 1);
	format_add("xosd_status_change_notavail", _("%1 is offline,"), 1);
	format_add("xosd_status_change_invisible", _("%1 is invisible,"), 1);
	format_add("xosd_status_change_chat", _("%1 is free for chat,"), 1);
	format_add("xosd_status_change_error", _("%1: status error,"), 1);
	format_add("xosd_status_change_blocking", _("%1 is blocking us,"), 1);
	format_add("xosd_status_change_description", "%1", 1);
	format_add("xosd_status_change_description_long", "%1...", 1);
	format_add("xosd_status_change_no_description", _("[no description]"), 1);
	format_add("xosd_welcome_message_line_1", _("ekg2 XOnScreenDisplay plugin"), 1);
	format_add("xosd_welcome_message_line_2", _("Author: Adam 'dredzik' Kuczynski"), 1);

	for (i = 0; i < 1; i++) {
		list_t l;

		for (l = formats; l; l = l->next) {
			struct format *f = l->data;
			
			format_find(f->name);
		}
	}

	{
		int totalhash = 0;

		for (i = 0; i < 0x100; i++)
			totalhash += hashes[i];

		printf("-- %d\n", totalhash);

		for (i = 0; i < 0x100; i++)
			printf("%d %.2f\n", hashes[i], (float) ( ((float) hashes[i] / (float) totalhash) * 100));
	}
	return 0;
}

