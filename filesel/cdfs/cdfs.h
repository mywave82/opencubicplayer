#ifndef CDFS_H
#define CDFS_H

#include "filesel/filesystem.h"

struct cdfs_disc_t;

#define SECTORSIZE 2048

// All this is specific for CD-ROM, not for DVD

#define SECTORSIZE_XA1 2056
#define SECTORSIZE_XA2 2352

/*                SYNC HEADER SUBHEADER DATA EDC RESERVED ECC   R-W       HEADER = 3 byte address + 1 byte mode
CDA                                     2352                    96

CLEAR                                                                     (mode byte = 0x00, rest of sector should be 0..)

MODE-1            12      4     -       2048  4      8   276    96        (mode byte = 0x01) DATA (Red Book defines the ECC)    The sync pattern for Mode 1 CDs is 0x00ffffffffffffffffffff00
MODE-2            12      4     -       2336  -      -    -     96        (mode byte = 0x02) AUDIO VIDEO/PICTURE-DATA    if XA MODE-2, TOC should contain this information

(XA allows audio and data to be written on the same disk/session)
XA MODE-2-FORM-1  12      4     8       2048  4      -   276    96        computer DATA
XA MODE-2-FORM-2  12      4     8       2324  4      -    -     96        mode byte = 0x02, XA set in TOC - compressed AUDIO VIDEO/PICTURE-DATA
                                                                          SUBHEADER byte 0,1,3 ?   byte 2 mask 0x20   false=>FORM_1, true =>FORM_2   byte 6 mask 0x20 should be a copy of byte 2 mask... if they differ, we have FORMLESS ?



For Audio, P channel contains the pause-trigger signal (being high the last two seconds of a track and the first frame of a new track)

Q channel contains:
  Q bit 4 (in the first byte per frame) can be used to mute audio, to avoid data to make noise
    timing indication for the cd-player to put onto the display.
    absolute sector indication used for locating sectors when tracking
   or can for a frame contain
    ISRC number (unique song number) at the start of an audio track
    or MCN number in the lead-in area, giving a unique CD ID number

CD-TEXT (artist per album or per track, titles + languages) is written in the R and W channel within the lead-in/TOC (first two seconds of the medium.. lead-in is not readable on most drives, but specific commands are provided to download them from the drive)


SUBHEADER:
struct SubHeader
{
  uint8 FileNumber; // Used for interleaving, 0 = no interleaving
  uint8 AudioChannel;
  uint8 EndOfRecord:1;
  uint8 VideoData:1; // true if the sector contains video data
  uint8 ADCPM:1; // Audio data encoded with ADPCM
  uint8 Data.1; // true if sector contains data
  uint8 TriggerOn:1; // OS dependent
  uint8 Form2:1; // true => form 2, false => form 1
  uint8 RealTime:1; // Sector contains real time data
  uint8 EndOfFile:1; // true if last sector of file
  uint8 Encoding; // Don't know what is this
};



SYNC = 00 ff ff ff ff ff ff ff ff ff 00
HEADER = 3 byte minute:second:from + 1 byte mode


mkisofs
-sectype data    logical sector size = 2048,   iso file sector size = 2048
-sectype xa1     logical sector size = 2048,   iso file sector size = 2056 - includes subheader   byte 2+6 is XA_SUBH_DATA - 0x08
-sectype raw     logical sector size = 2048,   iso file sector size = 2352 ???



Q-data is part of the 98 bytes per sector of control data, for time and track codes

Green-book:
CD-I (Interactive) interleaves audio and data sectors
*/

#include "iso9660.h"
#include "udf.h"

