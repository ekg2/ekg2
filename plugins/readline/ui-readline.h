#include <stdio.h>
#include <ekg/windows.h>

#define MAX_LINES_PER_SCREEN 300
#define readline_current ((readline_window_t *) window_current->private)
#define readline_window(w) ((readline_window_t *) w->private)

/* todo: ./configure should do it */
#define HAVE_RL_BIND_KEY_IN_MAP
#define HAVE_RL_GET_SCREEN_SIZE
#define HAVE_RL_SET_KEY
#define HAVE_RL_SET_PROMPT
#define HAVE_RL_FILENAME_COMPLETION_FUNCTION

extern int config_ctrld_quits;

typedef struct {
        char *line[MAX_LINES_PER_SCREEN];
} readline_window_t;

/* deklaracje funkcji interfejsu */
int ui_readline_loop();
void ui_readline_init();
void ui_readline_print(window_t *w, int separate, const char *xline);
int window_refresh();
const char *current_prompt();
int window_write(int id, const char *line);

char *window_activity();
int bind_sequence(const char *seq, const char *command, int quiet);
int bind_handler_window(int a, int key);
int my_getc(FILE *f);
int my_loop();
char **my_completion(char *text, int start, int end);
char *empty_generator(char *text, int state);

/* vars remove some !*/
int ui_need_refresh; /* DARK */
int ui_screen_width;
int ui_screen_height;
void *userlist;
int pager_lines, screen_lines, screen_columns;

