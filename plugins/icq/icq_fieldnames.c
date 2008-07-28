// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
// 
// Copyright Š 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright Š 2001-2002 Jon Keating, Richard Hughes
// Copyright Š 2002-2004 Martin  berg, Sam Kothari, Robert Rainwater
// Copyright Š 2004-2008 Joe Kucera
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// File name      : $URL: http://miranda.svn.sourceforge.net/svnroot/miranda/trunk/miranda/protocols/IcqOscarJ/icq_fieldnames.cpp $
// Revision       : $Revision: 7484 $
// Last change by : $Author: ghazan $

struct fieldnames_t {
	int code;
	char *text;
};

#define LPGEN(x) x

struct fieldnames_t interestsField[]={
	{100, LPGEN("Art")},
	{101, LPGEN("Cars")},
	{102, LPGEN("Celebrity Fans")},
	{103, LPGEN("Collections")},
	{104, LPGEN("Computers")},
	{105, LPGEN("Culture & Literature")},
	{106, LPGEN("Fitness")},
	{107, LPGEN("Games")},
	{108, LPGEN("Hobbies")},
	{109, LPGEN("ICQ - Providing Help")},
	{110, LPGEN("Internet")},
	{111, LPGEN("Lifestyle")},
	{112, LPGEN("Movies/TV")},
	{113, LPGEN("Music")},
	{114, LPGEN("Outdoor Activities")},
	{115, LPGEN("Parenting")},
	{116, LPGEN("Pets/Animals")},
	{117, LPGEN("Religion")},
	{118, LPGEN("Science/Technology")},
	{119, LPGEN("Skills")},
	{120, LPGEN("Sports")},
	{121, LPGEN("Web Design")},
	{122, LPGEN("Nature and Environment")},
	{123, LPGEN("News & Media")},
	{124, LPGEN("Government")},
	{125, LPGEN("Business & Economy")},
	{126, LPGEN("Mystics")},
	{127, LPGEN("Travel")},
	{128, LPGEN("Astronomy")},
	{129, LPGEN("Space")},
	{130, LPGEN("Clothing")},
	{131, LPGEN("Parties")},
	{132, LPGEN("Women")},
	{133, LPGEN("Social science")},
	{134, LPGEN("60's")},
	{135, LPGEN("70's")},
	{136, LPGEN("80's")},
	{137, LPGEN("50's")},
	{138, LPGEN("Finance and corporate")},
	{139, LPGEN("Entertainment")},
	{140, LPGEN("Consumer electronics")},
	{141, LPGEN("Retail stores")},
	{142, LPGEN("Health and beauty")},
	{143, LPGEN("Media")},
	{144, LPGEN("Household products")},
	{145, LPGEN("Mail order catalog")},
	{146, LPGEN("Business services")},
	{147, LPGEN("Audio and visual")},
	{148, LPGEN("Sporting and athletic")},
	{149, LPGEN("Publishing")},
	{150, LPGEN("Home automation")},
	{-1,  NULL}};

struct fieldnames_t languageField[]={
	{1, LPGEN("Arabic")},
	{2, LPGEN("Bhojpuri")},
	{3, LPGEN("Bulgarian")},
	{4, LPGEN("Burmese")},
	{5, LPGEN("Cantonese")},
	{6, LPGEN("Catalan")},
	{7, LPGEN("Chinese")},
	{8, LPGEN("Croatian")},
	{9, LPGEN("Czech")},
	{10, LPGEN("Danish")},
	{11, LPGEN("Dutch")},
	{12, LPGEN("English")},
	{13, LPGEN("Esperanto")},
	{14, LPGEN("Estonian")},
	{15, LPGEN("Farci")},
	{16, LPGEN("Finnish")},
	{17, LPGEN("French")},
	{18, LPGEN("Gaelic")},
	{19, LPGEN("German")},
	{20, LPGEN("Greek")},
	{21, LPGEN("Hebrew")},
	{22, LPGEN("Hindi")},
	{23, LPGEN("Hungarian")},
	{24, LPGEN("Icelandic")},
	{25, LPGEN("Indonesian")},
	{26, LPGEN("Italian")},
	{27, LPGEN("Japanese")},
	{28, LPGEN("Khmer")},
	{29, LPGEN("Korean")},
	{30, LPGEN("Lao")},
	{31, LPGEN("Latvian")},
	{32, LPGEN("Lithuanian")},
	{33, LPGEN("Malay")},
	{34, LPGEN("Norwegian")},
	{35, LPGEN("Polish")},
	{36, LPGEN("Portuguese")},
	{37, LPGEN("Romanian")},
	{38, LPGEN("Russian")},
	{39, LPGEN("Serbo-Croatian")},
	{40, LPGEN("Slovak")},
	{41, LPGEN("Slovenian")},
	{42, LPGEN("Somali")},
	{43, LPGEN("Spanish")},
	{44, LPGEN("Swahili")},
	{45, LPGEN("Swedish")},
	{46, LPGEN("Tagalog")},
	{47, LPGEN("Tatar")},
	{48, LPGEN("Thai")},
	{49, LPGEN("Turkish")},
	{50, LPGEN("Ukrainian")},
	{51, LPGEN("Urdu")},
	{52, LPGEN("Vietnamese")},
	{53, LPGEN("Yiddish")},
	{54, LPGEN("Yoruba")},
	{55, LPGEN("Afrikaans")},
	{56, LPGEN("Bosnian")},
	{57, LPGEN("Persian")},
	{58, LPGEN("Albanian")},
	{59, LPGEN("Armenian")},
	{60, LPGEN("Punjabi")},
	{61, LPGEN("Chamorro")},
	{62, LPGEN("Mongolian")},
	{63, LPGEN("Mandarin")},
	{64, LPGEN("Taiwaness")},
	{65, LPGEN("Macedonian")},
	{66, LPGEN("Sindhi")},
	{67, LPGEN("Welsh")},
	{68, LPGEN("Azerbaijani")},
	{69, LPGEN("Kurdish")},
	{70, LPGEN("Gujarati")},
	{71, LPGEN("Tamil")},
	{72, LPGEN("Belorussian")},
	{-1, NULL}};

