/*
 *  (C) Copyright 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001,2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002,2003,2004 Martin Ã–berg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004,2005,2006,2007 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *                     2008 Wies³aw Ochmiñski <wiechu@wiechu.com>
 *
 * Protocol description with author's permission from: http://iserverd.khstu.ru/oscar/
 *  (C) Copyright 2000-2005 Alexander V. Shutko <AVShutko@mail.khstu.ru>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ekg/debug.h>

#include "icq.h"
#include "misc.h"
#include "icq_caps.h"
#include "icq_const.h"
#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"


SNAC_SUBHANDLER(icq_snac_extension_error) {
	/* SNAC(15,01) SRV_ICQEXT_ERROR	Client/server error
	 *
	 * This is an error notification snac
	 */
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		pkt.error = 0;
	/* XXX	TLV.Type(0x08) - error subcode */
	/* XXX	TLV.Type(0x21) - service specific data from request */

	icq_snac_error_handler(s, "extension", pkt.error);
	return 0;
}

#define METASNAC_SUBHANDLER(x) static int x(session_t *s, unsigned char *buf, int len, private_data_t **info)
typedef int (*metasnac_subhandler_t) (session_t *s, unsigned char *, int, private_data_t **info);

#include "icq_fieldnames.inc"

typedef struct {
	int type;
	int item;		// 'S' - str; 'w' - word; 'c' - byte; 'L' bool
	const char *display;	// display name
	const char *name;	// private item name
	void *ltab;
} _userinfo_t;

