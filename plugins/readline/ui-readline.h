#define MAX_LINES_PER_SCREEN 300
#define readline_current ((readline_window_t *) window_current->private)
#define readline_window(w) ((readline_window_t *) w->private)

typedef struct {
	        char *line[MAX_LINES_PER_SCREEN];
} readline_window_t;

/* deklaracje funkcji interfejsu */
void ui_readline_loop();
void ui_readline_init();
void ui_readline_print(window_t *w, int separate, const char *xline);
int window_refresh();
const char *current_prompt();
int window_write(int id, const char *line);

char *window_activity();
int bind_sequence(const char *seq, const char *command, int quiet);
int bind_handler_window(int a, int key);