enum cdfs_format_t
{
	/* Can be any encoding - but can be limited to:
	 * Audio has 2352 bytes of audio samples
	 * MODE1_RAW has bytes: 12 x SYNC, 4 x HEADER, 2048 x DATA, 4 x EDC, 8 x Reserved and 276 x ECC
	 * MODE2_RAW has various encodings
	 */
	FORMAT_RAW___NONE                      =  0, /* autodection sometimes does not care... */
	FORMAT_RAW___RW                        =  1, /* AUDIO MODE1_RAW MODE1/2352 MODE2/2352 raw-files, CDA.. */
	FORMAT_RAW___RAW_RW                    =  2, /* AUDIO MODE1_RAW MODE1/2352 MODE2/2352 raw-files, CDA.. */
	                                             // *.cue: TRACK CDG   (Karaoke CD+G (sector size: 2448))
	FORMAT_AUDIO___NONE                    =  3, // *.toc: TRACK "file.bin" AUDIO
	                                             // *.cue: TRACK AUDIO    (Audio (sector size: 2352))
	FORMAT_AUDIO___RW                      =  4, // *.toc: TRACK "file.bin" AUDIO RW
	FORMAT_AUDIO___RAW_RW                  =  5, // *.toc: TRACK "file.bin" AUDIO RAW_RW

	FORMAT_AUDIO_SWAP___NONE               =  6, // *.toc: TRACK "file.bin" AUDIO SWAP
	FORMAT_AUDIO_SWAP___RW                 =  7, // *.toc: TRACK "file.bin" AUDIO RW SWAP
	FORMAT_AUDIO_SWAP___RAW_RW             =  8, // *.toc: TRACK "file.bin" AUDIO RAW_RW SWAP
	FORMAT_MODE1_RAW___NONE                =  9, // *.toc: TRACK "file.bin" MODE1_RAW
	                                             // *.cue: TRACK MODE1_RAW    (CD-ROM Mode 1 data (raw) (sector size: 2352), used by cdrdao)
	                                             // *.cue: TRACK MODE1/2352    (CD-ROM Mode 1 data (raw) (sector size: 2352))
	FORMAT_MODE1_RAW___RW                  = 10, // *.toc: TRACK "file.bin" MODE1_RAW RW
	FORMAT_MODE1_RAW___RAW_RW              = 11, // *.toc: TRACK "file.bin" MODE1_RAW RAW_RW
	FORMAT_MODE2_RAW___NONE                = 12, // *.toc: TRACK "file.bin" MODE2_RAW
	                                             // *.cue: TRACK MODE2_RAW    (CD-ROM Mode 2 data (raw) (sector size: 2352), used by cdrdao)
	                                             // *.cue: TRACK MODE2/2352    (CD-ROM Mode 2 data (raw) (sector size: 2352))
	                                             // *.cue: TRACK CDI/2352    (CDI Mode 2 data)
	FORMAT_MODE2_RAW___RW                  = 13, // *.toc: TRACK "file.bin" MODE2_RAW RW
	FORMAT_MODE2_RAW___RAW_RW              = 14, // *.toc: TRACK "file.bin" MODE2_RAW RAW_RW
	FORMAT_XA_MODE2_RAW                    = 15, /* TOC should indicate this.... */
	FORMAT_XA_MODE2_RAW___RW               = 16, /* TOC should indicate this.... */
	FORMAT_XA_MODE2_RAW___RAW_RW           = 17, /* TOC should indicate this.... */

	/* All of these modes appears to the high-level API as the same - 2048 bytes of datasectors */
	/* 2048 bytes */
	FORMAT_MODE1___NONE                    = 18, // *.toc: TRACK "file.bin" MODE1
	                                             // *.cue: TRACK MODE1/2048    (CD-ROM Mode 1 data (cooked) (sector size: 2048))
	FORMAT_MODE1___RW                      = 19, // *.toc: TRACK "file.bin" MODE1 RW
	FORMAT_MODE1___RAW_RW                  = 20, // *.toc: TRACK "file.bin" MODE1 RAW_RW
	FORMAT_XA_MODE2_FORM1___NONE           = 21, // *.toc: TRACK "file.bin" MODE2_FORM1
	                                             // *.cue: TRACK MODE2/2048    (CD-ROM Mode 2 XA form-1 data (sector size: 2048)
	FORMAT_XA_MODE2_FORM1___RW             = 22, // *.toc: TRACK "file.bin" MODE2_FORM1 RW
	FORMAT_XA_MODE2_FORM1___RAW_RW         = 23, // *.toc: TRACK "file.bin" MODE2_FORM1 RAW_RW
	FORMAT_MODE_1__XA_MODE2_FORM1___NONE   = 24, // mkisofs and iso files in general
	FORMAT_MODE_1__XA_MODE2_FORM1___RW     = 25,
	FORMAT_MODE_1__XA_MODE2_FORM1___RAW_RW = 26,