static const _userinfo_t userinfo[] = {
	/* User basic info reply */
	{META_BASIC_USERINFO,		'S', N_("Nickname"),		"nick",		NULL},
	{META_BASIC_USERINFO,		'S', N_("Firstname"),		"first_name",	NULL},
	{META_BASIC_USERINFO,		'S', N_("Lastname"),		"last_name",	NULL},
	{META_BASIC_USERINFO,		'S', N_("Email"),		"email",	NULL},
	{META_BASIC_USERINFO,		'S', N_("City"),		"city",		NULL},
	{META_BASIC_USERINFO,		'S', N_("State"),		"state",	NULL},
	{META_BASIC_USERINFO,		'S', N_("Phone"),		"phone",	NULL},
	{META_BASIC_USERINFO,		'S', N_("Fax"),			"fax",		NULL},
	{META_BASIC_USERINFO,		'S', N_("Street"),		"street",	NULL},
	{META_BASIC_USERINFO,		'S', N_("Cellular"),		"mobile",	NULL},
	{META_BASIC_USERINFO,		'S', N_("Zip"),			"zip", 		NULL},
	{META_BASIC_USERINFO,		'w', N_("Country"),		"country", 	countryField},
	{META_BASIC_USERINFO,		'c', N_("Timezone"),		"tzone", 	NULL},
	{META_BASIC_USERINFO,		'L', N_("Authorization"),	"auth",		NULL},
	{META_BASIC_USERINFO,		'c', N_("Webaware"),		"webaware",	webawareField},
	{META_BASIC_USERINFO,		'L', N_("Publish primary email"),"pub_email",	NULL},
	{META_BASIC_USERINFO,		'L', N_("Direct connection"),	"dc_perm",	NULL},
	{META_BASIC_USERINFO,		'S', NULL,			NULL,		NULL},	// Is here 'zip code' again?
	/* User work info reply */
	{META_WORK_USERINFO,		'S', N_("CompanyCity"),		"c_city",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyState"),	"c_state",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyPhone"),	"c_phone",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyFax"),		"c_fax",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyStreet"),	"c_street",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyZIP"),		"c_zip",	NULL},
	{META_WORK_USERINFO,		'w', N_("CompanyCountry"),	"c_country",	countryField},
	{META_WORK_USERINFO,		'S', N_("Company"),		"c_name",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyDepartment"),	"c_depart",	NULL},
	{META_WORK_USERINFO,		'S', N_("CompanyPosition"),	"c_pos",	NULL},
	{META_WORK_USERINFO,		'w', N_("CompanyOccupation"),	"c_occup",	workField},
	{META_WORK_USERINFO,		'S', N_("CompanyHomepage"),	"c_www",	NULL},
	{META_WORK_USERINFO,		'S', NULL,			NULL,		NULL},	// Is here 'zip code' again?
	/* User more info reply */
	{META_MORE_USERINFO,		'w', N_("Age"),			"age",		NULL},
	{META_MORE_USERINFO,		'c', N_("Gender"),		"gender",	genderField},
	{META_MORE_USERINFO,		'S', N_("Homepage"),		"www",		NULL},
	{META_MORE_USERINFO,		'w', N_("Birth date"),		"birth", 	NULL},
	{META_MORE_USERINFO,		'c', NULL,			".month", 	NULL},
	{META_MORE_USERINFO,		'c', NULL,			".day", 	NULL},
	{META_MORE_USERINFO,		'c', N_("Language1"),		"lang1",	languageField},
	{META_MORE_USERINFO,		'c', N_("Language2"),		"lang2",	languageField},
	{META_MORE_USERINFO,		'c', N_("Language3"),		"lang3",	languageField},
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'S', N_("Original from City"),	"o_city",	NULL},
	{META_MORE_USERINFO,		'S', N_("Original from State"),	"o_state",	NULL},
	{META_MORE_USERINFO,		'w', N_("Original from Country"),"o_country",	countryField},
	{META_MORE_USERINFO,		'c', N_("Marital status"),	"marital",	maritalField},
	{META_MORE_USERINFO,		'L', N_("AllowSpam"),		"AllowSpam", 	NULL},
	{META_MORE_USERINFO,		'w', N_("InfoCP"),		"InfoCP", 	NULL},
	{META_MORE_USERINFO,		'c', NULL,			NULL, 		NULL},	// skip 1 unknown byte
	{META_MORE_USERINFO,		'S', NULL,			NULL, 		NULL},	// skip unknown string
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'S', NULL,			NULL, 		NULL},	// skip unknown string
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'w', NULL,			NULL, 		NULL},	// skip 2 unknown bytes
	{META_MORE_USERINFO,		'c', NULL,			NULL, 		NULL},	// skip 1 unknown byte
	{META_MORE_USERINFO,		'S', NULL,			NULL, 		NULL},	// skip unknown string
	/* User notes (about) info reply */
	{META_NOTES_USERINFO,		'S', N_("About"),		"about",	NULL},
	/* User extended email info reply */
	{META_EMAIL_USERINFO,		'S', N_("Email"),		"email1",	NULL},
	{META_EMAIL_USERINFO,		'S', N_("Email"),		"email2",	NULL},
	{META_EMAIL_USERINFO,		'S', N_("Email"),		"email3",	NULL},
	{META_EMAIL_USERINFO,		'S', N_("Email"),		"email4",	NULL},	// more emails? ?wo?
	/* User interests info reply */
	{META_INTERESTS_USERINFO,	'w', N_("Interests"),		"interests1",		interestsField},
	{META_INTERESTS_USERINFO,	'S', N_("InterestsStr"),	"interestsStr1",	NULL},
	{META_INTERESTS_USERINFO,	'w', N_("Interests"),		"interests2",		interestsField},
	{META_INTERESTS_USERINFO,	'S', N_("InterestsStr"),	"interestsStr2",	NULL},
	{META_INTERESTS_USERINFO,	'w', N_("Interests"),		"interests3",		interestsField},
	{META_INTERESTS_USERINFO,	'S', N_("InterestsStr"),	"interestsStr3",	NULL},
	{META_INTERESTS_USERINFO,	'w', N_("Interests"),		"interests4",		interestsField},
	{META_INTERESTS_USERINFO,	'S', N_("InterestsStr"),	"interestsStr4",	NULL},
	/* User past/affilations info reply */
	{META_AFFILATIONS_USERINFO,	'w', N_("PastAff"),		"pastaff1",		pastField},
	{META_AFFILATIONS_USERINFO,	'S', N_("PastAffStr"),		"pastaffStr1",		NULL},
	{META_AFFILATIONS_USERINFO,	'w', N_("PastAff"),		"pastaff2",		pastField},
	{META_AFFILATIONS_USERINFO,	'S', N_("PastAffStr"),		"pastaffStr2",		NULL},
	{META_AFFILATIONS_USERINFO,	'w', N_("PastAff"),		"pastaff3",		pastField},
	{META_AFFILATIONS_USERINFO,	'S', N_("PastAffStr"),		"pastaffStr3",		NULL},
	{META_AFFILATIONS_USERINFO,	'w', N_("Aff"),			"aff1",			pastField},
	{META_AFFILATIONS_USERINFO,	'S', N_("AffStr"),		"affStr1",		NULL},
	{META_AFFILATIONS_USERINFO,	'w', N_("Aff"),			"aff2",			pastField},
	{META_AFFILATIONS_USERINFO,	'S', N_("AffStr"),		"affStr2",		NULL},
	{META_AFFILATIONS_USERINFO,	'w', N_("Aff"),			"aff3",			pastField},
	{META_AFFILATIONS_USERINFO,	'S', N_("AffStr"),		"affStr3",		NULL},
	/* Short user information reply */
	{META_SHORT_USERINFO,		'S', N_("Nickname"),		"nick",		NULL},
	{META_SHORT_USERINFO,		'S', N_("Firstname"),		"first_name",	NULL},
	{META_SHORT_USERINFO,		'S', N_("Lastname"),		"last_name",	NULL},
	{META_SHORT_USERINFO,		'S', N_("Email"),		"email",	NULL},
	/* User homepage category information reply */
	{META_HPAGECAT_USERINFO,	'w', N_("Homepage category"),	"wwwcat",	NULL},	// ?WO? lookup???
	{META_HPAGECAT_USERINFO,	'S', N_("Homepage keywords"),	"wwwkeys",	NULL},

	{0, 		0, NULL, NULL, NULL}
};

