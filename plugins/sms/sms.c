/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

typedef struct {
        char *uid;
        int count;
} sms_away_t;

static list_t sms_away = NULL;
static int config_sms_away = 0;
static int config_sms_away_limit = 0;
static char *config_sms_number = NULL;
static char *config_sms_app = NULL;
static int config_sms_max_length = 100;

PLUGIN_DEFINE(sms, PLUGIN_GENERIC, NULL);

static void sms_child_handler(child_t *c, int pid, const char *name, int status, void *data)
{
        char *number = data;

        if (number) {
                print((!status) ? "sms_sent" : "sms_failed", number);
                xfree(number);
        }
}

/*
 * sms_send()
 *
 * wysy³a sms o podanej tre¶ci do podanej osoby.
 *
 * 0/-1
 */
static int sms_send(const char *recipient, const char *message)
{
        int pid, fd[2] = { 0, 0 };
        char *tmp;

        if (!config_sms_app) {
                errno = EINVAL;
                return -1;
        }

        if (!recipient || !message) {
                errno = EINVAL;
                return -1;
        }

        if (pipe(fd))
                return -1;

        if (!(pid = fork())) {
                dup2(open("/dev/null", O_RDONLY), 0);

                if (fd[1]) {
                        close(fd[0]);
                        dup2(fd[1], 2);
                        dup2(fd[1], 1);
                        close(fd[1]);
                }

                execlp(config_sms_app, config_sms_app, recipient, message, (void *) NULL);
                exit(1);
        }

        if (pid < 0) {
                close(fd[0]);
                close(fd[1]);
                return -1;
        }

        close(fd[1]);

        tmp = saprintf("%s %s %s", config_sms_app, recipient, message);
        child_add(&sms_plugin, pid, tmp, sms_child_handler, xstrdup(recipient));
        xfree(tmp);

        return 0;
}

/*
 * sms_away_add()
 *
 * dodaje osobê do listy delikwentów, od których wiadomo¶æ wys³ano sms'em
 * podczas naszej nieobecno¶ci. je¶li jest ju¿ na li¶cie, to zwiêksza
 * przyporz±dkowany mu licznik.
 *
 *  - uin.
 */
static void sms_away_add(const char *uid)
{
        sms_away_t sa;
        list_t l;

        if (!config_sms_away_limit)
                return;

        sa.uid = xstrdup(uid);
        sa.count = 1;

        for (l = sms_away; l; l = l->next) {
                sms_away_t *s = l->data;

                if (!xstrcasecmp(s->uid, uid)) {
                        s->count += 1;
                        return;
                }
        }

        list_add(&sms_away, &sa, sizeof(sa));
}

/*
 * sms_away_check()
 *
 * sprawdza czy wiadomo¶æ od danej osoby mo¿e zostaæ przekazana
 * na sms podczas naszej nieobecno¶ci, czy te¿ mo¿e przekroczono
 * ju¿ limit.
 *
 *  - uin
 *
 * 1 je¶li tak, 0 je¶li nie.
 */
static int sms_away_check(const char *uid)
{
        int x = 0;
        list_t l;

        if (!config_sms_away_limit || !sms_away)
                return 1;

        /* limit dotyczy ³±cznej liczby sms'ów */
        if (config_sms_away == 1) {
                for (l = sms_away; l; l = l->next) {
                        sms_away_t *s = l->data;

                        x += s->count;
                }

                if (x > config_sms_away_limit)
                        return 0;
                else
                        return 1;
        }

        /* limit dotyczy liczby sms'ów od jednej osoby */
        for (l = sms_away; l; l = l->next) {
                sms_away_t *s = l->data;

                if (!xstrcasecmp(s->uid, uid)) {
                        if (s->count > config_sms_away_limit)
                                return 0;
                        else
                                return 1;
                }
        }

        return 1;
}

/*
 * sms_away_free()
 *
 * zwalnia pamiêæ po li¶cie ,,sms_away''
 */
static void sms_away_free()
{
        list_t l;

        if (!sms_away)
                return;

        for (l = sms_away; l; l = l->next) {
                sms_away_t *s = l->data;

                xfree(s->uid);
        }

        list_destroy(sms_away, 1);
        sms_away = NULL;
}

