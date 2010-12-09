#include "ekg2-config.h"
#include <stdio.h>

#ifdef HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#else
#	include <readline.h>
#endif

#include <ekg/completion.h>
#include <ekg/dynstuff.h>
#include <ekg/strings.h>
#include <ekg/xmalloc.h>

extern char **completion_matches();

char *empty_generator(char *text, int state) {
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

	return xstrdup(ret);
}

#define RL_LINE_MAXLEN 2048
/* XXX what is readline input line length limit? */

char **my_completion(char *text, int start, int end) {
	static char buffer[RL_LINE_MAXLEN];
	int i, n, e0=end, in_quote, out_quote;

	ekg2_complete_clear();

	xstrcpy(buffer, rl_line_buffer);

	if ((in_quote = (start && buffer[start-1] == '"'))) start--;

	ekg2_complete(&start, &end, buffer, RL_LINE_MAXLEN);

	out_quote = (buffer[start] == '"');

	if (e0!=end || (out_quote ^ in_quote)) {

		if ((n=array_count(ekg2_completions)) == 0) {
			if (in_quote && out_quote) start++;
			n = end - start - 1;
			if (n && out_quote && in_quote) n--;
			if (n && ' ' == buffer[start+n-1]) n--;
			one_and_only = xstrndup(buffer+start, n);

			return completion_matches(text, one_generator);
		}

		for(i=0;i<n;i++) {
			if (ekg2_completions[i][0] != '"')
				continue;
			char *tmp = xstrndup(ekg2_completions[i]+1, xstrlen(ekg2_completions[i])-2);
			xfree(ekg2_completions[i]);
			ekg2_completions[i] = tmp;
		}

	}

	return completion_matches(text, multi_generator);
}
