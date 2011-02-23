#define MAX_LINES_PER_SCREEN 300
#define readline_current ((readline_window_t *) window_current->priv_data)
#define readline_window(w) ((readline_window_t *) w->priv_data)

extern int config_ctrld_quits;
extern int config_print_line;

typedef struct {
	char *line[MAX_LINES_PER_SCREEN];
} readline_window_t;

/* deklaracje funkcji interfejsu */
int ui_readline_loop(void);
void ui_readline_init();
void ui_readline_print(window_t *w, int separate, const char *xline);
int window_refresh();
const /*locale*/ char *current_prompt();
void set_prompt(const char *prompt);
int window_write(int id, const char *line);

gchar *window_activity(void);
int bind_sequence(const char *seq, const char *command, int quiet);
int bind_handler_window(int a, int key);
int my_getc(FILE *f);
int my_loop(void);
char **my_completion(char *text, int start, int end);
char *empty_generator(char *text, int state);

/* vars remove some !*/
extern int ui_need_refresh; /* DARK */
extern int ui_screen_width;
extern int ui_screen_height;
extern int pager_lines, screen_lines, screen_columns;

