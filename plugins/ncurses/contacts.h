#ifndef __EKG_NCURSES_CONTACTS_H
#define __EKG_NCURSES_CONTACTS_H

int config_contacts_size;
int config_contacts;
char *config_contacts_options;
char *config_contacts_groups;

int contacts_group_index;

int contacts_update(window_t *w);
void contacts_changed(const char *name);
void contacts_new(window_t *w);

#endif /* __EKG_NCURSES_CONTACTS_H */

