/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * A database of charsets (used for translation of filenames in .ZIP files and similiar)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "types.h"
#include "charsets.h"

const struct ocp_charset_info_t codepages_IBM[] = /* VGA fonts */
{
	{"CP437",            "Code page 437",             "Default IBM CGA/EGA/VGA hardware font"},
	{"CP737",            "Code page 737",             "Greek VGA font"},
	{"CP770",            "Code page 770",             "Estonian, Lithuanian and Latvian VGA font"},
	{"CP771",            "Code page 771",             "KBL - Lithuanian VGA font"},
	{"CP772",            "Code page 772/1119",        "LST 1284:1993 - Lithuanian VGA font"},
	{"CP773",            "Code page 773",             "Estonian, Lithuanian and Latvian VGA font"},
	{"CP774",            "Code page 774/1118",        "LST 1283 - Lithuanian VGA font"},
	{"CP775",            "Code page 775",             "LST 1590-1 - Estonian, Lithuanian and Latvian VGA font"},
//	{"CP776",            "Code page 776",             "Lithuanian VGA font"}, - missing in iconv
//	{"CP777",            "Code page 777",             "Lithuanian VGA font"}, - missing in iconv
//	{"CP778",            "Code page 778",             "LST 1590-2 - Lithuanian VGA font"}, - missing in iconv
//      {"CP790",            "Code page 790",             "Polish Mazovia encoding (cp667/cp790/cp991/MAZ)"}, - missing in iconv
	{"CP850",            "Code page 850",             "Western Europe VGA font"},
	{"CP851",            "Code page 851",             "Greek VGA font"},
	{"CP852",            "Code page 852",             "Central European VGA font"},
//	{"CP853",            "Code page 853",             "Turkish, Maltese, and Esperanto VGA font"}, - missing in iconv
	{"CP855",            "Code page 855",             "Cyrillic script VGA font"},
	{"CP856",            "Code page 856",             "Hebrew in Israel VGA font"},
	{"CP857",            "Code page 857",             "Turkish VGA font"},
	{"CP858",            "Code page 858",             "modified version of CP850 - used by PC DOS 2000"},
//	{"CP859",            "Code page 859",             "Western Europe VGA font"}, - missing in iconv
	{"CP860",            "Code page 860",             "Portuguese VGA font"},
	{"CP861",            "Code page 861",             "Icelandic VGA font"},
	{"CP862",            "Code page 862",             "Hebrew in Isreal VGA font"},
	{"CP863",            "Code page 863",             "French in Canada VGA font"},
	{"CP864",            "Code page 864",             "Arabic in Egypt, Iraq, Jordan, Saudi Arabia, and Syria VGA font"},
	{"CP865",            "Code page 865",             "Nordic languages VGA font"},
	{"CP866",            "Code page 866",             "Russian Cyrillic VGA font"},
//	{"CP867",            "Code page 867",             "Hebrew VGA font"}, - missing in iconv
	{"CP868",            "Code page 868",             "Urdu in Pakistan VGA font"},
	{"CP869",            "Code page 869",             "Greek VGA font"},
//      {"CP872",            "Code page 872",             "Same as code page 855, but with Euro sign"} - missing in iconv
	{"CP874",            "Code page 874",             "Thai VGA font"}, // Windows-1162
//	{"CP895",            "Code page 895",             "Czech and Slovak VGA font"} - missing in iconv
//	{"CP897",            "Code page 875",             "JIS X 0201 VGA font"} - missing in iconv
//	{"CP900",            "Code page 900",             "Unofficial Russian VGA font"} - missing in iconv
//	{"CP903",            "Code page 903",             "Single byte part of some Chinese standards"}, - implied via variable two byte encoding
//	{"CP904",            "Code page 904",             "Single byte part of some Thai standards"}, - implied via variable two byte encoding that is not present in iconv
	{"CP912",            "Code page 912",             "ISO 8859-2 modified by IBM in Albanian, Bosnian, Croatian, Czech, English, German, Hungarian, Polish, Romanian, Serbian (Latin alphabet), Slovak, and Slovene languages"},
//	{"CP907",            "Code page 907",             "APL programming language VGA font"} - missing in iconv
//	{"CP1040",           "Code page 1040",            "Korean PC Data Extended - Hangul"} - missing in iconv
//	{"CP1041",           "Code page 1041",            "Korean - Superseeds CP897"} - missing in iconv
	{"CP1046",           "Code page 1046",            "Used by IBM in Egypt, Iraq, Jordan, Saudi Arabia, and Syria for Arabic VGA font"},
        {"CP1124",           "Code page 1124",            "ISO 8859-2 modified for Macedonian VGA font"},
	{"CP1125",           "Code page 1125",            "RST 2018-91 - Ukrainian VGA font"},
	{"CP1129",           "Code page 1129",            "Vietnamese VGA font"},
	{"CP1131",           "Code page 1131",            "Belarusian VGA font"}, /* missing in iconv, but present in libiconv */
	{"CP1133",           "Code page 1133",            "Lao VGA font"},
	{"CP1163",           "Code page 1163",            "Vietnamese with euro VGA font"},
//	{"CP3012",           "Code page 3012",            "Latvian VGA font, used by FreeDOS"}, - missing in iconv
	{0, 0, 0}
};

