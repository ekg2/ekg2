/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dynstuff.h"
#include "stuff.h"
#include "themes.h"
#include "xmalloc.h"
#include "windows.h"
#include "userlist.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

char *prompt_cache = NULL, *prompt2_cache = NULL, *error_cache = NULL;
const char *timestamp_cache = NULL;

int no_prompt_cache = 0;

list_t formats = NULL;

/*
 * format_find()
 *
 * odnajduje warto¶æ danego formatu. je¶li nie znajdzie, zwraca pusty ci±g,
 * ¿eby nie musieæ uwa¿aæ na ¿adne null-references.
 *
 *  - name.
 */
const char *format_find(const char *name)
{
        const char *tmp;
        int hash;
        list_t l;

        if (!name)
                return "";

        hash = ekg_hash(name);

        if (config_speech_app && !xstrchr(name, ',')) {
                char *name2 = saprintf("%s,speech", name);
                const char *tmp;

                if (xstrcmp((tmp = format_find(name2)), "")) {
                        xfree(name2);
                        return tmp;
                }

                xfree(name2);
        }

        if (config_theme && (tmp = xstrchr(config_theme, ',')) && !xstrchr(name, ',')) {
                char *name2 = saprintf("%s,%s", name, tmp + 1);
                const char *tmp;

                if (xstrcmp((tmp = format_find(name2)), "")) {
                        xfree(name2);
                        return tmp;
                }

                xfree(name2);
        }

        for (l = formats; l; l = l->next) {
                struct format *f = l->data;

                if (hash == f->name_hash && !xstrcasecmp(f->name, name))
                        return f->value;
        }

        return "";
}

/*
 * format_ansi()
 *
 * zwraca sekwencjê ansi odpowiadaj±c± danemu kolorkowi z thememów ekg.
 */
const char *format_ansi(char ch)
{
        if (ch == 'k')
                return "\033[2;30m";
        if (ch == 'K')
                return "\033[1;30m";
        if (ch == 'l')
                return "\033[40m";
        if (ch == 'r')
                return "\033[2;31m";
        if (ch == 'R')
                return "\033[1;31m";
        if (ch == 's')
                return "\033[41m";
        if (ch == 'g')
                return "\033[2;32m";
        if (ch == 'G')
                return "\033[1;32m";
        if (ch == 'h')
                return "\033[42m";
        if (ch == 'y')
                return "\033[2;33m";
        if (ch == 'Y')
                return "\033[1;33m";
        if (ch == 'z')
                return "\033[43m";
        if (ch == 'b')
                return "\033[2;34m";
        if (ch == 'B')
                return "\033[1;34m";
        if (ch == 'e')
                return "\033[44m";
        if (ch == 'm' || ch == 'p')
                return "\033[2;35m";
        if (ch == 'M' || ch == 'P')
                return "\033[1;35m";
        if (ch == 'q')
                return "\033[45m";
        if (ch == 'c')
                return "\033[2;36m";
        if (ch == 'C')
                return "\033[1;36m";
        if (ch == 'd')
                return "\033[46m";
        if (ch == 'w')
                return "\033[2;37m";
        if (ch == 'W')
                return "\033[1;37m";
        if (ch == 'x')
                return "\033[47m";
        if (ch == 'n')                  /* clear all attributes */
                return "\033[0m";
        if (ch == 'T')                  /* bold */
                return "\033[1m";
        if (ch == 'N')                  /* clears all attr exc for bkgd */
                return "\033[2m";
        if (ch == 'U')                  /* underline */
                return "\033[4m";
        if (ch == 'i')                  /* blink */
                return "\033[5m";
        if (ch == 'V')                  /* reverse */
                return "\033[7m";

        return "";
}

/*
 * va_format_string()
 *
 * formatuje zgodnie z podanymi parametrami ci±g znaków.
 *
 *  - format - warto¶æ, nie nazwa formatu,
 *  - ap - argumenty.
 */
