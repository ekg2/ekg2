/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Wojtek Bojdo³ <wojboj@htcon.pl>
 *			    Pawe³ Maziarz <drg@infomex.pl>
 *		  2008-2010 Wies³aw Ochmiñski <wiechu@wiechu.com>
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

#include <ekg/debug.h>
#include <ekg/xmalloc.h>
#include <ekg/stuff.h>

#include "old.h"
#include "spell.h"


#ifdef WITH_ASPELL

/* vars */

int config_aspell;
char *config_aspell_lang;

AspellSpeller *spell_checker = NULL;
static AspellConfig  *spell_config  = NULL;

/*
 * ncurses_spellcheck_init()
 *
 * it inializes dictionary
 */
void ncurses_spellcheck_init(void) {
	AspellCanHaveError *possible_err;

	if (!config_aspell || !config_console_charset || !config_aspell_lang) {
	/* jesli nie chcemy aspella to wywalamy go z pamieci */
		if (spell_checker)	delete_aspell_speller(spell_checker);
		if (spell_config)	delete_aspell_config(spell_config);
		spell_checker = NULL;
		spell_config = NULL;
		debug("Maybe config_console_charset, aspell_lang or aspell variable is not set?\n");
		return;
	}

	print("aspell_init");

	if (spell_checker)	{ delete_aspell_speller(spell_checker);	spell_checker = NULL; }
	if (!spell_config)	spell_config = new_aspell_config();
	aspell_config_replace(spell_config, "encoding", config_console_charset);
	aspell_config_replace(spell_config, "lang", config_aspell_lang);
	possible_err = new_aspell_speller(spell_config);
	/* delete_aspell_config(spell_config); ? */

	if (aspell_error_number(possible_err) != 0) {
		spell_checker = NULL;
		debug("Aspell error: %s\n", aspell_error_message(possible_err));
		print("aspell_init_error", aspell_error_message(possible_err));
		config_aspell = 0;
		delete_aspell_config(spell_config);
		spell_config = NULL;
	} else {
		spell_checker = to_aspell_speller(possible_err);
		print("aspell_init_success");
	}
}

static inline int isalpha_locale(int x) {
#ifdef USE_UNICODE
	return (isalpha(x) || (x > 0x7f));	/* moze i nie najlepsze wyjscie... */
#else
	return isalpha_pl(x);
#endif
}

/*
 * spellcheck()
 *
 * it checks if the given word is correct
 */
void spellcheck(CHAR_T *what, char *where) {
	register int i = 0;	/* licznik */
	register int j = 0;	/* licznik */
	CHAR_T what_j;

	/* Sprawdzamy czy nie mamy doczynienia z / (wtedy nie sprawdzamy reszty ) */
	if (!what || *what == '/')
		return;

	while (what[i] && what[i] != '\n' && what[i] != '\r') {
#if USE_UNICODE
		char *word_mbs;
#endif
		char fillznak = ' ';	/* do wypelnienia where[] (ASPELLCHAR gdy blednie napisane slowo) */

		if ((isalpha_locale(what[i]) && i != 0) || what[i+1] == '\0') {		/* separator/koniec lini/koniec stringu */
			i++;
			continue;
		}

		/* szukamy jakiejs pierwszej literki */
		for (; what[i] && what[i] != '\n' && what[i] != '\r'; i++) {
			if (isalpha_locale(what[i]))
				break;
		}

		/* trochê poprawiona wydajno¶æ */
		if (!what[i] || what[i] == '\n' || what[i] == '\r')
			continue;

		/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu www */
		if ((what[i] == 'h' && what[i + 1] == 't' && what[i + 2] == 't' && what[i + 3] == 'p') &&
			((what[i + 4] == ':' && what[i + 5] == '/' && what[i + 6] == '/') ||
			 (what[i + 4] == 's' && what[i + 5] == ':' && what[i + 6] == '/' && what[i + 7] == '/')))
		{
			while (what[i] && what[i] != ' ' && what[i] != '\n' && what[i] != '\r') i++;
			continue;
		}

		/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu ftp */
		if (what[i] == 'f' && what[i + 1] == 't' && what[i + 2] == 'p' && what[i + 3] == ':' && what[i + 4] == '/' && what[i + 5] == '/')
		{
			while (what[i] && what[i] != ' ' && what[i] != '\n' && what[i] != '\r') i++;
			continue;
		}

		for (j = i; what[j] && what[j] != '\n'; j++) {
			if (!isalpha_locale(what[j]))
				break;
		}

		if (j == i) {		/* Jak dla mnie nie powinno sie wydarzyc. */
			i++;
			continue;
		}

		what_j = what[j];
		what[j] = '\0';

#if USE_UNICODE
		word_mbs = wcs_to_normal(&what[i]);
		if (!userlist_find(session_current, word_mbs))
			fillznak = (aspell_speller_check(spell_checker, word_mbs, -1) == 0) ? ASPELLCHAR : ' ';
		free_utf(word_mbs);
#else
		/* sprawdzamy pisownie tego wyrazu */
		if (!userlist_find(session_current, what+i))
			fillznak = (aspell_speller_check(spell_checker, (char *) &what[i], j - i) == 0) ? ASPELLCHAR : ' ';
#endif
		what[j] = what_j;

		if (fillznak != ASPELLCHAR)
			i = j;
		else
			for (; i < j; i++)
				where[i] = fillznak;
	}
}

#endif