const struct ocp_charset_info_t codepages_ISO8859[] =
{
	{"UTF-8",            "UTF-8",                     "Unicode"},
	{"ISO-8859-1",       "ISO 8859-1 Latin-1",        "Americas, Western Euroipe, Oceania and much of Africa"}, // Windows-28591
	{"ISO-8859-2",       "ISO 8859-2 Latin-2",        "Central/Eastern Europe"}, // Windows-28592
	{"ISO-8859-3",       "ISO 8859-3 Latin-3",        "Southern European"},// Windows-28593
	{"ISO-8859-4",       "ISO 8859-4 Latin-4",        "North European (Estonian, Latvian, Lithuanian, Greenlandic and Sami)"}, // Windows-28594
	{"ISO-8859-5",       "ISO 8859-5 Latin/Cyrillic", "Cyrillic alphabet such as Bulgarian, Belarusian, Russian, Serbian and Macedonian"}, // Windows-28595
	{"ISO-8859-6",       "ISO 8859-6 Latin/Arabic",   "Only nominal letters are encoded"}, // Windows-28596
	{"ISO-8859-7",       "ISO 8859-7 Latin/Greek",    "It was designed to cover the modern Greek language"}, // Windows-28597
	{"ISO-8859-8",       "ISO 8859-8 Latin/Hebrew",   "Covers all the Hebrew letters, but no Hebrew vowel signs"},
	{"ISO-8859-9",       "ISO 8859-9 Latin-5",        "Turkish"}, // Windows-28599
	{"ISO-8859-10",      "ISO 8859-10 Latin-6",       "Nordic languages"}, // Windows-28600
	{"ISO-8859-11",      "ISO 8859-11 Thai",          "TIS-620"}, // Windows-28601
	{"ISO-8859-13",      "ISO 8859-12 Latin/Celtic",  "Also used as Latin/Devanagari - ISO standard is abandoned"}, // Windows-28603
	{"ISO-8859-14",      "ISO 8859-13 Latin-7",       "Baltic languages"}, // Windows-28604
	{"ISO-8859-15",      "ISO 8859-14 Latin-8",       "Celtic languages such as Irish, Manx, Scottish Gaelic, Welsh, Cornish, and Breton."}, // Windows-28605
	{"ISO-8859-16",      "ISO 8859-15 Latin-9",       "Western European"},// Windows-28606
	{0, 0, 0}
};

const struct ocp_charset_info_t codepages_Mac[] =
{
	{"MACROMAN",         "Mac Roman",                 "(Code page 1275) Apple Roman"}, /* missing in glibc */
	{"MACGREEK",         "Mac Greek",                 "(Code page 1280) Apple Greek"}, /* missing in glibc */
	{"MACTURKISH",       "Mac Turkish",               "(Code page 1281) Apple Turkish"}, /* missing in glibc */
	{"MACCENTRALEUROPE", "Mac Central Europe",        "(Code page 1282) Apple Central European"}, /* glibc only has MAC-CENTRALEUROPE// */
	{"MACCYRILLIC",      "Mac Cyrillic",              "(Code page 1283) Apple Cyrillic"},
	{"MACCROATIAN",      "Mac Croatian",              "(Code page 1284) Apple Croatian"}, /* missing in glibc */
	{"MACROMANIA",       "Mac Romanian",              "(Code page 1285) Apple Romanian"}, /* missing in glibc */
	{"MACICELAND",       "Mac Iceland",               "(Code page 1286) Apple Icelandic"}, /* missing in glibc */
	{0, 0, 0}
};