char *va_format_string(const char *format, va_list ap)
{
        static int dont_resolve = 0;
        string_t buf = string_init(NULL);
        const char *p, *args[9];
        int i, argc = 0;

        /* liczymy ilo¶æ argumentów */
        for (p = format; *p; p++) {
                if (*p == '\\' && p[1] == '%') {
                        p += 2;
                        continue;
                }

                if (*p != '%')
                        continue;

                p++;

                if (!*p)
                        break;

                if (*p == '@') {
                        p++;

                        if (!*p)
                                break;

                        if ((*p - '0') > argc)
                                argc = *p - '0';

                } else if (*p == '(' || *p == '[') {
                        if (*p == '(') {
                                while (*p && *p != ')')
                                        p++;
                        } else {
                                while (*p && *p != ']')
                                        p++;
                        }

                        if (*p)
                                p++;

                        if (!*p)
                                break;

                        if ((*p - '0') > argc)
                                argc = *p - '0';
                } else {
                        if (*p >= '1' && *p <= '9' && (*p - '0') > argc)
                                argc = *p - '0';
                }
        }

        for (i = 0; i < 9; i++)
                args[i] = NULL;

        for (i = 0; i < argc; i++)
                args[i] = va_arg(ap, char*);

        if (!dont_resolve) {
                dont_resolve = 1;
                if (no_prompt_cache) {
                        /* zawsze czytaj */
                        timestamp_cache = format_find("timestamp");
                        prompt_cache = format_string(format_find("prompt"));
                        prompt2_cache = format_string(format_find("prompt2"));
                        error_cache = format_string(format_find("error"));
                } else {
                        /* tylko je¶li nie s± keszowanie */
                        if (!timestamp_cache)
                                timestamp_cache = format_find("timestamp");
                        if (!prompt_cache)
                                prompt_cache = format_string(format_find("prompt"));
                        if (!prompt2_cache)
                                prompt2_cache = format_string(format_find("prompt2"));
                        if (!error_cache)
                                error_cache = format_string(format_find("error"));
                }
                dont_resolve = 0;
        }

        p = format;

        while (*p) {
                int escaped = 0;

                if (*p == '\\' && (p[1] == '%' || p[1] == '\\')) {
                        escaped = 1;
                        p++;
                }

                if (*p == '%' && !escaped) {
                        int fill_before, fill_after, fill_soft, fill_length, hm;
                        char fill_char, *cnt=NULL;

                        p++;
                        if (!*p)
                                break;
                        if (*p == '{')
                        {
                                char *str;
                                p++;
                                if (*p >= '0' && *p <= '9')
                                        str = (char *)args[*p - '1'];
                                else if (*p == '}') {
                                        p++; p++; continue;
                                } else {
                                        p++; continue;
                                }
                                p++;
                                cnt = (char *)p;
                                hm = 0;

                                while (*cnt && *cnt!='}') { cnt++; hm++; }
                                hm>>=1; cnt=(char *)(p+hm);
                                /* debug(">>> [HM:%d]", hm); */
                                for (; hm>0; hm--) {
                                        if (*p == *str) break;
                                        p++; cnt++;
                                }
                                /* debug(" [%c%c][%d] [%s] ", *p, *cnt, hm, p); */
                                if (!hm) { p=*cnt?*(cnt+1)?(cnt+2):(cnt+1):cnt; continue; }
                                p=(cnt+hm+1);
                                /* debug(" [%s]\n"); */
                                *((char *)p)=*cnt;
                        }
                        if (*p == '%')
                                string_append_c(buf, '%');
                        if (*p == '>')
                                string_append(buf, prompt_cache);
                        if (*p == ')')
                                string_append(buf, prompt2_cache);
                        if (*p == '!')
                                string_append(buf, error_cache);
                        if (*p == '|')
                                string_append(buf, "\033[00m"); /* g³upie, wiem */
                        if (*p == ']')
                                string_append(buf, "\033[000m");        /* jeszcze g³upsze */
                        if (*p == '#')
                                string_append(buf, timestamp(timestamp_cache));
                        else if (config_display_color) {
                                string_append(buf, format_ansi(*p));
                        }

                        if (*p == '@') {
                                char *str = (char*) args[*(p + 1) - '1'];

                                if (str) {
                                        char *q = str + xstrlen(str) - 1;

                                        while (q >= str && !isalpha_pl_PL(*q))
                                                q--;

                                        if (*q == 'a')
                                                string_append(buf, "a");
                                        else
                                                string_append(buf, "y");
                                }
                                p += 2;
                                continue;
                        }

                        fill_before = 0;
                        fill_after = 0;
                        fill_length = 0;
                        fill_char = ' ';
                        fill_soft = 1;

                        if (*p == '[' || *p == '(') {
                                char *q;

                                fill_soft = (*p == '(');

                                p++;
                                fill_char = ' ';

                                if (*p == '.') {
                                        fill_char = '0';
                                        p++;
                                } else if (*p == ',') {
                                        fill_char = '.';
                                        p++;
                                } else if (*p == '_') {
                                        fill_char = '_';
                                        p++;
                                }

                                fill_length = strtol(p, &q, 0);
                                p = q;
                                if (fill_length > 0)
                                        fill_after = 1;
                                else {
                                        fill_length = -fill_length;
                                        fill_before = 1;
                                }
                                p++;
                        }

                        if (*p >= '1' && *p <= '9') {
                                char *str = (char *) args[*p - '1'];
                                int i, len;

                                if (!str)
                                        str = "";
                                len = xstrlen(str);

                                if (fill_length) {
                                        if (len >= fill_length) {
                                                if (!fill_soft)
                                                        len = fill_length;
                                                fill_length = 0;
                                        } else
                                                fill_length -= len;
                                }

                                if (fill_before)
                                        for (i = 0; i < fill_length; i++)
                                                string_append_c(buf, fill_char);

                                string_append_n(buf, str, len);

                                if (fill_after)
                                        for (i = 0; i < fill_length; i++)
                                                string_append_c(buf, fill_char);

                        }
                } else
                        string_append_c(buf, *p);

                p++;
        }

        if (!dont_resolve && no_prompt_cache)
                theme_cache_reset();

        if (!config_display_pl_chars)
                iso_to_ascii(buf->str);

        return string_free(buf, 0);
}

/*
 * fstring_new()
 *
 * zamienia sformatowany ci±g znaków ansi na Nowy-i-Lepszy(tm).
 *
 *  - str - ci±g znaków,
 *
 * zwraca zaalokowan± fstring_t.
 */
fstring_t *fstring_new(const char *str)
{
        fstring_t *res = xmalloc(sizeof(fstring_t));
        short attr = 128;
        int i, j, len = 0, isbold = 0;

        res->margin_left = -1;

        for (i = 0; str[i]; i++) {
                if (str[i] == 27) {
                        if (str[i + 1] != '[')
                                continue;

                        while (str[i] && !isalpha_pl_PL(str[i]))
                                i++;

                        i--;

                        continue;
                }

                if (str[i] == 9) {
                        len += (8 - (len % 8));
                        continue;
                }

                if (str[i] == 13)
                        continue;

                if (str[i + 1] && str[i] == '/' && str[i + 1] == '|') {
                        if ((i != 0 && str[i - 1] != '/') || i == 0) {
                                i++;
                                continue;
                        }
                        continue;
                }

                len++;
        }

        res->str = xmalloc(len + 1);
        res->attr = xmalloc((len + 1) * sizeof(short));
        res->prompt_len = 0;
        res->prompt_empty = 0;

        for (i = 0, j = 0; str[i]; i++) {
                if (str[i] == 27) {
                        int tmp = 0;
                        int m, ism, once=1, deli;
                        char *p;

                        if (str[i + 1] != '[')
                                continue;

                        i += 2;

                        /* obs³uguje tylko "\033[...m", tak ma byæ */
                        p=(char *)&(str[i]);
                        while (1) {
                                ism=deli=0;
                                ism=sscanf(p, "%02d", &m);
                                if (ism) {
                                        p++; deli++; i++;
                                        if(isdigit(*p)) { p++; deli++; i++; }
                                        if(once && isdigit(*p)) { p++; deli++; i++; }
                                }
                                once = 0;
                                if (*p == ';' || *p == 'm') {
                                        if (!ism)
                                                goto wedonthavem;
                                        if (m == 0) {
                                                attr = 128;
                                                isbold = 0;
                                                if (deli >= 2)
                                                        res->prompt_len = j;
                                                if (i>3 && deli == 3)
                                                        res->prompt_empty = 1;
                                        }
                                        else if (m == 1) /* bold */
                                        {
                                                if (*p == 'm' && !isbold)
                                                        attr ^= 64;
                                                if (*p == ';')  {
                                                        attr |= 64;
                                                        isbold = 1;
                                                }
                                        }
                                        else if (m == 2) {
                                                attr &= (56);
                                                isbold = 0;
                                        } else if (m == 4) /* underline */
                                                attr ^= 512;
                                        else if (m == 5) /* blink */
                                                attr ^= 256;
                                        else if (m == 7) /* reverse */
                                                attr ^= 1024;
                                        else if (m>=30)
                                                tmp = m;
wedonthavem:
                                        if (tmp >= 30 && tmp <= 37) {
                                                attr &= ~(128+1+2+4);
                                                attr |= (tmp - 30);
                                        }

                                        if (tmp >= 40 && tmp <= 47) {
                                                attr &= ~(128+8+16+32);
                                                attr |= (tmp - 40) << 3;
                                        }
                                        if (*p == ';') { i++; p++; }
                                }
                                if (*p == 'm') break;
                                tmp = 0;
                        }

                        continue;
                }

                if (str[i] == 13)
                        continue;

                if (str[i + 1] && str[i] == '/' && str[i + 1] == '|') {
                        if ((i != 0 && str[i - 1] != '/') || i == 0) {
                                res->margin_left = j;
                                i++;
                                continue;
                        }
                        continue;
                }

                if (str[i] == 9) {
                        int k = 0, l = 8 - (j % 8);

                        for (k = 0; k < l; j++, k++) {
                                res->str[j] = ' ';
                                res->attr[j] = attr;
                        }

                        continue;
                }

                res->str[j] = str[i];
                res->attr[j] = attr;
                j++;
        }

        res->str[j] = 0;
        res->attr[j] = 0;

        return res;
}