struct fieldnames_t meta_name[]={
	{META_BASIC_USERINFO,		"basic"},
	{META_WORK_USERINFO,		"work"},
	{META_MORE_USERINFO,		"more"},
	{META_NOTES_USERINFO,		"notes"},
	{META_EMAIL_USERINFO,		"email"},
	{META_INTERESTS_USERINFO,	"interests"},
	{META_AFFILATIONS_USERINFO,	"affilations"},
	{META_SHORT_USERINFO,		"short"},
	{META_HPAGECAT_USERINFO,	"hpagecat"},

	{META_SET_FULLINFO_ACK,		"fullinfo_ack"},

	{SRV_USER_FOUND,		"userfound"},
	{SRV_LAST_USER_FOUND,		"userfound_last"},
	{SRV_RANDOM_FOUND,		""},
	{-1,  NULL}};


static int __get_userinfo_data(unsigned char *buf, int len, int type, private_data_t **info) {
	int i, ret = 0;

	for (i=0; userinfo[i].type; i++) {
		if (userinfo[i].type != type)
			continue;
		switch (userinfo[i].item) {
			case 'S':
			{
				char *str;
				if (!ICQ_UNPACK(&buf, "S", &str))
					ret = 1;
				else
					private_item_set(info, userinfo[i].name, str);
				break;
			}
			case 'w':
			{
				uint16_t w = 0;
				if (!ICQ_UNPACK(&buf, "w", &w))
					ret = 1;
				else
					private_item_set_int(info, userinfo[i].name, w);
				break;
			}
			case 'b':
			case 'c':
			case 'L':
			{
				uint8_t b = 0;
				if (!ICQ_UNPACK(&buf, "c", &b))
					ret = 1;
				else
					private_item_set_int(info, userinfo[i].name, b);
				break;
			}
			default:
				debug_error("__get_userinfo_data() unknown item type %d\n", userinfo[i].item);
				ret = 1;
				break;
		}
		if (ret)
			private_item_set(info, userinfo[i].name, "");
	}
	if (len)
		debug_error("__get_userinfo_data() more data follow: %u\n", len);
	if (ret)
		debug_error("__get_userinfo_data() type:0x%x error: %u\n", type, len);
	return ret;
}

static int __displayed = 0;	/* Luckily we're not multithreaded */

