#ifndef __EKG_RECODE_H
#define __EKG_RECODE_H

char *remote_recode_from(char *buf);
char *remote_recode_to(char *buf);
void remote_recode_reinit(void);
void remote_recode_destroy(void);

#endif