	/* 2336 bytes sector, known as MODE-2, used by AUDIO VIDEO/PICTURE-DATA */
	FORMAT_MODE2___NONE                    = 27, // *.toc: TRACK "file.bin" MODE2
	                                             // *.cue: TRACK MODE2/2336    (CD-ROM Mode 2 data (sector size: 2336))
	                                             // *.cue: TRACK CDI/2336    (CDI Mode 2 data)
	FORMAT_MODE2___RW                      = 28, // *.toc: TRACK "file.bin" MODE2 RW
	FORMAT_MODE2___RAW_RW                  = 29, // *.toc: TRACK "file.bin" MODE2 RAW_RW

	/* 2324 bytes sector, known as MODE-2 FORM-2, used by compressed AUDIO VIDEO/PICTURE-DATA */
	FORMAT_XA_MODE2_FORM2___NONE           = 30, // *.toc: TRACK "file.bin" MODE2_FORM2
	                                             // *.toc: TRACK MODE2/2324    (CD-ROM Mode 2 XA form-2 data (sector size: 2324))
	FORMAT_XA_MODE2_FORM2___RW             = 31, // *.toc: TRACK "file.bin" MODE2_FORM2 RW
	FORMAT_XA_MODE2_FORM2___RAW_RW         = 32, // *.toc: TRACK "file.bin" MODE2_FORM2 RAW_RW

	/* 4 bytes header + 8 bytes of subheader and either MODE2-FORM-1 + padding or MODE2-FORM-2 data */
	FORMAT_XA_MODE2_FORM_MIX___NONE        = 33, // *.toc: TRACK "file.bin" MODE2_FORM_MIX
	FORMAT_XA_MODE2_FORM_MIX___RW          = 34, // *.toc: TRACK "file.bin" MODE2_FORM_MIX RW
	FORMAT_XA_MODE2_FORM_MIX___RAW_RW      = 35, // *.toc: TRACK "file.bin" MODE2_FORM_MIX RAW_RW

	/* 8 bytes SUBHEADER + 2048 bytes of MODE2_FORM1 2048 bytes of data */
	/* 2056 bytes */
	FORMAT_XA1_MODE2_FORM1___NONE          = 250, // mkisofs -sectype xa1
	FORMAT_XA1_MODE2_FORM1___RW            = 251, // mkisofs -sectype xa1
	FORMAT_XA1_MODE2_FORM1___RW_RAW        = 252, // mkisofs -sectype xa1

	FORMAT_ERROR           = 255,
};

struct ocpfilehandle_t;

struct cdfs_datasource_t
{
	uint32_t sectoroffset; /* offset into disc */
	uint32_t sectorcount;  /* number of sectors */
	struct ocpfile_t       *file;
	struct ocpfilehandle_t *fh;
	enum cdfs_format_t format;
	uint64_t offset;       /* given in bytes */
	uint64_t length;       /* given in bytes */
};

struct cdfs_track_t
{	/* all units given in CD sectors */
	uint32_t pregap;
	uint32_t start;
	uint32_t length; /* including pregap */

	char *title;
	char *performer;
	char *songwriter;
	char *composer;
	char *arranger;
	char *message;
};

struct cdfs_instance_dir_t
{
	struct ocpdir_t     head;
	struct cdfs_disc_t *owner;
	uint32_t            dir_parent; /* used for making blob */
	uint32_t            dir_next;
	uint32_t            dir_child;
	uint32_t            file_child;
	//char             *orig_full_dirpath; /* if encoding deviates from UTF-8 */
};

struct cdfs_instance_file_extent
{
	uint32_t location; // UINT32_MAX for zero-fill
	uint32_t count;
	uint16_t skip_start;
//	uint16_t skip_end;
};

