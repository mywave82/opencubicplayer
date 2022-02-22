#ifndef _UDF_H
#define _UDF_H

struct cdfs_disc_t;

struct UDF_extent_ad // 3/7.1 - 8 bytes on disc
{
	uint32_t ExtentLength;
	uint32_t ExtentLocation;
};

struct UDF_lbaddr // 4/7.1 - 6 bytes on disc: A logical block address
{
	uint32_t LogicalBlockNumber;
	uint16_t PartitionReferenceNumber;
};

enum eExtentInterpretation
{
	ExtentRecordedAndAllocated = 0,
	ExtentNotRecordedButAllocated = 1,
	ExtentNotRecordedAndNotAllocated = 2,
	ExtentIsTheNextExtentOfAllocationDescriptors = 3,
};

struct UDF_shortad // 4/14.14.1 - 8 bytes on disc
{
	uint32_t                   ExtentLength;         /* top two bits are to be moved into Interpratation */
	uint32_t                   ExtentPosition;
	enum eExtentInterpretation ExtentInterpretation; /* taken from the top two bits of the Length */
};


struct UDF_longad // 4/14.14.2 - 16 bytes on disc
{
	uint32_t          ExtentLength;
	struct UDF_lbaddr ExtentLocation;
	uint8_t           ExtentErased; /* stored in Flags (AdImpUse.flags), within 6 bytes of ImplementationUse */
};

struct UDF_extad // 4/14.14.3 - 20 bytes on disc
{
	uint32_t          ExtentLength;
	uint32_t          RecordedLength;
	uint32_t          InformationLength;
	struct UDF_lbaddr ExtentLocation;
	/* 2 bytes of ImplementationUse - not specified further in the UDF spec, could be inteprated as flags => ExtentErased */
};

struct UDF_PrimaryVolumeDescriptor
{
	uint32_t VolumeDescriptorSequenceNumber;
	char *VolumeIdentifier;
	uint16_t VolumeSequenceNumber;
};
struct UDF_LogicalVolumes_t;

enum PhysicalPartition_Content
{
	PhysicalPartition_Content_Unknown = 0,
	PhysicalPartition_Content_ECMA107__FDC01 = 1,
	PhysicalPartition_Content_ECMA119__CD001 = 2,
	PhysicalPartition_Content_ECMA167__NSR02 = 3,
	PhysicalPartition_Content_ECMA167__NSR03 = 4,
	PhysicalPartition_Content_ECMA168__CDW02 = 5
};

struct UDF_Partition_Common
{
	int (*Initialize)(struct cdfs_disc_t *disc, struct UDF_Partition_Common *self);
	int (*FetchSector)(struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint8_t *buffer, uint32_t sector);
	void (*PushAbsoluteLocations)(struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t sector, uint32_t length, int skiplength, int handle);
	void (*Free)(void *self);

	void (*DefaultSession)(struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12]);
	int (*SelectSession)(struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t LocationIterator/*, uint8_t TimeStamp[12]*/);
	int (*NextSession)(struct cdfs_disc_t *disc, struct UDF_Partition_Common *self, uint32_t *LocationIterator, uint8_t TimeStamp[12]);
};

struct UDF_PhysicalPartition_t
{
	struct UDF_Partition_Common PartitionCommon;
	uint32_t VolumeDescriptorSequenceNumber;
	uint16_t PartitionNumber;

	enum PhysicalPartition_Content Content;
	uint32_t SectorSize; /* Will be 2048 for all ISO files */
	uint32_t Start;
	uint32_t Length; /* Given in sectors */
};

struct UDF_LogicalVolume_Common
{
	struct UDF_Partition_Common PartitionCommon;
	uint16_t PartId; /* The actual index in the map */
	uint8_t Type; /* Type 2 partitions needs to find their Type1 host */
	uint8_t IsSpareablePartitionMap;
	char Info[96];
};

struct UDF_LogicalVolume_Type1;
struct UDF_LogicalVolume_Type2_VAT;

struct UDF_LogicalVolume_Type1
{
	struct UDF_LogicalVolume_Common Common;

	int (*FetchSectorReal)(struct cdfs_disc_t *disc, void *self, uint8_t *buffer, uint32_t sector); /* The real one */

	uint16_t                            PhysicalPartition_VolumeSequenceNumber; /* only used for multiple image files */
	uint16_t                            PhysicalPartition_PartitionNumber;
	struct UDF_PhysicalPartition_t     *PhysicalPartition;

