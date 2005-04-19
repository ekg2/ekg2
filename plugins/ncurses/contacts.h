#ifndef __EKG_NCURSES_CONTACTS_H
#define __EKG_NCURSES_CONTACTS_H

int config_contacts_size;
int config_contacts;
int config_contacts_groups_all_sessions;
char *config_contacts_options;
char *config_contacts_groups;
int config_contacts_metacontacts_swallow;

list_t sorted_all_cache;
int contacts_index;
int contacts_group_index;

#define CONTACTS_MAX_HEADERS 20

int ncurses_contacts_update(window_t *w);
void ncurses_contacts_changed(const char *name);
void ncurses_all_contacts_changed(const char *name);
void ncurses_contacts_new(window_t *w);

void ncurses_backward_contacts_line(int arg);
void ncurses_forward_contacts_line(int arg);
void ncurses_backward_contacts_page(int arg);
void ncurses_forward_contacts_page(int arg);

#endif /* __EKG_NCURSES_CONTACTS_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