static void __display_info(session_t *s, int type, private_data_t *data) {
	int i, uid = private_item_get_int(&data, "uid");
	const char *str;
	char *theme = saprintf("icq_userinfo_%s", icq_lookuptable(meta_name, type));

	for (i=0; userinfo[i].type; i++) {
		if ( (userinfo[i].type != type) || (!userinfo[i].name) )
			continue;
		if (userinfo[i].ltab)
			str = icq_lookuptable(userinfo[i].ltab, private_item_get_int(&data, userinfo[i].name));
		else if (userinfo[i].item == 'L')
			str = private_item_get_int(&data, userinfo[i].name) ? _("Yes") : _("No");
		else
			str = private_item_get(&data, userinfo[i].name);
		if ( str && *str) {
			char *___str = xstrdup(str); /* XXX, guess recode */

			if (!__displayed)
				print("icq_userinfo_start", session_name(s), itoa(uid), theme);
			print(theme, session_name(s), itoa(uid), userinfo[i].display, ___str);
			__displayed = 1;

			xfree(___str);
		}
	}
	xfree(theme);
}


/*
 * Userinfo handlers
 *
 */


METASNAC_SUBHANDLER(icq_snac_extensions_interests) {
	uint8_t count;
	int i;

	if (!ICQ_UNPACK(&buf, "C", &count))
		return -1;

	/* 4 is the maximum allowed personal interests, if count is
	   higher it's likely a parsing error */

	if (count > 4)
		count = 4;

	for (i = 0; i < count; i++) {
		char *tmp;
		const char *str;
		uint16_t w;

		if (ICQ_UNPACK(&buf, "wS", &w, &str)) {
			tmp = saprintf("interests%d", i+1);
			private_item_set_int(info, tmp, w);
			xfree(tmp);
			tmp = saprintf("interestsStr%d", i+1);
			private_item_set(info, tmp, str);
			xfree(tmp);
		}
	}
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_affilations) {
	static const char *names[] = {"pastaff", "aff"};
	uint8_t count;
	int i, k;

	for (k=0; k<2; k++) {
		if (!ICQ_UNPACK(&buf, "C", &count))
			return -1;

		/* 3 is the maximum allowed backgrounds, if count is
		   higher it's likely a parsing error */

		if (count > 3)
			count = 3;

		for (i = 0; i < count; i++) {
			char *name1, *name2;
			const char *str;
			uint16_t w;

			name1 = saprintf("%s%d", names[k], i+1);
			name2 = saprintf("%sStr%d", names[k], i+1);
			if (!ICQ_UNPACK(&buf, "wS", &w, &str)) {
				w = 0;
				str = "";
			}
			private_item_set_int(info, name1, w);
			private_item_set(info, name2, str);
			xfree(name1);
			xfree(name2);
		}
	}

	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_basicinfo) {

	__get_userinfo_data(buf, len, META_BASIC_USERINFO, info);

	{	/* correct results */
		char *tmp;
		int tz = private_item_get_int(info, "tzone");
		if (tz & 0x80)
			tz -= 256;
		tmp = saprintf("GMT%+d", tz/2);
		private_item_set(info, "tzone", tmp);
		xfree(tmp);

		private_item_set_int(info, "auth", !private_item_get_int(info, "auth"));
		private_item_set_int(info, "webaware", private_item_get_int(info, "webaware") + 1);
	}

	{
		userlist_t *u;
		char *uid = icq_uid(private_item_get(info, "uid"));
		if ( (u = userlist_find(s, uid)) ) {
			user_private_item_set(u, "first_name", private_item_get(info, "first_name"));
			user_private_item_set(u, "last_name",  private_item_get(info, "last_name"));
		}
		xfree(uid);
	}

	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_notes) {
	__get_userinfo_data(buf, len, META_NOTES_USERINFO, info);
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_workinfo) {
	__get_userinfo_data(buf, len, META_WORK_USERINFO, info);
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_shortinfo) {
	__get_userinfo_data(buf, len, META_SHORT_USERINFO, info);
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_email) {
	uint8_t count_discard;
	int i;

	/* This value used to be a e-mail counter. Either that was wrong or
	 * Mirabilis changed the behaviour again. It usually says NULL now so
	 * I use the packet byte count to extract the e-mails instead.
	 */

	if (!ICQ_UNPACK(&buf, "C", &count_discard))
		return -1;

	for (i = 0; (len > 4); i++) {
		char *tmp;
		const char *str;

		uint8_t publish_flag;	/* Don't publish flag */

		if (!ICQ_UNPACK(&buf, "C", &publish_flag))
			return -1;

		if (!ICQ_UNPACK(&buf, "S", &str)) {
			tmp = saprintf("email%d", i+1);
			private_item_set(info, tmp, str);
			xfree(tmp);
		}
	}

	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_moreinfo) {

	__get_userinfo_data(buf, len, META_MORE_USERINFO, info);

	{
		int year  = private_item_get_int(info, "birth");
		int month = private_item_get_int(info, ".month");
		int day   = private_item_get_int(info, ".day");

		if (year && month && day) {
			char *bdate = saprintf("%04d-%02d-%02d", year, month, day);
			private_item_set(info, "birth", bdate);
			xfree(bdate);
		} else
			private_item_set(info, "birth", "");

		private_item_set(info, ".month", "");
		private_item_set(info, ".day", "");

		if (!private_item_get_int(info, "age"))
			private_item_set(info, "age", "");
	}
	return 0;
}