const struct ocp_charset_info_t codepages_Windows[] =
{
	{"WINDOWS-1250",     "Windows-1250",              "Windows Central Europe"},
	{"WINDOWS-1251",     "Windows-1251",              "Windows Cyrillic"},
	{"WINDOWS-1252",     "Windows-1252",              "Windows Western / Latin"},
	{"WINDOWS-1253",     "Windows-1253",              "Windows Greek"},
	{"WINDOWS-1254",     "Windows-1254",              "Windows Turkish"},
	{"WINDOWS-1255",     "Windows-1255",              "Windows Hebrew"},
	{"WINDOWS-1256",     "Windows 1256",              "Windows Arabic language with some support for french diacritics"}, // ARABIC !!!!
	{"WINDOWS-1257",     "Windows-1257",              "Windows Baltic"},
	{"WINDOWS-1258",     "Windows-1258",              "Windows Vietnamese"},
	{"CP1361",           "Windows-1361",              "Microsoft Kodrean (JOHAB)"},
	{0, 0, 0}
};

const struct ocp_charset_info_t codepages_Arabic[] =
{
//	{"CP720",            "Code page 720",             "Arabic language used in Egypt, Iraq, Jordan, Saudi Arabia and Syria"} - missing in iconv
	{"ASMO_449",         "ASMO 449",                  "Arabic language - ISO-9036"}, /* I think the text will appear in the correct "reversed" order */
	{"ASMO-708",         "ASMO 708",                  "Arabic language"}, /* I think the text will appear in the correct "reversed" order */

#if 1
/* duplicated for simplicity */
	{"CP864",            "Code page 864",             "Arabic in Egypt, Iraq, Jordan, Saudi Arabia, and Syria VGA font"},
	{"CP1046",           "Code page 1046",            "Used by IBM in Egypt, Iraq, Jordan, Saudi Arabia, and Syria for Arabic VGA font"},
	{"ISO-8859-6",       "ISO 8859-6 Latin/Arabic",   "Only nominal letters are encoded"}, // Windows-28596
	{"WINDOWS-1256",     "Windows 1256",              "Windows Arabic language with some support for french diacritics"}, // ARABIC !!!!
#endif
	{0, 0, 0}
};


const struct ocp_charset_info_t codepages_Cyrillic[] =
{
	{"KOI-7",            "KOI-7 N1",                  "GOST 13052-67 7-bit Cyrillic alphabet."},
	{"KOI-8",            "KOI-8",                     "GOST 19768-74 8-bit, ASCII + Cyrillic alphabet."}, // libiconv includes KOI8-B in this
//	{"KOI8-B",           "KOI8-B,                     "KOI-8 + three missing characters"},

//	{"KOI8-C"  a proposal for Caucasus and Central Asia; hardly ever been used
//      {"KOI8-O",           "KOI-O",                     "(formerly KOI8-C) for Old Russian orthography"}, // missing in iconv

//	{"KOI8-CS",          "KOI8-CS",                   "Czech?  (ČSN (Czech technical standard) 369103, devised by the Comecon. This encoded Latin with diacritics, as used in Czech and Slovak, rather than Cyrillic, but the basic idea was the same - text should remain legible with the 8-th bit cleared, thus e.g. Č became C etc.)."} // missing in iconv
//	{"KOI8-CS2",         "KOI8-CS2",                  "Slovak? (ČSN (Czech technical standard) 369103, devised by the Comecon. This encoded Latin with diacritics, as used in Czech and Slovak, rather than Cyrillic, but the basic idea was the same - text should remain legible with the 8-th bit cleared, thus e.g. Č became C etc.). } // missing in iconv
	{"CSN_369103",       "KOI8-CS2",                  "ČSN (Czech technical standard) 369103, devised by the Comecon. This encoded Latin with diacritics, as used in Czech and Slovak, rather than Cyrillic."}, // missing in libiconv
	{"ECMACYRILLIC",     "KOI8-E",                    "ISO-IR-111 / ECMA-113"}, // missing in libiconv
//	{"KOI8-F",           "KOI8-F",                    "Only used by software from Fingertip Software - KOI8 Unified, includes all unique KOI-8 characters"}, // missing in libiconv
	{"ST_SEV_358-88",    "KOI8-K1",                   "Cyrillic-1 (defined in CSN 36 9103, ST SEV 358-88)"}, // missing in libiconv
	{"ISO-IR-139",       "KOI8-L2",                   "KOI8-L2 \"Latin-2\" (defined in CSN 36 9103), ISO IR 139 (almost identical to ISO 8859-2 (1987), but has the dollar sign and currency sign swapped)"}, // missing in libiconv
	{"KOI8-R",           "KOI8-R",                    "(Code page 878) RFC1489 Russian and Bulgarian"}, // Windows-20866
	{"KOI8-RU",          "KOI8-RU",                   "(Code page 1167) Ukrainian, Belorussian and Russian"},
	{"KOI8-T",           "KOI8-T",                    "Tajik"},
	{"KOI8-U",           "KOI8-U",                    "(Code page 1168) RFC2319 Ukrainian, Russian, Bulgarian"}, // Windows-21866
	{"MIK",              "MIK",                       "Bulgarian Pravetz 16"}, // missing in libiconv

#if 1
/* duplicated for simplicity */
	{"CP855",            "Code page 855",             "Cyrillic script VGA font"},
	{"CP866",            "Code page 866",             "Russian Cyrillic VGA font"},
	{"ISO-8859-5",       "ISO 8859-5 Latin/Cyrillic", "Cyrillic alphabet such as Bulgarian, Belarusian, Russian, Serbian and Macedonian"}, // Windows-28595
	{"MACCYRILLIC",      "Mac Cyrillic",              "(Code page 1283) Apple Cyrillic"},
	{"WINDOWS-1251",     "Windows-1251",              "Windows Cyrillic"},

#endif
	{0, 0, 0}
};