static COMMAND(sms_command_sms)
{
        userlist_t *u;
        const char *number = NULL;

        if (!params[0] || !params[1] || !config_sms_app) {
                printq("not_enough_params", name);
                return -1;
        }

        if ((u = userlist_find(session, params[0]))) {
                if (!u->mobile || !xstrcmp(u->mobile, "")) {
                        printq("sms_unknown", format_user(session, u->uid));
                        return -1;
                }
                number = u->mobile;
        } else
                number = params[0];

        if (sms_send(number, params[1]) == -1) {
                printq("sms_error", strerror(errno));
                return -1;
        }

        return 0;
}

static int dd_sms(const char *name)
{
        return (config_sms_app != NULL);
}

/*
 * sms_session_status()
 *
 * obs³uga zmiany stanu sesji.
 */
static int sms_session_status(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__status = va_arg(ap, char**), *status = *__status;

        if (session);   /* warning */

        if (xstrcmp(status, EKG_STATUS_AWAY) && xstrcmp(status, EKG_STATUS_XA) && xstrcmp(status, EKG_STATUS_DND))
                sms_away_free();

        return 0;
}

/*
 * sms_protocol_message()
 *
 * obs³uga przychodz±cych wiadomo¶ci.
 */
static int sms_protocol_message(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        char ***__rcpts = va_arg(ap, char***);
        char **__text = va_arg(ap, char**), *text = *__text;
        const char *status = session_status_get_n(session);

        if (!status || !config_sms_away || !config_sms_app || !config_sms_number)
                return 0;

        if (xstrcmp(status, EKG_STATUS_AWAY) && xstrcmp(status, EKG_STATUS_XA) && xstrcmp(status, EKG_STATUS_DND))
                return 0;

        sms_away_add(uid);

        if (sms_away_check(uid)) {
                const char *sender;
                char *msg;
                userlist_t *u = userlist_find(session_find(session), uid);

                sender = (u && u->nickname) ? u->nickname : uid;

                if (config_sms_max_length && xstrlen(text) > config_sms_max_length) {
                        char *tmp = xstrmid(text, 0, config_sms_max_length);
                        msg = format_string(format_find("sms_away"), sender, tmp);
                        xfree(tmp);
                } else
                        msg = format_string(format_find("sms_away"), sender, text);

                /* niech nie wysy³a smsów, je¶li brakuje formatów */
                if (xstrcmp(msg, ""))
                        sms_send(config_sms_number, msg);

                xfree(msg);
        }

        if (__rcpts);   /* warning */

        return 0;
}

static void sms_changed_sms_away(const char *name)
{
        static int last = -1;

        if (last != -1 && last == config_sms_away)
                return;

        if (config_sms_away)
                query_connect(&sms_plugin, "protocol-message", sms_protocol_message, NULL);
        else
                query_disconnect(&sms_plugin, "protocol-message");

        last = config_sms_away;
}

int sms_plugin_init()
{
        plugin_register(&sms_plugin);

        command_add(&sms_plugin, "sms:sms", "u ?", sms_command_sms, 0, NULL);

        variable_add(&sms_plugin, "sms_send_app", VAR_STR, 1, &config_sms_app, NULL, NULL, NULL);
        variable_add(&sms_plugin, "sms_away", VAR_MAP, 1, &config_sms_away, NULL, variable_map(3, 0, 0, "none", 1, 2, "all", 2, 1, "separate"), dd_sms);
        variable_add(&sms_plugin, "sms_away_limit", VAR_INT, 1, &config_sms_away_limit, sms_changed_sms_away, NULL, dd_sms);
        variable_add(&sms_plugin, "sms_max_length", VAR_INT, 1, &config_sms_max_length, NULL, NULL, dd_sms);
        variable_add(&sms_plugin, "sms_number", VAR_STR, 1, &config_sms_number, NULL, NULL, dd_sms);

        if (config_sms_away)
                query_connect(&sms_plugin, "protocol-message", sms_protocol_message, NULL);

        query_connect(&sms_plugin, "session-status", sms_session_status, NULL);

        format_add("sms_error", _("%! Error sending SMS: %1\n"), 1);
        format_add("sms_unknown", _("%! %1 not a cellphone number\n"), 1);
        format_add("sms_sent", _("%> SMS to %T%1%n sent\n"), 1);
        format_add("sms_failed", _("%! SMS to %T%1%n not sent\n"), 1);
        format_add("sms_away", "<ekg:%1> %2", 1);

        return 0;
}

static int sms_plugin_destroy()
{
        plugin_unregister(&sms_plugin);

        return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
