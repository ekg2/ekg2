#include "ekg2.h"
#include <stdio.h>
#include <string.h>

#ifdef HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#else
#	include <readline.h>
#endif

#include <ekg/completion.h>

extern char **completion_matches();

static char *rl_strndup(gchar *s, gssize n) {
	static GString *buf = NULL;
	gchar *rec;

		/* XXX: API for stated-size recoding */
	if (G_UNLIKELY(!buf))
		buf = g_string_sized_new(G_LIKELY(n != -1) ? n+1 : 16);
	if (G_LIKELY(n == -1))
		g_string_assign(buf, s);
	else {
		g_string_truncate(buf, 0);
		g_string_append_len(buf, s, n);
	}

	rec = ekg_recode_to_locale(buf->str);
	if (G_LIKELY(g_mem_is_system_malloc()))
		return rec;
	else {
		gsize len = strlen(rec) + 1;
		char *out = malloc(len);
		
		memcpy(out, rec, len);
		g_free(rec);
		return out;
	}
}

char *empty_generator(const char *text, int state) {
	return NULL;
}

char *one_and_only = NULL;

char *one_generator(char *text, int state) {
	return state ? NULL : one_and_only;
}

char *multi_generator(char *text, int state) {
	char *ret;

	if (!ekg2_completions)
		return NULL;

	if (!*ekg2_completions) {
		ekg2_complete_clear();
		ekg2_completions = NULL;
		return NULL;
	}

	ret = *ekg2_completions;
	ekg2_completions++;

	return rl_strndup(ret, -1);
}

/*locale*/ char **my_completion(/*locale*/ const char *text, int start, int end) {
	gchar *buffer;
	GString *buf = g_string_sized_new(80);
	int i, n, e0=end, in_quote, out_quote;

	ekg2_complete_clear();

	buffer = ekg_recode_from_locale(rl_line_buffer);
		/* XXX: start & end? */
	g_string_assign(buf, buffer);
	g_free(buffer);

	buffer = buf->str;
	if ((in_quote = (start && buffer[start-1] == '"'))) start--;

	char *p1, *p2;

	/* Multiple spaces confuse `ekg2_complete`. Remove them.
	 * The following loop performs `s/ +/ /g` on `buffer[0:end]` and adjusts
	 * `start` and `end` variables accordingly. */
	for (p1 = p2 = buffer; *p1; p1++) {
		*p1 = *p2++;
		if ((*p1 != ' ') || (p1 >= buffer+end)) continue;
		while (*(p2) == ' ') {
			start--;
			end--;
			p2++;
		}
	}

	ekg2_complete(&start, &end, buf->str, buf->allocated_len);

	out_quote = (buffer[start] == '"');

	if (e0!=end || (out_quote ^ in_quote)) {

		if ((n=g_strv_length(ekg2_completions)) == 0) {
			if (in_quote && out_quote) start++;
			n = end - start - 1;
			if (n && out_quote && in_quote) n--;
			if (n && ' ' == buffer[start+n-1]) n--;
			one_and_only = rl_strndup(buffer+start, n);

			g_string_free(buf, TRUE);
			return completion_matches(text, one_generator);
		}

		for(i=0;i<n;i++) {
			if (ekg2_completions[i][0] != '"')
				continue;
			gchar *tmp = g_strndup(ekg2_completions[i]+1, xstrlen(ekg2_completions[i])-2);
			g_free(ekg2_completions[i]);
			ekg2_completions[i] = tmp;
		}

	}

	g_string_free(buf, TRUE);
	return completion_matches(text, multi_generator);
}
