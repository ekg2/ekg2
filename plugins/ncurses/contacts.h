#ifndef __EKG_NCURSES_CONTACTS_H
#define __EKG_NCURSES_CONTACTS_H

extern int config_contacts_size;
extern int config_contacts;
extern int config_contacts_groups_all_sessions;
extern int config_contacts_descr;
extern int config_contacts_edge;
extern int config_contacts_frame;
extern int config_contacts_margin;
extern int config_contacts_orderbystate;
extern int config_contacts_wrap;
extern char *config_contacts_order;
extern char *config_contacts_groups;
extern int config_contacts_metacontacts_swallow;

extern int contacts_group_index;

#define CONTACTS_MAX_HEADERS 20

int ncurses_contacts_update(window_t *w, int save_pos);
void ncurses_contacts_changed(const char *name);
void ncurses_contacts_new(window_t *w);

#endif /* __EKG_NCURSES_CONTACTS_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
