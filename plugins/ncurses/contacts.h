#ifndef __EKG_NCURSES_CONTACTS_H
#define __EKG_NCURSES_CONTACTS_H

int config_contacts_size;
int config_contacts;
char *config_contacts_options;
char *config_contacts_groups;

list_t sorted_all_cache;
int contacts_index;
int contacts_group_index;

#define CONTACTS_MAX_HEADERS 20

int ncurses_contacts_update(window_t *w);
void ncurses_contacts_changed(const char *name);
void ncurses_contacts_new(window_t *w);

#endif /* __EKG_NCURSES_CONTACTS_H */

