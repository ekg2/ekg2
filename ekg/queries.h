#ifndef __EKG_QUERIES
#define __EKG_QUERIES

#ifdef __cplusplus
extern "C" {
#endif

#define QUERY_ARGS_MAX 12

enum query_arg_type {
	QUERY_ARG_END = 0,	/* Terminates an array of `query_arg_type' values */

	/* Type specifiers */
	QUERY_ARG_CHARP,	/* char *	*/
	QUERY_ARG_CHARPP,	/* char **	*/
	QUERY_ARG_INT,		/* int		*/
	QUERY_ARG_UINT,		/* unsgined int */		/* -> time_t, uint32_t */

	QUERY_ARG_WINDOW = 100, /* window_t	*/
	QUERY_ARG_FSTRING,	/* fstring_t	*/
	QUERY_ARG_USERLIST,	/* userlist_t	*/
	QUERY_ARG_SESSION,	/* session_t	*/

	/* Flags. Can be OR-ed with type specifiers. */
	QUERY_ARG_CONST = (1<<31),	/* Means that the argument should not be modified by a script.
					 * In case it _will_ be modified, the new value will be
					 * ignored and not propagated further. */

	/* Masks. Used for extracting type specifiers and flags. */
	QUERY_ARG_FLAGS = (QUERY_ARG_CONST),
	QUERY_ARG_TYPES = ~QUERY_ARG_FLAGS
};

typedef struct query_def_node {
        struct query_def_node* next;
        char *name;
        int name_hash;
        enum query_arg_type params[QUERY_ARGS_MAX];
} query_def_t;

extern query_def_t *registered_gueries;
extern int gueries_registered_count;

#ifdef __cplusplus
}
#endif

#endif