	struct UDF_LogicalVolume_Type2_VAT *VAT; /* The VAT attaches itself, and virtual API can use it if attached */

	int Initialized;
};

struct UDF_VAT_Entry
{
	uint32_t RemappedTo;
};
struct UDF_VAT_Entries;
struct UDF_VAT_Entries
{
	uint32_t VAT_Location;
	uint32_t Length;
	uint8_t  TimeStamp_1_7_3[12];
	struct UDF_VAT_Entry *Entries;
	struct UDF_VAT_Entries *Previous;
};
struct UDF_LogicalVolume_Type2_VAT
{
	struct UDF_LogicalVolume_Common Common;
	uint16_t                            PhysicalPartition_VolumeSequenceNumber;  /* only used for multiple image files */
	uint16_t                            PhysicalPartition_PartitionNumber;
	struct UDF_PhysicalPartition_t     *PhysicalPartition;
	struct UDF_LogicalVolume_Type1     *LogicalVolume_Type1;

	int Initialized;
	struct UDF_VAT_Entries              RootEntry;
	struct UDF_VAT_Entries             *ActiveEntry;
};

struct UDF_SparingTable_MapEntry_t
{
	uint32_t OriginalLocation; /* Virtual Location */
 	uint32_t MappedLocation; /* Physical Location */
};

struct UDF_LogicalVolume_Type2_SparingPartition
{
	struct UDF_LogicalVolume_Common Common;

	uint16_t VolumeSequenceNumber;
	uint16_t PhysicalPartition_PartitionNumber;
	struct UDF_PhysicalPartition_t *PhysicalPartition;

	uint16_t  PacketLength;
	uint32_t  SizeOfEachSparingTable;
	uint8_t   NumberOfSparingTables;
	uint32_t *SparingTableLocations;

	int Initialized;
	struct UDF_SparingTable_MapEntry_t *SparingTable;
	uint32_t   SparingTableLength;
};

struct UDF_LogicalVolume_Type2_Metadata
{
	struct UDF_LogicalVolume_Common Common;
	uint16_t VolumeSequenceNumber;
	uint16_t PartitionNumber;
	uint32_t MetadataFileLocation;
	uint32_t MetadataMirrorFileLocation;
	uint32_t MetadataBitmapFileLocation;
	uint32_t AllocationUnitSize;
	uint32_t AlignmentUnitSize;
	uint8_t Flags;

	/*	
	struct UDF_LogicalVolume_Type1                  *LogicalVolume_Type1;
	struct UDF_LogicalVolume_Type2_SparingPartition *SparablePartitionMap_Type2;
	*/
	int Initialized;
	struct UDF_LogicalVolume_Common *Master;
	uint8_t  *MetaData;
	uint64_t  MetaSize;
};

struct UDF_FS_DirectoryEntry_t;
struct UDF_FS_FileEntry_t;
struct UDF_FileEntry_t;

enum eFileType
{
	FILETYPE_UNSPECIFIED = 0,
	FILETYPE_UNALLOCATED_SPACE_ENTRY = 1,
	FILETYPE_PARTITION_INTEGRITY_ENTRY = 2,
	FILETYPE_INDIRECT_ENTRY = 3,
	FILETYPE_DIRECTORY = 4,
	FILETYPE_FILE = 5,
	FILETYPE_BLOCK_SPECIAL_DEVICE = 6,
	FILETYPE_CHARACTER_SPECIAL_DEVICE = 7,
	FILETYPE_RECORDING_EXTENDED_ATTRIBUTES = 8,
	FILETYPE_FIFO = 9,
	FILETYPE_C_ISSOCK = 10,
	FILETYPE_TERMINAL_ENTRY = 11,
	FILETYPE_SYMLINK = 12,
	FILETYPE_STREAM_DIRECTORY = 13,
	FILETYPE_THE_VIRTUAL_ALLOCATED_TABLE = 248,
	FILETYPE_REAL_TIME_FILE = 249,
	FILETYPE_METADATA_FILE = 250,
	FILETYPE_METADATA_MIRROR_FILE = 251,
	FILETYPE_METADATA_BITMAP_FILE = 252,
	FILETYPE_UNSET = 256,
};

struct FileAllocation
{
	struct UDF_Partition_Common *Partition; // set to 0 for zerofill holes
	uint32_t ExtentLocation;
	uint32_t SkipLength;        // used for finding back InlineData
	uint32_t InformationLength;
};