METASNAC_SUBHANDLER(icq_snac_extensions_hpagecat) {
	struct {
	    uint8_t enabled;
	    uint16_t cat;
	    char *str;
	} pkt;

	private_item_set(info, "wwwcat", NULL);
	private_item_set(info, "wwwkeys", NULL);

	if (!ICQ_UNPACK(&buf, "c", &pkt.enabled))
		return -1;

	if (!pkt.enabled)
		return 0;

	if (!ICQ_UNPACK(&buf, "wS", &pkt.cat, &pkt.str))
		return -1;

	private_item_set_int(info, "wwwcat", pkt.cat);
	private_item_set(info, "wwwkeys", pkt.str);

	return 0;
}


/*
 * search handlers
 *
 */

static int icq_snac_extension_userfound_common(session_t *s, unsigned char *buf, int len, int islast) {
	char *nickname = NULL;
	char *first_name = NULL;
	char *last_name = NULL;
	char *email = NULL;
	char *full_name;
	char *temp;
	const char *__age = NULL;
	const char *__gender = "";
	char *__active;

	uint32_t uin;
	uint16_t len2;
	uint16_t status, age;
	uint8_t auth, gender;

	/* XXX, sprawdzic czy mamy cookie. */

	if (!ICQ_UNPACK(&buf, "w", &len2))
		return -1;

	if (len < len2)
		return -1;

	if (!ICQ_UNPACK(&buf, "i", &uin))
		return -1;

	if (!ICQ_UNPACK(&buf, "S", &temp)) goto cleanup;
	nickname = xstrdup(temp);

	if (!ICQ_UNPACK(&buf, "S", &temp)) goto cleanup;
	first_name = xstrdup(temp);

	if (!ICQ_UNPACK(&buf, "S", &temp)) goto cleanup;
	last_name = xstrdup(temp);

	if (!ICQ_UNPACK(&buf, "S", &temp)) goto cleanup;
	email = xstrdup(temp);

	if (first_name[0] && last_name[0])
		full_name = saprintf("%s %s", first_name, last_name);
	else
		full_name = xstrdup(first_name[0] ? first_name : last_name);

	if (ICQ_UNPACK(&buf, "cwcw", &auth, &status, &gender, &age)) {
		if (age)
			__age = itoa(age);		// XXX calculate birthyear?
		if (gender)
			__gender = (gender==2) ? "m" : "f";
	} else {
		debug_error("icq_snac_extension_userfound_common() broken\n");
		auth = status = gender = age = 0;
	}

	/* XXX, "search_results_multi", "search_results_single" */
	/* XXX, instead of email we had city */
	/* XXX, some time ago i was thinking to of function which
	 * 	if data was truncated [because of width in format]
	 * 	it'd take another line to complete..
	 *
	 * 	i don't like truncation of data for instance:
	 * 	 08:17:12  97320776 | darkjames    | Jakub Zawadz | -    | darkjames@po
	 *
	 * 	i was thinking about:
	 * 	           97320776 | darkjames    | Jakub Zawwdz | -    | darkjames@po
	 * 	                                     ki                    czta.onet.pl
	 *
	 * 	of course we can do more magic, and wrap...
	 * 					     Jakub
	 * 					     Zawadzki
	 *
	 * 	or maybe let's  align to center? :)
	 * 						 Jakub
	 * 					       Zawadzki
	 */

	{
		const char *fvalue;
		/* XXX ?wo? new formats for icq status
		 * status (0 - offline, 1 - online, 2 - non_webaware)
		 */
		switch (status) {
			case 0:
				fvalue = format_find("search_results_multi_notavail");
				break;
			case 1:
				fvalue = format_find("search_results_multi_avail");
				break;
			default:
				fvalue = format_find("search_results_multi_unknown");
				break;
		}
		temp = format_string(fvalue);
		/* XXX ?wo? add format for "auth" */
		__active = saprintf("%s %s", temp, auth ? " " : "A");
		xfree(temp);
	}
	print_info(NULL, s, "search_results_multi", itoa(uin), full_name, nickname, email,
			__age ? __age : ("-"), __gender, __active);

	xfree(__active);
	xfree(full_name);

	if (islast && len>=4) {
		uint32_t omit;
		ICQ_UNPACK(&buf, "I", &omit);
		debug_warn("icq_snac_extension_userfound_last() Bulshit warning!\n");
		debug_white("icq_snac_extension_userfound_last() %d search results omitted\n", omit);
	}

	icq_hexdump(DEBUG_WHITE, buf, len);

	xfree(nickname); xfree(first_name); xfree(last_name); xfree(email);
	return 0;

cleanup:
	xfree(nickname); xfree(first_name); xfree(last_name); xfree(email);
	return -1;
}