struct fieldnames_t pastField[]={
	{300, LPGEN("Elementary School")},
	{301, LPGEN("High School")},
	{302, LPGEN("College")},
	{303, LPGEN("University")},
	{304, LPGEN("Military")},
	{305, LPGEN("Past Work Place")},
	{306, LPGEN("Past Organization")},
	{399, LPGEN("Other")},
	{-1,  NULL}};

struct fieldnames_t genderField[]={
	{1, LPGEN("Female")},
	{2, LPGEN("Male")},
	{-1,  NULL}};

struct fieldnames_t workField[]={
	{1, LPGEN("Academic")},
	{2, LPGEN("Administrative")},
	{3, LPGEN("Art/Entertainment")},
	{4, LPGEN("College Student")},
	{5, LPGEN("Computers")},
	{6, LPGEN("Community & Social")},
	{7, LPGEN("Education")},
	{8, LPGEN("Engineering")},
	{9, LPGEN("Financial Services")},
	{10, LPGEN("Government")},
	{11, LPGEN("High School Student")},
	{12, LPGEN("Home")},
	{13, LPGEN("ICQ - Providing Help")},
	{14, LPGEN("Law")},
	{15, LPGEN("Managerial")},
	{16, LPGEN("Manufacturing")},
	{17, LPGEN("Medical/Health")},
	{18, LPGEN("Military")},
	{19, LPGEN("Non-Government Organization")},
	{20, LPGEN("Professional")},
	{21, LPGEN("Retail")},
	{22, LPGEN("Retired")},
	{23, LPGEN("Science & Research")},
	{24, LPGEN("Sports")},
	{25, LPGEN("Technical")},
	{26, LPGEN("University Student")},
	{27, LPGEN("Web building")},
	{99, LPGEN("Other services")},
	{-1,  NULL}};

struct fieldnames_t affiliationField[]={
	{200, LPGEN("Alumni Org.")},
	{201, LPGEN("Charity Org.")},
	{202, LPGEN("Club/Social Org.")},
	{203, LPGEN("Community Org.")},
	{204, LPGEN("Cultural Org.")},
	{205, LPGEN("Fan Clubs")},
	{206, LPGEN("Fraternity/Sorority")},
	{207, LPGEN("Hobbyists Org.")},
	{208, LPGEN("International Org.")},
	{209, LPGEN("Nature and Environment Org.")},
	{210, LPGEN("Professional Org.")},
	{211, LPGEN("Scientific/Technical Org.")},
	{212, LPGEN("Self Improvement Group")},
	{213, LPGEN("Spiritual/Religious Org.")},
	{214, LPGEN("Sports Org.")},
	{215, LPGEN("Support Org.")},
	{216, LPGEN("Trade and Business Org.")},
	{217, LPGEN("Union")},
	{218, LPGEN("Volunteer Org.")},
	{299, LPGEN("Other")},
	{-1,  NULL}};

struct fieldnames_t agesField[]={
	{0x0011000D, LPGEN("13-17")},
	{0x00160012, LPGEN("18-22")},
	{0x001D0017, LPGEN("23-29")},
	{0x0027001E, LPGEN("30-39")},
	{0x00310028, LPGEN("40-49")},
	{0x003B0032, LPGEN("50-59")},
	{0x2710003C, LPGEN("60-above")},
	{-1,         NULL}};

struct fieldnames_t maritalField[]={
	{10, LPGEN("Single")},
	{11, LPGEN("Close relationships")},
	{12, LPGEN("Engaged")},
	{20, LPGEN("Married")},
	{30, LPGEN("Divorced")},
	{31, LPGEN("Separated")},
	{40, LPGEN("Widowed")},
	{-1, NULL}};

static const char *icq_lookuptable(struct fieldnames_t *table, int code) {
	int i;

	if (code == 0)
		return NULL;

	for(i = 0; table[i].code != -1 && table[i].text; i++) {
		if (table[i].code == code)
			return table[i].text;
	}

	debug_error("icq_lookuptable() invalid lookup: %x\n", code);
	return NULL;
}
