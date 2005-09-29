#include "module.h"

static int initialized = 0;

MODULE = Ekg2  PACKAGE = Ekg2

PROTOTYPES: ENABLE

#> MAIN

void exit()
CODE:
	ekg_exit();

void echo(char *str)
CODE:
	char *skrypt = SvPV(perl_eval_pv("caller", TRUE), PL_na)+14;
	print("script_generic", "perl", skrypt, str);

void debug(char *debstr)
CODE:
	debug("(perldebug) %s", debstr);

void format_add(char *str, char *value)
CODE:
	format_add(str, value, 1);

char *format_string(char *str)
CODE:
	RETVAL = format_string(str);
OUTPUT:
	RETVAL	

char *fstring2ascii(char *str, void *attr_)
CODE:
        string_t st = string_init(NULL);
	short *attr = attr_;
        int prev = -1, prevbold = 0, prevblink = 0;
        int i;

        for (i=0; i < strlen(str); i++) {
                short chattr = attr[i];
                int bold = 0, blink = 0;

                if (chattr & 64)  bold = 1;
		if (chattr & 256) blink = 1;
/*
		if (chattr & 512) underlinke = 1;
		if (chattr & 1024) reverse = 1;
*/
		if (!blink && prevblink != blink && prev != -1) { /* turn off blinking */
			prev = -1;
			string_append(st, "%n");
		}
                if (blink && (prevblink != blink || prev == -1)) 
			string_append(st, "%i"); /* turn on blinking */

                if (!(chattr & 128) && (prev != (chattr & 7) || prevbold != bold)) { /* change color/bold */
			string_append_c(st, '%');
                        switch (chattr & 7) {
				case (0): string_append_c(st, (bold) ? 'K' : 'k'); break;
				case (1): string_append_c(st, (bold) ? 'R' : 'r'); break;
				case (2): string_append_c(st, (bold) ? 'G' : 'g'); break;
				case (3): string_append_c(st, (bold) ? 'Y' : 'y'); break;
				case (4): string_append_c(st, (bold) ? 'B' : 'b'); break;
				case (5): string_append_c(st, (bold) ? 'M' : 'm'); break; /* | fioletowy     | %m/%p  | %M/%P | %q  | */
				case (6): string_append_c(st, (bold) ? 'C' : 'c'); break;
				case (7): string_append_c(st, (bold) ? 'W' : 'w'); break;
                        }
                        prev = (chattr & 7);
                } else if ((chattr & 128) && prev != -1) { /* reset all attributes */
                        string_append(st, "%n");
                        prev = -1;
                }
                string_append_c(st, str[i]);

		prevblink = blink;
		prevbold  = bold;
        }
        RETVAL =  string_free(st, 0);
OUTPUT:
	RETVAL


void print(int dest, char *str)
CODE:
	char *line;
        while ((line = split_line(&str))) {
                window_print(ekg2_window_target(window_exist(dest)), NULL, 0, fstring_new((const char *) va_format_string(line)));
        }

void init()
CODE:
	initialized = 1;

void deinit()
CODE:

#> WATCHE

int watch_add(int fd, int type, int persist, char *handler, void *data);
CODE:
	perl_watch_add(fd, type, persist, handler, data);

int watch_remove(int fd, int type);
CODE:	
	watch_remove(&perl_plugin, fd, type);

#> QUERIES

int handler_bind(char *query_name, char *handler)
CODE:
	perl_handler_bind(query_name, handler);

Ekg2::Script script_find(char *name)
CODE:
	RETVAL = script_find(&perl_lang, name);
OUTPUT:
	RETVAL

char *get_ekg2_dir()
CODE:
	RETVAL = config_dir;
OUTPUT:
	RETVAL

#> STALE! 

int EKG_MSGCLASS_SENT()
CODE:
	RETVAL = EKG_MSGCLASS_SENT;
OUTPUT:
	RETVAL

int EKG_MSGCLASS_SENT_CHAT()
CODE:
	RETVAL = EKG_MSGCLASS_SENT_CHAT;
OUTPUT:
	RETVAL

int EKG_NO_THEMEBIT()
CODE:
        RETVAL = EKG_NO_THEMEBIT;
OUTPUT:
        RETVAL

int WATCH_READ_LINE()
CODE:
	RETVAL = WATCH_READ_LINE;
OUTPUT:
	RETVAL

int WATCH_READ()
CODE:
	RETVAL = WATCH_READ;
OUTPUT:
	RETVAL

int WATCH_WRITE()
CODE:
	RETVAL = WATCH_WRITE;
OUTPUT:
	RETVAL
	
int PLUGIN_UI()
CODE:
	RETVAL = PLUGIN_UI;
OUTPUT:
	RETVAL

int PLUGIN_PROTOCOL()
CODE:
	RETVAL = PLUGIN_PROTOCOL;
OUTPUT:
	RETVAL
##################################################################################

BOOT:
	ekg2_boot(Session);
	ekg2_boot(Variable);
	ekg2_boot(Plugin);
	ekg2_boot(Window);
	ekg2_boot(Command);
	ekg2_boot(Timer);
	ekg2_boot(Userlist);