/*
 * fstring_free()
 *
 * zwalnia pamiêæ zajmowan± przez fstring_t
 *
 *  - str - do usuniêcia.
 */
void fstring_free(fstring_t *str)
{
        if (!str)
                return;

        xfree(str->str);
        xfree(str->attr);
        xfree(str->private);
        xfree(str);
}

/*
 * format_string()
 *
 * j.w. tyle ¿e nie potrzeba dawaæ mu va_list, a wystarcz± zwyk³e parametry.
 *
 *  - format... - j.w.,
 */
char *format_string(const char *format, ...)
{
        va_list ap;
        char *tmp;

        va_start(ap, format);
        tmp = va_format_string(format, ap);
        va_end(ap);

        return tmp;
}

/*
 * print_window()
 *
 * wy¶wietla tekst w podanym oknie.
 *
 *  - target - nazwa okna
 *  - session - sesja, w której wy¶wietlamy
 *  - separate - czy niezbêdne jest otwieranie nowego okna?
 *  - theme, ... - tre¶æ.
 */
void print_window(const char *target, session_t *session, int separate, const char *theme, ...)
{
        char *tmp, *stmp, *line, *prompt = NULL, *newtarget = NULL;
        va_list ap;

        /* je¶li podamy nazwê z zasobem
         * i nie ma otwartego okna, a jest otwarte dla nazwy bez
         * zasobem to wrzucamy tam. je¶li mamy otwarte okno dla zasobu,
         * a przychodzi z innego, otwieramy nowe. */

        if (!window_find_s(session, target)) {
                const char *res;
                userlist_t *u;

                if ((res = xstrchr(target, '/'))) {
                        newtarget = xstrdup(target);
                        *(xstrchr(newtarget, '/')) = 0;
                        u = userlist_find(session, target);
                        /* XXX cza dorobiæ, szefie */
                } else {
                        u = userlist_find(session, target);

                        if (u && window_find_s(session, u->uid))
                                newtarget = xstrdup(u->uid);
                        else if (u && u->nickname)
                                newtarget = xstrdup(u->nickname);
                }
        }

        if (newtarget)
                target = newtarget;

        if (!target)
                target = "__current";

        va_start(ap, theme);
        tmp = stmp = va_format_string(format_find(theme), ap);
        va_end(ap);

        while ((line = split_line(&tmp))) {
                char *p;

                if ((p = xstrstr(line, "\033[00m"))) {
                        xfree(prompt);
                        if (p != line)
                                prompt = xstrmid(line, 0, (int) (p - line) + 5);
                        else
                                prompt = NULL;
                        line = p;
                }

                if (prompt) {
                        char *tmp = saprintf("%s%s", prompt, line);
                        window_print(target, session, separate, fstring_new(tmp));
                        xfree(tmp);
                } else
                        window_print(target, session, separate, fstring_new(line));
        }

        xfree(prompt);
        xfree(stmp);
        xfree(newtarget);
}

/*
 * theme_cache_reset()
 *
 * usuwa cache'owane prompty. przydaje siê przy zmianie theme'u.
 */
void theme_cache_reset()
{
        xfree(prompt_cache);
        xfree(prompt2_cache);
        xfree(error_cache);

        prompt_cache = prompt2_cache = error_cache = NULL;
        timestamp_cache = NULL;
}

/*
 * format_add()
 *
 * dodaje dan± formatkê do listy.
 *
 *  - name - nazwa,
 *  - value - warto¶æ,
 *  - replace - je¶li znajdzie, to zostawia (=0) lub zamienia (=1).
 */
int format_add(const char *name, const char *value, int replace)
{
        struct format f;
        list_t l;
        int hash;

        if (!name || !value)
                return -1;

        hash = ekg_hash(name);

        if (hash == ekg_hash("no_prompt_cache") && !xstrcasecmp(name, "no_prompt_cache")) {
                no_prompt_cache = 1;
                return 0;
        }

        for (l = formats; l; l = l->next) {
                struct format *g = l->data;

                if (hash == g->name_hash && !xstrcasecmp(name, g->name)) {
                        if (replace) {
                                xfree(g->value);
                                g->value = xstrdup(value);
                        }

                        return 0;
                }
        }

        f.name = xstrdup(name);
        f.name_hash = ekg_hash(name);
        f.value = xstrdup(value);

        return (list_add(&formats, &f, sizeof(f)) ? 0 : -1);
}

/*
 * format_remove()
 *
 * usuwa formatkê o danej nazwie.
 *
 *  - name.
 */
int format_remove(const char *name)
{
        list_t l;

        if (!name)
                return -1;

        for (l = formats; l; l = l->next) {
                struct format *f = l->data;

                if (!xstrcasecmp(f->name, name)) {
                        xfree(f->value);
                        xfree(f->name);
                        list_remove(&formats, f, 1);

                        return 0;
                }
        }

        return -1;
}

/*
 * theme_open() // funkcja wewnêtrzna
 *
 * próbuje otworzyæ plik, je¶li jeszcze nie jest otwarty.
 *
 *  - prevfd - deskryptor z poprzedniego wywo³ania,
 *  - prefix - ¶cie¿ka,
 *  - filename - nazwa pliku.
 */
static FILE *theme_open(FILE *prevfd, const char *prefix, const char *filename)
{
        char buf[PATH_MAX];
        int save_errno;
        FILE *f;

        if (prevfd)
                return prevfd;

        if (prefix)
                snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
        else
                snprintf(buf, sizeof(buf), "%s", filename);

        if ((f = fopen(buf, "r")))
                return f;

        if (prefix)
                snprintf(buf, sizeof(buf), "%s/%s.theme", prefix, filename);
        else
                snprintf(buf, sizeof(buf), "%s.theme", filename);

        save_errno = errno;

        if ((f = fopen(buf, "r")))
                return f;

        if (errno == ENOENT)
                errno = save_errno;

        return NULL;
}