METASNAC_SUBHANDLER(icq_snac_extension_userfound) { return icq_snac_extension_userfound_common(s, buf, len, 0); }
METASNAC_SUBHANDLER(icq_snac_extension_userfound_last) { return icq_snac_extension_userfound_common(s, buf, len, 1); }
METASNAC_SUBHANDLER(icq_snac_extension_fullinfo_ack) { return 0; }

static metasnac_subhandler_t get_userinfo_extension_handler(uint16_t subtype) {
	switch (subtype) {
	/* userinfo */
		case META_BASIC_USERINFO:	return icq_snac_extensions_basicinfo;
		case META_INTERESTS_USERINFO:	return icq_snac_extensions_interests;
		case META_NOTES_USERINFO:	return icq_snac_extensions_notes;
		case META_HPAGECAT_USERINFO:	return icq_snac_extensions_hpagecat;
		case META_WORK_USERINFO:	return icq_snac_extensions_workinfo;
		case META_MORE_USERINFO:	return icq_snac_extensions_moreinfo;
		case META_AFFILATIONS_USERINFO:	return icq_snac_extensions_affilations;
		case META_EMAIL_USERINFO:	return icq_snac_extensions_email;
		case META_SHORT_USERINFO:	return icq_snac_extensions_shortinfo;
	}
	return NULL;
}

static int icq_meta_info_reply(session_t *s, unsigned char *buf, int len, private_data_t **info, int show) {
	/* SNAC(15,03)/07DA SRV_META_INFO_REPLY	Meta information response
	 *
	 * This is the server response to client meta info request SNAC(15,02)/07D0.
	 */
	struct {
		uint16_t subtype;
		uint8_t result;
		unsigned char *data;
	} pkt;
	int userinfo = 0;

	metasnac_subhandler_t handler;

	if (!ICQ_UNPACK(&pkt.data, "wc", &pkt.subtype, &pkt.result)) {
		debug_error("icq_meta_info_reply() broken\n");
		return -1;
	}

	debug_white("icq_meta_info_reply() subtype=%.4x result=%.2x (len=%d)\n", pkt.subtype, pkt.result, len);

	if ( (handler = get_userinfo_extension_handler(pkt.subtype)) ) {
		userinfo = 1;
	} else {
		switch (pkt.subtype) {
			/* search */
			case SRV_LAST_USER_FOUND:	handler = icq_snac_extension_userfound_last; break;
			case SRV_USER_FOUND:		handler = icq_snac_extension_userfound; break;
		
			case META_SET_FULLINFO_ACK:	handler = icq_snac_extension_fullinfo_ack; break;

			case SRV_RANDOM_FOUND:		handler = NULL; break;	/* XXX, SRV_RANDOM_FOUND */
			default:			handler = NULL;
		}
	}

	__displayed = 0;
	if (!handler) {
		debug_error("icq_meta_info_reply() ignored: %.4x\n", pkt.subtype);
		icq_hexdump(DEBUG_ERROR, pkt.data, len);
		return 0;
	} else {
		int uid = info ? private_item_get_int(info, "uid") : -1;
		debug_function("icq_snac_extensions_%s()", icq_lookuptable(meta_name, pkt.subtype));
		if (userinfo)
			debug_function(" uid: %u", uid);
		debug_function("\n");

		if (pkt.result == 0x0A) {
			handler(s, pkt.data, len, info);
		} else if (!userinfo){
			/* Failed search */
			debug_error("icq_snac_extension_userfound() search error: %u\n", pkt.result);
		}

		if (show) {
			__display_info(s, pkt.subtype, *info);
			if (__displayed)
				print("icq_userinfo_end", session_name(s), itoa(uid));
		}
	}

	return 0;
}