struct UDF_FileEntry_t
{
	struct UDF_FileEntry_t      *PreviousVersion; /* WORM feature, known as stragety 4096... UDF_FS_FileEntry_t and UDF_FS_DirectoryEntry_t can unroll this */
	struct UDF_Partition_Common *PartitionCommon; // can be discarded now?
	uint16_t                     TagIdentifier;   // can be discarded now?
	enum eFileType               FileType;
	uint16_t                     Flags;
	uint8_t                      TimeStamp[12];   // handy for 4096...
	uint32_t                     ExtentLocation;  // can be discarded now?

	uint8_t                      HasMajorMinor;
	uint32_t                     Major;
	uint32_t                     Minor;

	uint32_t                     UID;
	uint32_t                     GID;
	uint32_t                     Permissions;

	uint8_t                      atime[12];
	uint8_t                      mtime[12];
	uint8_t                      ctime[12];
	uint8_t                      attrtime[12];

	uint64_t                     InformationLength; // ObjectSize, FileSize
	uint8_t                     *InlineData;
	uint16_t                     InlineDataOffset; // Keeping record of where the information was located
	int                          FileAllocations;
	struct FileAllocation        FileAllocation[];
};

struct UDF_FS_FileEntry_t
{
	struct UDF_FS_FileEntry_t *PreviousVersion;
	struct UDF_FileEntry_t    *FE;

	char                      *FileName;
	char                      *Symlink;
	struct UDF_FS_FileEntry_t *Next; /* next item in the current directory */
};

struct UDF_FS_DirectoryEntry_t
{
	struct UDF_FS_DirectoryEntry_t *PreviousVersion;
	struct UDF_FileEntry_t         *FE;
	//uint16_t                        Location_Partition;
	//uint32_t                        Location_Sector;
	char                           *DirectoryName; /* as seen by the parent */
	struct UDF_FS_DirectoryEntry_t *Next;
	struct UDF_FS_DirectoryEntry_t *DirectoryEntries;
	struct UDF_FS_FileEntry_t      *FileEntries;
};

struct UDF_RootDirectory_t
{
	uint32_t FileSetDescriptor_Partition_Session;
	uint16_t FileSetDescriptor_PartitionNumber; /* can be a trail, does not have to be the same as FileSetDescriptor_PartitionReferenceNumber */
	uint32_t FileSetDescriptor_Location;      /* can be a trail, does not have to be the same as FileSetDescriptor_LogicalBlockNumber */
	uint8_t  FileSetDescriptor_TimeStamp_1_7_3[12];
	uint8_t  FileSetCharacterSet[32];
	struct UDF_longad RootDirectory;
	struct UDF_longad SystemStreamDirectory;

	struct UDF_FS_DirectoryEntry_t *Root;
	struct UDF_FS_DirectoryEntry_t *SystemStream;
};

struct UDF_LogicalVolumes_t
{
	uint32_t  VolumeDescriptorSequenceNumber;
	char     *LogicalVolumeIdentifier;
	uint8_t   DescriptorCharacterSet[64];

	uint32_t FileSetDescriptor_LogicalBlockNumber;
	uint16_t FileSetDescriptor_PartitionReferenceNumber;

	int                         RootDirectories_N;
	struct UDF_RootDirectory_t *RootDirectories;

	int                                  LogicalVolume_N;
	struct UDF_LogicalVolume_Common    **LogicalVolume;
};

struct UDF_Session
{
	struct UDF_PrimaryVolumeDescriptor *PrimaryVolumeDescriptor;

	struct UDF_Partition_Common       CompleteDisk;

	/* When appending, if same PartitionNumber appears, only keep the one with the highest DicsriptorSequenceNumber */
	int                               PhysicalPartition_N;
	struct UDF_PhysicalPartition_t   *PhysicalPartition; /* zero terminated  */

	struct UDF_LogicalVolumes_t      *LogicalVolumes; /* We keep only the one with the highest VolumeDescriptorSequenceNumber */
};


void UDF_Descriptor (struct cdfs_disc_t *disc);

void UDF_Session_Free (struct cdfs_disc_t *disc);

#ifdef CDFS_DEBUG

void DumpFS_UDF (struct cdfs_disc_t *disc);

#endif

void CDFS_Render_UDF (struct cdfs_disc_t *disc, uint32_t parent_directory); /* parent_directory should point to "UDF" */

#endif
