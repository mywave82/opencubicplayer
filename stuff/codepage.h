#ifndef _STUFF_CODEPAGE_H
#define _STUFF_CODEPAGE_H 1

struct codepage_database_entry
{
	const char *internal_name;
	const char *display_name;
	const char *description;
	const uint8_t undefined[32];
	/* 0 = undefined[0] 0x01
           1 = undefined[0] 0x02
           7 = undefined[0] 0x80
          16 = undefined[2] 0x01
        */
}

struct codepage_database
{
	const char *title;
	const codepage_database_entry **entries;
};

struct codepage_databases
{
	struct codepage_database **database;
};


struct codepage_database  codepage_all;
struct codepage_databases codepage_databases;

#endif