static int check_replyreq(session_t *s, unsigned char **buf, int *len, int *type) {
	struct {
		uint16_t type;
		uint16_t len;
	} tlv;
	struct {
		uint16_t len;
		uint32_t uid;
		uint16_t type;
		uint16_t id;
	} pkt;

	if (!icq_unpack(*buf, buf, len, "WW", &tlv.type, &tlv.len) || (tlv.type != 0x0001) || (tlv.len < 10)) {
		debug_error("check_replyreq() broken(1)\n");
		return 0;
	}

	if (*len!=tlv.len) {
		debug_error("icq_snac_extension_replyreq() broken(1,5)\n");
		return 0;
	}

	if (!icq_unpack(*buf, buf, len, "wiwW", &pkt.len, &pkt.uid, &pkt.type, &pkt.id)) {
		debug_error("icq_snac_extension_replyreq() broken(2)\n");
		return 0;
	}

	debug_white("icq_snac_extension_replyreq() uid=%d type=%.4x (len=%d, len2=%d)\n", pkt.uid, pkt.type, *len, pkt.len);

	if (xstrcmp(s->uid+4, itoa(pkt.uid))) {
		debug_error("icq_snac_extension_replyreq() 1919 UIN mismatch: %s vs %ld.\n", s->uid+4, pkt.uid);
		return 0;
	}

	if (tlv.len - 2 != pkt.len) {
		debug("icq_snac_extension_replyreq() 1743 Size mismatch in packet lengths.\n");
		return 0;
	}

	*type = pkt.type;

	return 1;
}

static int icq_offline_message(session_t *s, unsigned char *buf, int len, private_data_t **info) {
	/*
	 * SNAC(15,03)/0041 SRV_OFFLINE_MESSAGE Offline message response
	 *
	 * This is the server response to CLI_OFFLINE_MSGS_REQ SNAC(15,02)/003C.
	 * This snac contain single offline message that was sent by another user
	 * and buffered by server when client was offline.
	 */
	struct {
		uint32_t uin;		/* message sender uin */

		uint16_t y;		/* year when message was sent (LE) */
		uint8_t M;		/* month when message was sent */
		uint8_t d;		/* day when message was sent */
		uint8_t h;		/* hour (GMT) when message was sent */
		uint8_t m;		/* minute when message was sent */

		uint8_t type;		/* message type */
		uint8_t flags;		/* message flags */

		uint16_t len;		/* message string length (LE) */
		char *msg;		/* message string (null-terminated) */
	} pkt;
	char *recode = NULL;
	char *uid;

	debug_function("icq_offline_message()\n");

	if (ICQ_UNPACK(&buf, "i wcccc cc w", &pkt.uin, &pkt.y, &pkt.M, &pkt.d, &pkt.h, &pkt.m, &pkt.type, &pkt.flags, &pkt.len)) {
		struct tm lt;
		lt.tm_sec	= 0;
		lt.tm_min	= pkt.m;
		lt.tm_hour	= pkt.h;
		lt.tm_mday	= pkt.d;
		lt.tm_mon	= pkt.M - 1;
		lt.tm_year	= pkt.y - 1900;
		lt.tm_isdst	= -1;

		recode = icq_convert_from_ucs2be((char *) buf, pkt.len - 1);
		if (!recode)
			recode = xstrdup((const char*)buf);

		uid = saprintf("icq:%u", pkt.uin);

		if (recode && *recode)
			protocol_message_emit(s, uid, NULL, recode, NULL, mktime(&lt), EKG_MSGCLASS_CHAT, NULL, EKG_TRY_BEEP, 0);

		xfree(uid);
		xfree(recode);
	}

	return 0;
}