/*
 * theme_read()
 *
 * wczytuje opis wygl±du z podanego pliku.
 *
 *  - filename - nazwa pliku z opisem,
 *  - replace - czy zastêpowaæ istniej±ce wpisy.
 *
 * zwraca 0 je¶li wszystko w porz±dku, -1 w przypadku b³êdu.
 */
int theme_read(const char *filename, int replace)
{
        char *buf;
        FILE *f = NULL;

        if (!filename) {
                filename = prepare_path("default.theme", 0);
                if (!filename || !(f = fopen(filename, "r")))
                        return -1;
        } else {
                char *fn = xstrdup(filename), *tmp;

                if ((tmp = xstrchr(fn, ',')))
                        *tmp = 0;

                errno = ENOENT;
                f = NULL;

                if (!xstrchr(filename, '/')) {
                        f = theme_open(f, prepare_path("", 0), fn);
                        f = theme_open(f, prepare_path("themes", 0), fn);
                        f = theme_open(f, DATADIR "/themes", fn);
                }

                xfree(fn);

                if (!f)
                        return -1;
        }

        theme_free();
        theme_init();
//      ui_event("theme_init");

        while ((buf = read_file(f))) {
                char *value, *p;

                if (buf[0] == '#') {
                        xfree(buf);
                        continue;
                }

                if (!(value = xstrchr(buf, ' '))) {
                        xfree(buf);
                        continue;
                }

                *value++ = 0;

                for (p = value; *p; p++) {
                        if (*p == '\\') {
                                if (!*(p + 1))
                                        break;
                                if (*(p + 1) == 'n')
                                        *p = '\n';
                                memmove(p + 1, p + 2, xstrlen(p + 1));
                        }
                }

                if (buf[0] == '-')
                        format_remove(buf + 1);
                else
                        format_add(buf, value, replace);

                xfree(buf);
        }

        fclose(f);

        theme_cache_reset();

        return 0;
}

/*
 * theme_free()
 *
 * usuwa formatki z pamiêci.
 */
void theme_free()
{
        list_t l;

        for (l = formats; l; l = l->next) {
                struct format *f = l->data;

                xfree(f->name);
                xfree(f->value);
        }

        list_destroy(formats, 1);
        formats = NULL;

        theme_cache_reset();
}

void theme_plugins_init()
{
        list_t l;

        if (!plugins)
                return;

        for (l = plugins; l; l = l->next) {
                plugin_t *p = l->data;

                if (!p)
                        continue;

                plugin_theme_reload(p);
        }
}

/*
 * theme_init()
 *
 * ustawia domy¶lne warto¶ci formatek.
 */
