#ifndef __EKG_RECODE_H
#define __EKG_RECODE_H

#include "dynstuff.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EKG_RECODE_CP "CP-1250"
#define EKG_RECODE_ISO2 "ISO-8859-2"
#define EKG_RECODE_UTF8 "UTF-8"

void *ekg_convert_string_init(const char *from, const char *to, void **rev);
void ekg_convert_string_destroy(void *ptr);
char *ekg_convert_string_p(const char *ps, void *ptr);
char *ekg_convert_string(const char *ps, const char *from, const char *to);
string_t ekg_convert_string_t_p(string_t s, void *ptr);
string_t ekg_convert_string_t(string_t s, const char *from, const char *to);

void changed_console_charset(const char *name);
int ekg_converters_display(int quiet);

void ekg_recode_inc_ref(const gchar *enc);
void ekg_recode_dec_ref(const gchar *enc);

char *ekg_recode_from_core(const gchar *enc, gchar *buf);
gchar *ekg_recode_to_core(const gchar *enc, char *buf);

char *ekg_recode_from_core_dup(const gchar *enc, const gchar *buf);
gchar *ekg_recode_to_core_dup(const gchar *enc, const char *buf);

const char *ekg_recode_from_core_use(const gchar *enc, const gchar *buf);
const gchar *ekg_recode_to_core_use(const gchar *enc, const char *buf);

/* below starts the current API */

gchar *ekg_recode_from(const gchar *enc, const char *str);
char *ekg_recode_to(const gchar *enc, const gchar *str);

gchar *ekg_recode_from_locale(const char *str);
char *ekg_recode_to_locale(const gchar *str);

gboolean ekg_recode_gstring_from(const gchar *enc, GString *s);
gboolean ekg_recode_gstring_to(const gchar *enc, GString *s);

void ekg_fix_utf8(gchar *buf);

fstring_t *ekg_recode_fstr_to_locale(const fstring_t *fstr);

#define recode_xfree(org, ret) xfree((char *) ret);

/* CP-1250 */
#define ekg_recode_cp_inc()	ekg_recode_inc_ref(EKG_RECODE_CP)
#define ekg_recode_cp_dec() 	ekg_recode_dec_ref(EKG_RECODE_CP)
#define ekg_locale_to_cp(buf)	ekg_recode_from_core(EKG_RECODE_CP, buf)
#define ekg_cp_to_core(buf)	ekg_recode_to_core(EKG_RECODE_CP, buf)
#define ekg_locale_to_cp_dup(buf) ekg_recode_from_core_dup(EKG_RECODE_CP, buf)
#define ekg_cp_to_core_dup(buf) ekg_recode_to_core_dup(EKG_RECODE_CP, buf)
#define ekg_locale_to_cp_use(buf) ekg_recode_from_core_use(EKG_RECODE_CP, buf)
#define ekg_cp_to_core_use(buf) ekg_recode_to_core_use(EKG_RECODE_CP, buf)

/* ISO-8859-2 */
#define ekg_recode_iso2_inc()	ekg_recode_inc_ref(EKG_RECODE_ISO2)
#define ekg_recode_iso2_dec()	ekg_recode_dec_ref(EKG_RECODE_ISO2)
#define ekg_locale_to_iso2(buf)	ekg_recode_from_core(EKG_RECODE_ISO2, buf)
#define ekg_iso2_to_core(buf)	ekg_recode_to_core(EKG_RECODE_ISO2, buf)
#define ekg_locale_to_iso2_dup(buf) ekg_recode_from_core_dup(EKG_RECODE_ISO2, buf)
#define ekg_iso2_to_core_dup(buf) ekg_recode_to_core_dup(EKG_RECODE_ISO2, buf)
#define ekg_locale_to_iso2_use(buf) ekg_recode_from_core_use(EKG_RECODE_ISO2, buf)
#define ekg_iso2_to_core_use(buf) ekg_recode_to_core_use(EKG_RECODE_ISO2, buf)

/* UTF-8 */
#define ekg_recode_utf8_inc()	ekg_recode_inc_ref(EKG_RECODE_UTF8)
#define ekg_recode_utf8_dec()	ekg_recode_dec_ref(EKG_RECODE_UTF8)
#define ekg_locale_to_utf8(buf)	ekg_recode_from_core(EKG_RECODE_UTF8, buf)
#define ekg_utf8_to_core(buf)	ekg_recode_to_core(EKG_RECODE_UTF8, buf)
#define ekg_locale_to_utf8_dup(buf) ekg_recode_from_core_dup(EKG_RECODE_UTF8, buf)
#define ekg_utf8_to_core_dup(buf) ekg_recode_to_core_dup(EKG_RECODE_UTF8, buf)
#define ekg_locale_to_utf8_use(buf) ekg_recode_from_core_use(EKG_RECODE_UTF8, buf)
#define ekg_utf8_to_core_use(buf) ekg_recode_to_core_use(EKG_RECODE_UTF8, buf)

#ifdef __cplusplus
}
#endif

#endif
