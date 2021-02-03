#ifndef _CHARSETS_H
#define _CHARSETS_H 1

struct ocp_charset_info_t
{
	const char *key;
	const char *label;
	const char *description;
};

struct ocp_charset_collection_t
{
	const char *label;
	const struct ocp_charset_info_t *entries; /* zero terminated */
};

extern const struct ocp_charset_collection_t charset_collections[];

#endif