void theme_init()
{
        theme_cache_reset();

        /* wykorzystywane w innych formatach */
        format_add("prompt", "%K:%g:%G:%n", 1);
        format_add("prompt,speech", " ", 1);
        format_add("prompt2", "%K:%c:%C:%n", 1);
        format_add("prompt2,speech", " ", 1);
        format_add("error", "%K:%r:%R:%n", 1);
        format_add("error,speech", "b³±d!", 1);
        format_add("timestamp", "%T", 1);
        format_add("timestamp,speech", " ", 1);

        /* prompty dla ui-readline */
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

        /* prompty i statusy dla ui-ncurses */
        format_add("ncurses_prompt_none", "", 1);
        format_add("ncurses_prompt_query", "[%1] ", 1);
        format_add("statusbar", " %c(%w%{time}%c)%w %c(%w%{?session %{?away %G}%{?avail %Y}%{?chat %W}%{?dnd %K}%{?xa %g}%{?invisible %C}%{?notavail %r}%{session}}%{?!session ---}%c) %{?window (%wwin%c/%w%{window}}%{?query %c:%W%{query}}%{?debug %c(%Cdebug}%c)%w%{?activity  %c(%wact%c/%W}%{activity}%{?activity %c)%w}%{?mail  %c(%wmail%c/%w}%{mail}%{?mail %c)}%{?more  %c(%Gmore%c)}", 1);
        format_add("header", " %{?query %c(%{?query_away %w}%{?query_avail %W}%{?query_invisible %K}%{?query_notavail %k}%{query}%{?query_descr %c/%w%{query_descr}}%c) %{?query_ip (%wip%c/%w%{query_ip}%c)}}%{?!query %c(%wekg2%c/%w%{version}%c) (%w%{url}%c)}", 1);
        format_add("statusbar_act_important", "%W", 1);
        format_add("statusbar_act", "%K", 1);
        format_add("statusbar_timestamp", "%H:%M", 1);

        /* dla funkcji format_user() */
        format_add("known_user", "%T%1%n/%2", 1);
        format_add("known_user,speech", "%1", 1);
        format_add("unknown_user", "%T%1%n", 1);

        /* czêsto wykorzystywane, ró¿ne, przydatne itd. */
        format_add("none", "%1\n", 1);
        format_add("generic", "%> %1\n", 1);
        format_add("generic_bold", "%> %T%1%n\n", 1);
        format_add("generic2", "%) %1\n", 1);
        format_add("generic2_bold", "%) %T%1%n\n", 1);
        format_add("generic_error", "%! %1\n", 1);
        format_add("debug", "%n%1\n", 1);
	format_add("value_none", _("(none)"), 1);
        format_add("not_enough_params", _("%! Too few parameters. Try %Thelp %1%n\n"), 1);
        format_add("invalid_params", _("%! Invalid parameters. Try %Thelp %1%n\n"), 1);
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

        /* mail */
        format_add("new_mail_one", _("%) You got one email\n"), 1);
        format_add("new_mail_two_four", _("%) You got %1 new emails\n"), 1);
        format_add("new_mail_more", _("%) You got %1 new email\n"), 1);

        /* add, del */
        format_add("user_added", _("%> (%2) Added %T%1%n to roster\n"), 1);
        format_add("user_deleted", _("%) (%2) Removed %T%1%n from roster\n"), 1);
        format_add("user_cleared_list", _("%) (%1) Roster cleared\n"), 1);
        format_add("user_exists", _("%! (%2) %T%1%n already in roster\n"), 1);
        format_add("user_exists_other", _("%! (%3) %T%1%n already in roster as %2\n"), 1);

        /* zmiany stanu */
        format_add("away", _("%> (%1) Status changed to %Gaway%n\n"), 1);
        format_add("away_descr", _("%> (%3) Status changed to %Gaway%n: %T%1%n%2\n"), 1);
        format_add("back", _("%> (%1) Status changed to %Yavailable%n\n"), 1);
        format_add("back_descr", _("%> (%3) Status changed to %Yavailable%n: %T%1%n%2%n\n"), 1);
        format_add("invisible", _("%> (%1) Status changed to %cinvisible%n\n"), 1);
        format_add("invisible_descr", _("%> (%3) Status changed to %cinvisible%n: %T%1%n%2\n"), 1);
        format_add("dnd", _("%> (%1) Status changed to %Bdo not disturb%n\n"), 1);
        format_add("dnd_descr", _("%> (%3) Status changed to %Bdo not disturb%n: %T%1%n%2\n"), 1);
        format_add("chat", _("%> (%1) Status changed to %Wfree for chat%n\n"), 1);
        format_add("chat_descr", _("%> (%3) Status changed to %Wfree for chat%n: %T%1%n%2%n\n"), 1);
        format_add("xa", _("%> (%1) Status changed to %gextended away%n\n"), 1);
        format_add("xa_descr", _("%> (%3) Status changed to %gextended away%n: %T%1%n%2%n%n\n"), 1);
        format_add("private_mode_is_on", _("% (%1) Friends only mode is on\n"), 1);
        format_add("private_mode_is_off", _("%> (%1) Friends only mode is off\n"), 1);
        format_add("private_mode_on", _("%) (%1) Turned on ,,friends only'' mode\n"), 1);
        format_add("private_mode_off", _("%> (%1) Turned off ,,friends only'' mode\n"), 1);
        format_add("private_mode_invalid", _("%! Invalid value'\n"), 1);
        format_add("descr_too_long", _("%! Description longer than maximum %T%1%n characters\nDescr: %B%2%b%3%n\n"), 1);

        format_add("auto_away", _("%> (%1) Auto %Gaway%n\n"), 1);
        format_add("auto_away_descr", _("%> (%2) Auto %Gaway%n: %T%1%n\n"), 1);
        format_add("auto_back", _("%> (%1) Auto back - %Yavailable%n\n"), 1);
        format_add("auto_back_descr", _("%> (%2) Auto back - %Yavailable%n: %T%1%n\n"), 1);

        /* pomoc */
        format_add("help", "%> %T%1%n%2 - %3\n", 1);
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
        format_add("help_session_body", "%> %|%1\n", 1);
        format_add("help_session_file_not_found", _("%! Can't find variables descriptions for %T%1%n plugin (incomplete installation)\n"), 1);
        format_add("help_session_var_not_found", _("%! Cant find description of %T%1%n variable\n"), 1);
        format_add("help_session_header", _("%> %T%1%n (%2, default value: %3)\n%>\n"), 1);
        format_add("help_session_footer", "", 1);




        /* ignore, unignore, block, unblock */
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

	/* contact list */
        format_add("list_empty", _("%! Roster is empty\n"), 1);
        format_add("list_avail", _("%> %1 %Y(available)%n %b%3:%4%n\n"), 1);
        format_add("list_avail_descr", _("%> %1 %Y(available: %n%5%Y)%n %b%3:%4%n\n"), 1);
        format_add("list_away", _("%> %1 %G(away)%n %b%3:%4%n\n"), 1);
        format_add("list_away_descr", _("%> %1 %G(away: %n%5%G)%n %b%3:%4%n\n"), 1);
        format_add("list_dnd", _("%> %1 %B(do not disturb)%n %b%3:%4%n\n"), 1);
        format_add("list_dnd_descr", _("%> %1 %G(do not disturb: %5%G)%n %b%3:%4%n\n"), 1);
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
        format_add("list_blocked", _("%> %1 %m(blocking)%n\n"), 1);
        format_add("list_unknown", "%> %1\n", 1);
        format_add("modify_offline", _("%> %1 will not see aout status\n"), 1);
        format_add("modify_online", _("%> %1 will se aout status\n"), 1);
        format_add("modify_done", _("%> Modified item in roster\n"), 1);

        /* lista kontaktów z boku ekranu */
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
        format_add("contacts_footer", "", 1);
        format_add("contacts_footer_group", "", 1);
        format_add("contacts_metacontacts_footer", "", 1);
        format_add("contacts_vertical_line_char", "|", 1);
        format_add("contacts_horizontal_line_char", "-", 1);

        /* we are saying goodbye and we are saving configuration */
        format_add("quit", _("%> Bye\n"), 1);
        format_add("quit_descr", _("%> Bye: %T%1%n%2\n"), 1);
        format_add("config_changed", _("Save new configuration ? (t-yes/n-no) "), 1);
        format_add("quit_keep_reason", _("You've set keep_reason to save status.\nDo you want to save current description to file (it will be restored upon next EKG exec)? (t-yes/n-no) "), 1);
        format_add("saved", _("%> Configuration saved\n"), 1);
        format_add("error_saving", _("%! There was some error during save\n"), 1);

        /* incoming messages */
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

        /* acks of messages */
        format_add("ack_queued", _("%> Message to %1 will be delivered later\n"), 1);
        format_add("ack_delivered", _("%> Message to %1 delivered\n"), 1);
        format_add("ack_unknown", _("%> Not clear what happened to message to %1\n"), 1);
        format_add("ack_filtered", _("%! %|Message to %1 probably was not delivered. The person is unavailable, but server claims message is delivered. Message could been filtered out (e.g. because of web address in it)\n"), 1);
        format_add("message_too_long", _("%! Message was too long and got shortened\n"), 1);

        /* people are changing their statuses */
	format_add("status_avail", _("%> (%1) %1 is %Yavailable%n\n"), 1);
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

        /* connection with server */
        format_add("connecting", _("%> (%1) Connecting to server %n\n"), 1);
        format_add("conn_failed", _("%! (%2) Connection failure: %1%n\n"), 1);
        format_add("conn_failed_resolving", _("Server not found"), 1);
        format_add("conn_failed_connecting", _("Can not connect ro server"), 1);
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
        format_add("conn_broken", _("%! (%1) Connection broken%n\n"), 1);
        format_add("conn_disconnected", _("%! (%1) Server disconnected%n\n"), 1);
        format_add("not_connected", _("%! (%1) Not connected.%n\n"), 1);
        format_add("not_connected_msg_queued", _("%! (%1) Not connected. Message will be delivered when connected.%n\n"), 1);
        format_add("wrong_id", _("%! (%1) Wrong session id.%n\n"), 1);
        format_add("inet_addr_failed", _("%! (%1) Invalid \"server\".%n\n"), 1);
        format_add("invalid_local_ip", _("%! (%1) Invalid local address. I'm clearing %Tlocal_ip%n session variable\n"), 1);

        /* obs³uga motywów */
        format_add("theme_loaded", "%> Wczytano motyw %T%1%n\n", 1);
        format_add("theme_default", "%> Ustawiono domy¶lny motyw\n", 1);
        format_add("error_loading_theme", "%! B³±d podczas ³adowania motywu: %1\n", 1);

        /* zmienne, konfiguracja */
        format_add("variable", "%> %1 = %2\n", 1);
        format_add("variable_not_found", _("%! Unknown variable: %T%1%n\n"), 1);
        format_add("variable_invalid", _("%! Invalid session variable value\n"), 1);
        format_add("no_config", _("%! Incomplete configuration. Use:\n%!   %Tsession -a <gg:gg-number/jid:jabber-id>%n\n%!   %Tsession password <password>%n\n%!   %Tsave%n\n%! And then:\n%!   %Tconnect%n\n%! If you don't have uid, use:\n%!   %Tregister <e-mail> <password>%n\n\n%> %|Query windows will be created automatically. To switch windows press %TAlt-number%n or %TEsc%n and then number. To start conversation use %Tquery%n. To add some to roster use %Tadd%n. All key shortcuts are described in %TREADME%n. There is also %Thelp%n command. Rememer about prefixes before UID, for example %Tgg:<no>%n. \n\n"), 2);
        format_add("no_config,speech", _("incomplete configuration. enter session -a, and then gg: gg-number, or jid: jabber id, then session password and your password. enter save to save. enter connect to connect. if you dont have UID enter register, space, e-mail and password. blah blah blah"), 1);
        format_add("error_reading_config", _("%! Error reading configuration file: %1\n"), 1);
        format_add("config_read_success", _("%> Opened configuration file %T%1%n\n"), 1);
        format_add("config_line_incorrect", _("%! Invalid line '%T%1%n', skipping\n"), 1);
        format_add("autosaved", _("%> Autosaved settings\n"), 1);

        /* rejestracja nowego numeru */
        format_add("register", _("%> Registration successful. Your number: %T%1%n\n"), 1);
        format_add("register_failed", _("%! Error during registration: %1\n"), 1);
        format_add("register_pending", _("%! Registration in progress\n"), 1);
        format_add("register_timeout", _("%! Registration timed out\n"), 1);
        format_add("registered_today", _("%! Already registered. Do not abuse\n"), 1);

	/* deleting user's account from public catalog */
        format_add("unregister", _("%> Account removed\n"), 1);
        format_add("unregister_timeout", _("%! Account removal timed out\n"), 1);
        format_add("unregister_bad_uin", _("%! Unknown uin: %T%1%n\n"), 1);
        format_add("unregister_failed", _("%! Error while deleting account: %1\n"), 1);

        /* password remind */
	format_add("remind", _("%> Password sent\n"), 1);
        format_add("remind_failed", _("%! Error while sending password: %1\n"), 1);
        format_add("remind_timeout", _("%! Password sending timed out\n"), 1);

        /* password change */
	format_add("passwd", _("%> Password changed\n"), 1);
        format_add("passwd_failed", _("%! Error while changing password: %1\n"), 1);
        format_add("passwd_timeout", _("%! Password changing timed out\n"), 1);

        /* changing information in public catalog */
	format_add("change", _("%> Informations in public directory chenged\n"), 1);
        format_add("change_failed", _("%! Error while changing informations in public directory\n"), 1);

	/* users finding */
        format_add("search_failed", _("%! Error while search: %1\n"), 1);
        format_add("search_not_found", _("%! Not found\n"), 1);
        format_add("search_no_last", _("%! Last search returned no result\n"), 1);
        format_add("search_no_last_nickname", _("%! No nickname in last search\n"), 1);
        format_add("search_stopped", _("%> Search stopped\n"), 1);

        /* 1 uin, 2 name, 3 nick, 4 city, 5 born, 6 gender, 7 active */
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

        /* exec */
        format_add("process", "%> %(-5)1 %2\n", 1);
        format_add("no_processes", _("%! There are no running procesees\n"), 1);
        format_add("process_exit", _("%> Proces %1 (%2) exited with %3 status\n"), 1);
        format_add("exec", "%1\n",1);   /* lines are ended by \n */
        format_add("exec_error", _("%! Error running process : %1\n"), 1);
        format_add("exec_prompt", "$ %1\n", 1);

        /* detailed info about user */
	format_add("user_info_header", "%K.--%n %T%1%n/%2 %K--- -- -%n\n", 1);
        format_add("user_info_nickname", _("%K| %nNickname: %T%1%n\n"), 1);
        format_add("user_info_name", _("%K| %nName: %T%1 %2%n\n"), 1);
        format_add("user_info_status", _("%K| %nStatus: %T%1%n\n"), 1);
        format_add("user_info_status_time_format", "%Y-%m-%d %H:%M", 1);
        format_add("user_info_status_time", _("%K| %nCurrent status since: %T%1%n\n"), 1);
        format_add("user_info_auth_type", _("%K| %nSubscription type: %T%1%n\n"), 1);
        format_add("user_info_block", _("%K| %nBlocked\n"), 1);
        format_add("user_info_offline", _("%K| %nDon't see status\n"), 1);
        format_add("user_info_not_in_contacts", _("%K| %nDon't have us in roster\n"), 1);
        format_add("user_info_firewalled", _("%K| %nFirewalled/NATed\n"), 1);
        format_add("user_info_ip", _("%K| %nAddress: %T%1%n\n"), 1);
        format_add("user_info_mobile", _("%K| %nTelephone: %T%1%n\n"), 1);
        format_add("user_info_groups", _("%K| %nGroups: %T%1%n\n"), 1);
        format_add("user_info_never_seen", _("%K| %nNever seen\n"), 1);
        format_add("user_info_last_seen", _("%K| %nLast seen: %T%1%n\n"), 1);
        format_add("user_info_last_seen_time", "%Y-%m-%d %H:%M", 1);
        format_add("user_info_last_ip", _("%K| %nLast address: %T%1%n\n"), 1);
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
        format_add("user_info_chat", _("%Wfree for chat%n"), 1);
        format_add("user_info_chat_descr", _("%Wfree for chat%n %K(%n%2%K)%n"), 1);
        format_add("user_info_error", _("%m error%n"), 1);
        format_add("user_info_error_descr", _("%merror%n %K(%n%2%K)%n"), 1);
        format_add("user_info_xa", _("%gextended away%n"), 1);
        format_add("user_info_xa_descr", _("%gextended away%n %K(%n%2%K)%n"), 1);
        format_add("user_info_blocked", _("%mblocking%n"), 1);
        format_add("user_info_blocked_descr", _("%mblocking%n %K(%n%2%K)%n"), 1);
        format_add("user_info_unknown", _("%Munknown%n"), 1);

        /* grupy */
        format_add("group_members", _("%> %|Group %T%1%n: %2\n"), 1);
        format_add("group_member_already", _("%! %1 already in group %T%2%n\n"), 1);
        format_add("group_member_not_yet", _("%! %1 not in group %T%2%n\n"), 1);
        format_add("group_empty", _("%! Group %T%1%n is empty\n"), 1);

        /* status */
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

        /* aliasy */
        format_add("aliases_list_empty", _("%! No aliases\n"), 1);
        format_add("aliases_list", "%> %T%1%n: %2\n", 1);
        format_add("aliases_list_next", "%> %3  %2\n", 1);
        format_add("aliases_add", _("%> Created alias %T%1%n\n"), 1);
        format_add("aliases_append", _("%> Added to alias %T%1%n\n"), 1);
        format_add("aliases_del", _("%) Removed alias %T%1%n\n"), 1);
        format_add("aliases_del_all", _("%) Removed all aliases\n"), 1);
        format_add("aliases_exist", _("%! Alias %T%1%n already exists\n"), 1);
        format_add("aliases_noexist", _("%! Alias %T%1%n do not exist\n"), 1);
        format_add("aliases_command", _("%! %T%1%n is internal command\n"), 1);
        format_add("aliases_not_enough_params", _("%! Alias %T%1%n requires more parameters\n"), 1);

        /* dcc connections */
	format_add("dcc_attack", _("%! To many direct connections, last from %1\n"), 1);
        format_add("dcc_limit", _("%! %|Direct connections count over limit, so they got disabled. To enable them use %Tset dcc 1% and reconnect. Limit is controlled by %Tdcc_limit%n variable.\n"), 1);
        format_add("dcc_create_error", _("%! Can't turn on direct connections: %1\n"), 1);
        format_add("dcc_error_network", _("%! Error transmitting with %1\n"), 1);
        format_add("dcc_error_refused", _("%! Connection to %1 refused\n"), 1);
        format_add("dcc_error_unknown", _("%! Uknown direct connection error\n"), 1);
        format_add("dcc_error_handshake", _("%! Can't connect with %1\n"), 1);
        format_add("dcc_user_aint_dcc", _("%! %1 don't have direct connections enabled\n"), 1);
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

        format_add("dcc_done_get", _("%> Finished receiving filr %T%2%n from %1\n"), 1);
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

        /* query */
        format_add("query_started", _("%) (%2) Query with %T%1%n started\n"), 1);
        format_add("query_started_window", _("%) Press %TAlt-G%n to ignore, %TAlt-K%n to close window\n"), 1);
        format_add("query_finished", _("%) (%2) Finished query with %T%1%n\n"), 1);
        format_add("query_exist", _("%! Query with %T%1%n already in window no %T%2%n\n"), 1);

        /* zdarzenia */
        format_add("events_list_empty", _("%! No events\n"), 1);
        format_add("events_list_header", "", 1);
        format_add("events_list", "%> %5 on %1 %3 %4 - prio %2\n", 1);
        format_add("events_add", _("%> Added event %T%1%n\n"), 1);
        format_add("events_del", _("%) Removed event %T%1%n\n"), 1);
        format_add("events_del_all", _("%) Removed all events\n"), 1);
        format_add("events_exist", _("%! Event %T%1%n exist for %2\n"), 1);
        format_add("events_del_noexist", _("%! Event %T%1%n do not exist\n"), 1);

        /* contact list from the server */
	format_add("userlist_put_ok", _("%> Roster saved on server\n"), 1);
        format_add("userlist_put_error", _("%! Error sending roster\n"), 1);
        format_add("userlist_get_ok", _("%> Roster read from server\n"), 1);
        format_add("userlist_get_error", _("%! Error getting roster\n"), 1);
        format_add("userlist_clear_ok", _("%) Removed roster from server\n"), 1);
        format_add("userlist_clear_error", _("%! Error removing roster\n"), 1);

        /* szybka lista kontaktów pod F2 */
        format_add("quick_list", "%)%1\n", 1);
        format_add("quick_list,speech", _("roster:"), 1);
        format_add("quick_list_avail", " %Y%1%n", 1);
        format_add("quick_list_avail,speech", _("%1 is available"), 1);
        format_add("quick_list_away", " %G%1%n", 1);
        format_add("quick_list_away,speech", _("%1 is away"), 1);
        format_add("quick_list_invisible", " %c%1%n", 1);

        /* window */
        format_add("window_add", _("%) New window created\n"), 1);
        format_add("window_noexist", _("%! Choosend window do not exist\n"), 1);
        format_add("window_doesnt_exist", _("%! Window %T%1%n do not exist\n"), 1);
        format_add("window_no_windows", _("%! Can not close last window\n"), 1);
        format_add("window_del", _("%) Window closed\n"), 1);
        format_add("windows_max", _("%! Window limit exhausted\n"), 1);
        format_add("window_list_query", _("%) %1: query with %T%2%n\n"), 1);
        format_add("window_list_nothing", _("%) %1 no query\n"), 1);
        format_add("window_list_floating", _("%) %1: floating %4x%5 in %2,%3 %T%6%n\n"), 1);
        format_add("window_id_query_started", _("%) Query with %T%2%n started in %T%1%n\n"), 1);
        format_add("window_kill_status", _("%! Can't close status window!\n"), 1);

        /* bind */
        format_add("bind_seq_incorrect", _("%! Sequence %T%1%n is invalid\n"), 1);
        format_add("bind_seq_add", _("%> Sequence %T%1%n added\n"), 1);
        format_add("bind_seq_remove", _("%) Sequence %T%1%n removed\n"), 1);
        format_add("bind_seq_list", "%> %1: %T%2%n\n", 1);
        format_add("bind_seq_exist", _("%! Sequence %T%1%n is already bound\n"), 1);
        format_add("bind_seq_list_empty", _("%! No bound actions\n"), 1);
        format_add("bind_doesnt_exist", _("%! Can't open file %T%1%n\n"), 1);
        format_add("bind_press_key", _("%! Press key(s) which should be binded\n"), 1);
        format_add("bind_added", _("%> Binding acomplished\n"), 1);

        /* at */
        format_add("at_list", "%> %1, %2, %3 %K(%4)%n %5\n", 1);
        format_add("at_added", _("%> Created plan %T%1%n\n"), 1);
        format_add("at_deleted", _("%) Removed plan %T%1%n\n"), 1);
        format_add("at_deleted_all", _("%) Removed user's plans\n"), 1);
        format_add("at_exist", _("%! Plan %T%1%n already exists\n"), 1);
        format_add("at_noexist", _("%! Plan %T%1%n do not exists\n"), 1);
        format_add("at_empty", _("%! No plans\n"), 1);
        format_add("at_timestamp", "%d-%m-%Y %H:%M", 1);
        format_add("at_back_to_past", _("%! If time travel were possible...\n"), 1);

        /* timer */
        format_add("timer_list", "%> %1, %2s, %3 %K(%4)%n %T%5%n\n", 1);
        format_add("timer_added", _("%> Created timer %T%1%n\n"), 1);
        format_add("timer_deleted", _("%) Removed timer  %T%1%n\n"), 1);
        format_add("timer_deleted_all", _("%) Removed user's timers\n"), 1);
        format_add("timer_exist", _("%! Timer %T%1%n already exists\n"), 1);
        format_add("timer_noexist", _("%! Timer %T%1%n does not exists\n"), 1);
        format_add("timer_empty", _("%! No timers\n"), 1);

        /* last */
        format_add("last_list_in", "%) %Y <<%n [%1] %2 %3\n", 1);
        format_add("last_list_out", "%) %G >>%n [%1] %2 %3\n", 1);
        format_add("last_list_empty", _("%! No messages logged\n"), 1);
        format_add("last_list_empty_nick", _("%! No messages from %T%1%n logged\n"), 1);
        format_add("last_list_timestamp", "%d-%m-%Y %H:%M", 1);
        format_add("last_list_timestamp_today", "%H:%M", 1);
        format_add("last_clear_uin", _("%) Messages from %T%1%n cleared\n"), 1);
        format_add("last_clear", _("%) All messages cleared\n"), 1);

        /* queue */
        format_add("queue_list_timestamp", "%d-%m-%Y %H:%M", 1);
        format_add("queue_list_message", "%) %G >>%n [%1] %2 %3\n", 1);
        format_add("queue_clear", _("%) Message queue cleared\n"), 1);
        format_add("queue_clear_uid", _("%) Message queue for %T%1%n cleared\n"), 1);
        format_add("queue_wrong_use", _("%! Command works only when disconected\n"), 1);
        format_add("queue_empty", _("%! Messaged queue is empty\n"), 1);
        format_add("queue_empty_uid", _("%! No messages to %T%1%n in queue\n"), 1);
        format_add("queue_flush", _("%> (%1) Sent messages from queue\n"), 1);

        /* conference */
        format_add("conferences_list_empty", _("%! No conference\n"), 1);
        format_add("conferences_list", "%> %T%1%n: %2\n", 1);
        format_add("conferences_list_ignored", _("%> %T%1%n: %2 (%yingored%n)\n"), 1);
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

        /* shared by http */
	format_add("http_failed_resolving", _("Server not found"), 1);
        format_add("http_failed_connecting", _("Can not connect ro server"), 1);
        format_add("http_failed_reading", _("Server disconnected"), 1);
        format_add("http_failed_writing", _("Server disconnected"), 1);
        format_add("http_failed_memory", _("No memory"), 1);

#ifdef WITH_PYTHON
        /* python */
        format_add("python_list", "%> %1\n", 1);
        format_add("python_list_empty", _("%! No scripts loaded\n"), 1);
        format_add("python_removed", _("%) Script removed\n", 1);
        format_add("python_need_name", _("%! Enter script name\n", 1);
        format_add("python_not_found", _("%! Not found script: %T%1%n\n", 1);
        format_add("python_wrong_location", _("%! Script should be located in  %T%1%n\n", 1);
#endif
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
        format_add("session_doesnt_exist", _("%! Sesion %T%1%n does not exists\n"), 1); /* uid */
        format_add("session_added", _("%> Created session %T%1%n\n"), 1); /* uid */
        format_add("session_removed", _("%> Removed session %T%1%n\n"), 1); /* uid */
        format_add("session_format", "%T%1%n", 1);
        format_add("session_format_alias", "%T%1%n/%2", 1);
        format_add("session_cannot_change", _("%! Can't change session in query window%n\n"), 1);

        format_add("metacontact_list", "%> %T%1%n", 1);
        format_add("metacontact_list_empty", "%! Nie ma ¿adnych metakontaktów\n", 1);
        format_add("metacontact_exists", "%! Metakontakt %T%1%n ju¿ istnieje\n", 1);
        format_add("metacontact_added", "%> Utworzono metakontakt %T%1%n\n", 1);
        format_add("metacontact_removed", "%> Usuniêto metakontakt %T%1%n\n", 1);
        format_add("metacontact_doesnt_exist", "%! Metakontakt %T%1%n nie istnieje\n", 1);
        format_add("metacontact_added_item", "%> Dodano %T%1/%2%n do %T%3%n\n", 1);
        format_add("metacontact_removed_item", "%> Usuniêto %T%1/%2%n z %T%3%n\n", 1);
        format_add("metacontact_item_list_header", "", 1);
        format_add("metacontact_item_list", "%> %T%1/%2 (%3)%n - prio %T%4%n\n", 1);
        format_add("metacontact_item_list_empty", "%! Metakontakt jest pusty\n", 1);
        format_add("metacontact_item_list_footer", "", 1);
        format_add("metacontact_item_doesnt_exist", "%! Kontakt %T%1/%2%n nie istnieje\n", 1);
        format_add("metacontact_info_header", "%K.--%n Metakontakt %T%1%n %K--- -- -%n\n", 1);
        format_add("metacontact_info_status", "%K| %nStan: %T%1%n\n", 1);
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
        format_add("metacontact_info_blocked", _("%mblocking%n"), 1);
        format_add("metacontact_info_blocked_descr", _("%mblocking%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_unknown", _("%Munknown%n"), 1);

        format_add("plugin_already_loaded", _("%! Plugin %T%1%n already loaded%n.\n"), 1);
        format_add("plugin_doesnt_exist", _("%! Plugin %T%1%n can not be found%n\n"), 1);
        format_add("plugin_incorrect", _("%! Plugin %T%1%n is not correct EKG2 plugin%n\n"), 1);
        format_add("plugin_not_initialized", _("%! Plugin %T%1%n not initialized correctly%n\n"), 1);
        format_add("plugin_unload_ui", _("%! Plugin %T%1%n is an UI plugin and can't ne unloaded%n\n"), 1);
        format_add("plugin_loaded", _("%> Plugin %T%1%n loaded%n\n"), 1);
        format_add("plugin_unloaded", _("%> Plugin %T%1%n unloaded%n\n"), 1);

        theme_plugins_init();
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
