#ifndef __EKG_BINDINGS_H
#define __EKG_BINDINGS_H

#define BINDING_FUNCTION(x) void x(const char *arg) 

struct binding {
	struct binding	*next;

	char		*key;

	char		*action;			/* akcja */
	unsigned int	internal		: 1;	/* czy domyślna kombinacja? */
	void	(*function)(const char *arg);		/* funkcja obsługująca */
	char		*arg;				/* argument funkcji */

	char		*default_action;		/* domyślna akcja */
	void	(*default_function)(const char *arg);	/* domyślna funkcja */
	char		*default_arg;			/* domyślny argument */
};

typedef struct binding_added {
	struct binding_added	*next;

	char		*sequence;
	struct binding	*binding;
} binding_added_t;

extern struct binding *bindings;
extern binding_added_t *bindings_added;

void binding_list(int quiet, const char *name, int all);

int binding_help(int a, int b);
int binding_quick_list(int a, int b);

void bindings_init();
void bindings_default();
void binding_free();

#endif