const struct ocp_charset_info_t codepages_Korean[] =
{
	{"CP949",            "Code page 949",             "Korean - Combination of CP1088 and double-byte CP951"}, /* 1 and 2 byte encoding */
	{"EUC-KR",           "Microsoft code page 51959", "(Code page 970) EUC-KR - Korean, Wansung"},
	{"MSCP949",          "Microsoft code page 949",   "(Code page 1363) Korean and Unified Hangul Code"},
	{0, 0, 0}
};

const struct ocp_charset_info_t codepages_Japanese[] =
{
//	{"CP897",            "Code page 897",             "IBM's implementation of the 8-bit form of JIS X 0201"} - Missing in iconv
	{"IBM932",           "IBM Code page 932",         "Combination of CP897 and double-byte CP301"}, /* 1 and 2 byte encoding */
	{"MS932",            "Microsoft code page 932",   "(Windows 31J) Microsoft Windows code page for the Japanese language"}, /* missing in libiconv, seems like 1 and 2 byte encoding */
//	{"IBM942",           "IBM Code page 942",         "Combination of CP1041 and double-byte CP943"}, /* 1 and 2 byte encoding */ - missing in iconv
	{"IBM943",           "IBM Code page 943",         "Combination of CP897 and double-byte CP943"}, /* 1 and 2 byte encoding */
	{"EUC-JISX0213",     "EUC-JIS-2004",              "JIS X 0213"},
	{"EUC-JP",           "EUC-JP",                    "(Code page 954) Microsoft 51932 and 20932 - Superseeds EUC-JIS-2004"},
	{"EUC-JISX0213",     "EUC-JISX0213",              "Superseeds EUC-JP"},
	{0, 0, 0}
};

const struct ocp_charset_info_t codepages_Chinese[] =
{
	{"CP936",            "Code page 936",             "Simplified Chinese, combination of CP903 and double-byte CP288"},
	{"CP950",            "Code page 950",             "Traditional Chinese combination of CP1114 and big5"},
	{"GB2312",           "GB 2312",                   "GB/T 2312-1980 - Simplified Chinese"},
	{"GBK",              "GBK",                       "Extended version of GB 2312"},
	{"MS936",            "Windows 936",               "Simplified Chinese, combination of CP1114 and double byte CP1385"}, /* not in libiconv */
	{"GB18030",          "GB 18030",                  "Simplified and traditional Chinese"},
	{"EUC-CN",           "EUC-CN",                    "Simplified Chinese, based on GB 3212"},

/* Taiwan */
	{"EUC-TW",           "EUC-TW",                    "Traditional Chinese characters as used in Taiwan"},
	{0, 0, 0}
};

const struct ocp_charset_collection_t charset_collections[] =
{
	{"IBM code pages / DOS",    codepages_IBM},
	{"ISO-8859-xx and unicode", codepages_ISO8859},
	{"Mac / Apple",             codepages_Mac},
	{"Windows",                 codepages_Windows},
	{"Arabic",                  codepages_Arabic},
	{"Cyrillic",                codepages_Cyrillic},
	{"Korean",                  codepages_Korean},
	{"Japanese",                codepages_Japanese},
	{"Chinese",                 codepages_Chinese},
	{0, 0}
};
