#ifndef ISO9660_H
#define ISO9660_H

/* these are for struct iso_dirent_t.Flags */
#define ISO9660_DIRENT_FLAGS_HIDDEN                      0x01
#define ISO9660_DIRENT_FLAGS_DIR                         0x02 /* Files has this flagged cleared */
#define ISO9660_DIRENT_FLAGS_ASSOCIATED_FILE             0x04 /* ???? */
#define ISO9660_DIRENT_FLAGS_EXTENDED_ATTRIBUTES_PRESENT 0x08
#define ISO9660_DIRENT_FLAGS_PERMISSIONS_PRESENT         0x10
#define ISO9660_DIRENT_FLAGS_FILE_NOT_LAST_EXTENT        0x80 /* Allows a file to be made out of several extents */

/* these are for struct iso_dirent_t.XA_attr - which is an optional extension */
#define XA_ATTR__OWNER_READ  0x0001
// WRITE could have been     0x0002
#define XA_ATTR__OWNER_EXEC  0x0004
#define XA_ATTR__GROUP_READ  0x0010
// WRITE could have been     0x0020
#define XA_ATTR__GROUP_EXEC  0x0040
#define XA_ATTR__OTHER_READ  0x0100
// WRITE could have been     0x0200
#define XA_ATTR__OTHER_EXEC  0x0400
#define XA_ATTR__MODE2_FORM1 0x0800
#define XA_ATTR__MODE2_FORM2 0x1000
#define XA_ATTR__INTERLEAVED 0x2000
#define XA_ATTR__CDDA        0x4000
#define XA_ATTR__DIR         0x8000


#include <stdint.h>


/* We use a common decoded struct for both the binary 7 and 17 bytes ISO9660 date/time format */
struct iso9660_datetime_t
{
	uint16_t year;   /* normal range */
	uint8_t  month;  /* 0-11 */
	uint8_t  day;    /* 1-31 */
	uint8_t  hour;   /* 0-23 */
	uint8_t  minute; /* 0-59 */
	uint8_t  second; /* 0-59 */
	int16_t  tz;
};

/* Container for a directory entry, holds data found in parsed format for most parts. Not all data is stored, just the ones needed for further display */
struct iso_dirent_t
{
	struct iso_dirent_t *next_extent; /* Large files can be concatinated by multiple entries */
      //Extended Attribute Length: 0
	uint32_t Absolute_Location;
	uint32_t Length;
	uint8_t  Flags;

	uint8_t XA; /* we got it? */
	uint16_t XA_GID;
	uint16_t XA_UID;
	uint16_t XA_attr;

	//uint8_t InterLeave_Unit_Size; ??
	//uint8_t InterLeave_Gap_Size; ??
	uint16_t Volume_Sequence;
	uint8_t  Name_ISO9660_Length;
	uint8_t  Name_ISO9660[256];
	uint32_t Name_RockRidge_Length;
	uint8_t *Name_RockRidge;
	struct iso9660_datetime_t Created;

	uint8_t               RockRidge_TF_Created_Present;
	struct iso9660_datetime_t RockRidge_TF_Created;

	uint8_t  RockRidge_PX_Present;
	uint32_t RockRidge_PX_st_mode;
	uint32_t RockRidge_PX_st_uid;
	uint32_t RockRidge_PX_st_gid;

	uint8_t  RockRidge_PN_Present;
	uint32_t RockRidge_PN_major;
	uint32_t RockRidge_PN_minor;

	uint32_t RockRidge_Symlink_Components_Length;
	uint8_t *RockRidge_Symlink_Components; /* Needs processing */

	uint8_t  RockRidge_DirectoryIsRedirected; /* Do not display this entry */
	uint8_t  RockRidge_DotDotIsRedirected; /* Can only be set in a .. entry, and it overrides the Absolute_Location */
	uint8_t  RockRidge_IsAugmentedDirectory; /* This is not a file, but a directory.. Use attributes from the RockRidge_AugmentedDirectoryFrom . entry instead of attributes that are here, except the Name_* attributes */

	uint32_t RockRidge_DotDotRedirectedTo;
	uint32_t RockRidge_AugmentedDirectoryFrom;
};

/* A directory container - First entry is reserved for "." and second entry for ".." */
struct iso_dir_t
{
	uint32_t Location;
	int dirents_count;
	int dirents_size;
	struct iso_dirent_t **dirents_data;
};

/* Temporary container for unscanned directories */
struct iso_dir_queue
{
	uint32_t Location;
	uint32_t Length;
	int isrootnode;
};

struct cdfs_disc_t;

void ISO9660_Descriptor (struct cdfs_disc_t *disc, uint8_t buffer[SECTORSIZE], const int sector, const int descriptor, int *descriptorend);

struct Volume_Description_t
{
	struct iso_dirent_t root_dirent;

/* All entries are currently for ISO9660.....*/
	uint8_t SystemUse_Skip; /* used by the SUSP parser - default is zero, unless XA1 - can be overriden in SP entry */
	uint8_t XA1;            /* used by the SUSP parser */
	uint8_t RockRidge; /* Any RockRidge SUSP data found? */

	uint8_t UTF8;  /* Name_ISO9660 */
	uint8_t UTF16; /* Name_ISO9660 */

	int               directories_count;
	int               directories_size;
	struct iso_dir_t *directories_data;

	int                   directory_scan_queue_count;
	int                   directory_scan_queue_size;
	struct iso_dir_queue *directory_scan_queue_data;
};

void Volume_Description_Free (struct Volume_Description_t *volume_desc);

struct ISO9660_session_t
{ /* in theory there might be multiple sessions on the disc, but only the last one easy to detect */
	struct Volume_Description_t *Primary_Volume_Description;
	struct Volume_Description_t *Supplementary_Volume_Description;	
};

void ISO9660_Session_Free (struct ISO9660_session_t **s);

#ifdef CDFS_DEBUG
void DumpFS_dir_ISO9660 (struct Volume_Description_t *vd, const char *name, uint32_t Location);

void DumpFS_dir_RockRidge (struct Volume_Description_t *vd, const char *name, uint32_t Location);

void DumpFS_dir_Joliet (struct Volume_Description_t *vd, const char *name, uint32_t Location);
#endif

void CDFS_Render_ISO9660 (struct cdfs_disc_t *disc, uint32_t parent_directory); /* parent_directory should point to "ISO9660" */

void CDFS_Render_Joliet (struct cdfs_disc_t *disc, uint32_t parent_directory); /* parent_directory should point to "Joliet" */

void CDFS_Render_RockRidge (struct cdfs_disc_t *disc, uint32_t parent_directory); /* parent_directory should point to "RockRidge" */


#endif