static int icq_offline_message_end(session_t *s, unsigned char *buf, int len, private_data_t **info) {
	/*
	 * SNAC(15,03)/0042 SRV_END_OF_OFFLINE_MSGS End-of-offline messages reply
	 *
	 * This is the last SNAC in server response to CLI_OFFLINE_MSGS_REQ SNAC(15,02)/003C.
	 * It doesn't contain message - it is only end_of_sequence marker.
	 */
	debug_function("icq_offline_message_end()\n");

	/* SNAC(15,02)/003E CLI_DELETE_OFFLINE_MSGS_REQ Delete offline messages request
	 *
	 * Client sends this SNAC when wants to delete offline messages from
	 * server. But first you should request them from server using
	 * SNAC(15,02)/003C. If you doesn't delete messages server will send them
	 * again after client request.
	 */
	string_t pkt = string_init(NULL);
	icq_makemetasnac(s, pkt, CLI_DELETE_OFFLINE_MSGS_REQ, 0, NULL, NULL);
	icq_send_pkt(s, pkt);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_extension_replyreq) {
	/* SNAC(15,03) SRV_META_REPLY	Meta information response
	 *
	 * This is the server response to client meta request SNAC(15,02)
	 */
	int type = 0;
	private_data_t *info = NULL;

	debug_function("icq_snac_extension_replyreq()\n");

	if (!check_replyreq(s, &buf, &len, &type))
		return -1;

	private_item_set_int(&info, "uid", private_item_get_int(&data, "uid"));

	switch (type) {
		case SRV_OFFLINE_MESSAGE:		/* Offline message response */
			icq_offline_message(s, buf, len, &info);
			break;
		case SRV_END_OF_OFFLINE_MSGS:		/* End-of-offline messages reply */
			icq_offline_message_end(s, buf, len, &info);
			break;
		case SRV_META_INFO_REPLY:		/* Meta information response */
			icq_meta_info_reply(s, buf, len, &info, 1);
			break;
		default:
			debug_error("icq_snac_extension_replyreq() METASNAC with unknown code: %x received.\n", type);
			break;
	}

	private_items_destroy(&info);

	return 0;
}

SNAC_SUBHANDLER(icq_my_meta_information_response) {
	int type;
	icq_private_t *j = s->priv;

	debug_function("icq_my_meta_information_response()\n");

	if (!check_replyreq(s, &buf, &len, &type))
		return -1;

	private_item_set(&j->whoami, "uid", s->uid+4);

	switch (type) {
		case 0x7da:
			icq_meta_info_reply(s, buf, len, &j->whoami, 0);
			break;
		default:
			debug_error("icq_my_meta_information_response() METASNAC with unknown code: %x received.\n", type);
			break;
	}
	return 0;
}

void display_whoami(session_t *s) {
	icq_private_t *j = s->priv;
	int uid = private_item_get_int(&j->whoami, "uid");
	int end = 0;
	__displayed = 0;
	__display_info(s, META_BASIC_USERINFO,		j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_MORE_USERINFO,		j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_EMAIL_USERINFO,		j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_NOTES_USERINFO,		j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_HPAGECAT_USERINFO,	j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_WORK_USERINFO,		j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_INTERESTS_USERINFO,	j->whoami);
	end |= __displayed; __displayed = 0;
	__display_info(s, META_AFFILATIONS_USERINFO,	j->whoami);
	if (end)
		print("icq_userinfo_end", session_name(s), itoa(uid));
}


SNAC_HANDLER(icq_snac_extension_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_extension_error; break;
		case 0x03: handler = icq_snac_extension_replyreq; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_extension_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
		return 0;
	} else
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