struct cdfs_instance_file_t
{
// We need to store information about expected sector XA formats...
	struct ocpfile_t                  head;
	struct cdfs_disc_t               *owner;
	uint32_t                          dir_parent; /* used for making blob */
	uint32_t                          file_next;
	uint64_t                          filesize;
	int                               extents;
	struct cdfs_instance_file_extent *extent;
	//struct cdfs_instance_file_extent  inline_extent;
	//char                           *orig_full_filepath; /* if encoding deviates from UTF-8 */
	char                             *filenameshort;
	int                               audiotrack;
};

struct musicbrainz_database_h;
struct cdfs_disc_t
{
	struct cdfs_disc_instance_t       *next; // TODO

	struct cdfs_instance_dir_t       **dirs;
	struct cdfs_instance_dir_t         dir0;
	int                                dir_fill;
	int                                dir_size;
	struct cdfs_instance_file_t      **files;
        int                                file_fill;
	int                                file_size;

	int                                refcount;
	//int                                iorefcount;

	// CDROM-AUDIO START
        void                          *musicbrainzhandle;
	struct musicbrainz_database_h *musicbrainzdata;
	char                          *discid;
	char                          *toc;
	// CDROM-AUDIO STOP

	int                       datasources_count; /* these are normally bound to a session, but easier to have them listed here */
	struct cdfs_datasource_t *datasources_data;

	int                       tracks_count;
	struct cdfs_track_t       tracks_data[100]; /* track 0 is for global text-info only */

	/* can in theory be multiple sessions.... */
	struct ISO9660_session_t *iso9660_session;

	/* One UDF session can in theory cross sessions on disc */
	struct UDF_Session       *udf_session;
};

void cdfs_disc_datasource_append (struct cdfs_disc_t     *disc,
                                  uint32_t                sectoroffset,
                                  uint32_t                sectorcount,
                                  struct ocpfile_t       *file,
                                  struct ocpfilehandle_t *fh,
                                  enum cdfs_format_t      format,
                                  uint64_t                offset,
                                  uint64_t                length);

void cdfs_disc_track_append (struct cdfs_disc_t *disc,
                             uint32_t            pregap,
                             uint32_t            offset,
                             uint32_t            length,
                             const char         *title,
                             const char         *performer,
                             const char         *songwriter,
                             const char         *composer,
                             const char         *arranger,
                             const char         *message);

struct cdfs_disc_t *cdfs_disc_new (struct ocpfile_t *file);
void cdfs_disc_ref (struct cdfs_disc_t *self);
void cdfs_disc_unref (struct cdfs_disc_t *self);

int cdfs_fetch_absolute_sector_2048 (struct cdfs_disc_t *disc, uint32_t sector, uint8_t *buffer); /* 2048 byte modes */
int cdfs_fetch_absolute_sector_2352 (struct cdfs_disc_t *disc, uint32_t sector, uint8_t *buffer); /* 2352 byte modes */

int detect_isofile_sectorformat (struct ocpfilehandle_t *isofile_fh,
                                 const char *filename,
                                 off_t st_size,
                                 enum cdfs_format_t *isofile_format,
                                 uint32_t *isofile_sectorcount);

enum cdfs_format_t cdfs_get_sector_format (struct cdfs_disc_t *disc, uint32_t sector);

/* returns 0 on errors */
uint32_t CDFS_Directory_add (struct cdfs_disc_t *disc, const uint32_t dir_parent_handle, const char *Dirname);

/* returns UINT32_MAX on errors */
uint32_t CDFS_File_add (struct cdfs_disc_t *self, const uint32_t dir_parent_handle, char *Filename);
void     CDFS_File_zeroextent (struct cdfs_disc_t *disc, uint32_t handle, uint64_t length);
void     CDFS_File_extent (struct cdfs_disc_t *disc, uint32_t handle, uint32_t location, uint64_t length, int sector_skip);
uint32_t CDFS_File_add_audio (struct cdfs_disc_t *self, const uint32_t dir_parent_handle, char *FilenameShort, char *FilenameLong, uint_fast32_t filesize, int audiotrack);

#endif
