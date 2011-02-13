#ifndef __EKG_NCURSES_NOTIFY_H
#define __EKG_NCURSES_NOTIFY_H

extern int ncurses_typing_mod;			/* whether buffer was modified */
extern window_t *ncurses_typing_win;		/* last window for which typing was sent */
extern int config_typing_timeout;
extern int config_typing_timeout_inactive;
extern int config_typing_interval;

int ncurses_typingsend(window_t *w, int chatstate);
EKG_TIMER(ncurses_typing);

#endif