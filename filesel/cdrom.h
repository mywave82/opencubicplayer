#ifndef _CDROM_H
#define _CDROM_H 1

#define IOCTL_CDROM_READTOC "CDROM_READTOC"

struct _ioctl_cdrom_tocentry
{
	uint32_t lba_addr;
	uint8_t is_data;
};

struct ioctl_cdrom_readtoc_request_t
{
	uint8_t starttrack;
	uint8_t lasttrack;
	// uint8_t is_xa; this would be stored in the session / multisession header - only relevant for non-audio */

	/* track at endtrack+1 is LEADOUT... 0xaa in the Linux ioctl */
	struct _ioctl_cdrom_tocentry track[101];
};

#define IOCTL_CDROM_READAUDIO "CDROM_READAUDIO"

struct ioctl_cdrom_readaudio_request_t
{
	uint32_t  lba_addr;
	uint32_t  lba_count;
	uint8_t  *ptr;
};

#endif
