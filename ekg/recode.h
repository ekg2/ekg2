#ifndef __EKG_RECODE_H
#define __EKG_RECODE_H

#include "dynstuff.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ekg_recode_name {
	EKG_RECODE_CP = 0,
	EKG_RECODE_LATIN2,
	EKG_RECODE_UTF8
};

void ekg_recode_inc_ref(enum ekg_recode_name ref);
void ekg_recode_dec_ref(enum ekg_recode_name ref);

void *ekg_convert_string_init(const char *from, const char *to, void **rev);
void ekg_convert_string_destroy(void *ptr);
char *ekg_convert_string_p(const char *ps, void *ptr);
char *ekg_convert_string(const char *ps, const char *from, const char *to);
string_t ekg_convert_string_t_p(string_t s, void *ptr);
string_t ekg_convert_string_t(string_t s, const char *from, const char *to);
int ekg_converters_display(int quiet);

/* CP-1250 */
char *ekg_locale_to_cp(char *buf);
char *ekg_cp_to_locale(char *buf);
void ekg_recode_cp_inc();
void ekg_recode_cp_dec();

/* ISO-8859-2 */
char *ekg_locale_to_latin2(char *buf);
char *ekg_latin2_to_locale(char *buf);

/* UTF-8 */
char *ekg_locale_to_utf8(char *buf);
char *ekg_utf8_to_locale(char *buf);
void ekg_recode_utf8_inc();
void ekg_recode_utf8_dec();

char *ekg_any_to_locale(char *buf, char *inp);	char *ekg_locale_to_any(char *buf, char *inp);

#ifdef __cplusplus
}
#endif

#endif
