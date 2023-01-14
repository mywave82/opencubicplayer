/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsers for the different ZIP internal headers
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

static int32_t local_file_header (uint8_t *src, uint64_t len,
                                  uint16_t *GeneralPurposeFlags,
                                  uint16_t *CompressionMethod)
{
	uint16_t filename_length;
	uint16_t extra_field_length;

	if (len < 30)
	{
		return -1;
	}
	if ((src[3] != 0x04) ||
	    (src[2] != 0x03) ||
	    (src[1] != 0x4b) ||
	    (src[0] != 0x50))
	{
		return -1;
	}

	*GeneralPurposeFlags = (src[7] << 8) | src[6];

	*CompressionMethod = (src[9] << 8) | src[8];

	//last_mod_file_time = (src[11] << 8) | src[10];

	//last_mod_file_date = (src[13] << 8) | src[12];

	//crc = (src[17] << 24) | (src[16] << 16) | (src[15] << 8) | src[14];

	//compressed_size = (uint32_t)((src[21] << 24) | (src[20] << 16) | (src[19] << 8) | src[18]);

	//uncompressed_size = (uint32_t)((src[25] << 24) | (src[24] << 16) | (src[23] << 8) | src[22]);

	filename_length = (src[27] << 8) | src[26];

	extra_field_length = (src[29] << 8) | src[28];
	if (len < (30 + filename_length + extra_field_length))
	{
		return -1;
	}

	//extra_data (src + 30 + filename_length, extra_field_length, &uncompressed_size, &uncompressed_missing, compressed_size, &compressed_missing);

	return 30 + filename_length + extra_field_length;
}

static int64_t end_of_central_directory_record (uint8_t *src, uint64_t len,
                                                uint16_t *Number_of_this_disk,                                                           /* [ZIP64 end of central directory record] 32bit    [End of central directory record] 16bit  -  should match total number of disks */
                                                uint32_t *Number_of_the_disk_with_the_start_of_the_end_of_central_directory,             /* [ZIP64 end of central directory record] 32bit    [End of central directory record] 16bit */
                                                uint64_t *Total_number_of_entries_in_the_central_directory_on_this_disk,                 /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 16bit */
                                                uint64_t *Total_number_of_entries_in_the_central_directory,                              /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 16bit */
                                                uint64_t *Size_of_the_central_directory,                                                 /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 32bit */
                                                uint64_t *Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number) /* [ZIP64 end of central directory record] 64bit    [End of central directory record] 32bit */
{
	uint16_t ZIP_file_comment_length;
	int retval = 0;

	if (len < 22)
	{
		DEBUG_PRINT ("len=%"PRIu64" is too small to end of central directory recprd\n", len);
		return -1;
	}

	if ((src[3] != 0x06) ||
	    (src[2] != 0x05) ||
	    (src[1] != 0x4b) ||
	    (src[0] != 0x50))
	{
		DEBUG_PRINT ("invalid header signature for a end of central directory record\n");
		return -1;
	}

	*Number_of_this_disk = (src[5] << 8) | src[4];
	if (*Number_of_this_disk == UINT16_MAX)
	{
		retval = -2; // size is too big, see Zip64
	}

	*Number_of_the_disk_with_the_start_of_the_end_of_central_directory = (uint16_t)((src[7] << 8) | src[6]);
	if (*Number_of_the_disk_with_the_start_of_the_end_of_central_directory == UINT16_MAX)
	{
		retval = -2; // size is too big, see Zip64
	}

	*Total_number_of_entries_in_the_central_directory_on_this_disk = (uint16_t)((src[9] << 8) | src[8]);
	if (*Total_number_of_entries_in_the_central_directory_on_this_disk == UINT16_MAX)
	{
		retval = -2; // size is too big, see Zip64
	}

	*Total_number_of_entries_in_the_central_directory = (uint16_t)((src[11] << 8) | src[10]);
	if (*Total_number_of_entries_in_the_central_directory == UINT16_MAX)
	{
		retval = -2; // size is too big, see Zip64
	}

	*Size_of_the_central_directory = (uint32_t)((src[15] << 24) | (src[14] << 16) | (src[13] << 8) | src[12]);
	if (*Size_of_the_central_directory == UINT32_MAX)
	{
		retval = -2; // size is too big, see Zip64
	}

	*Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number = (uint32_t)((src[19] << 24) | (src[18] << 16) | (src[17] << 8) | src[16]);
	if (*Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number == UINT32_MAX)
	{
		retval = -2; // size is too big, see Zip64
	}

	ZIP_file_comment_length = (src[21] << 8) | src[20];

	if ((ZIP_file_comment_length + 22) > len)
	{
		//printf ("No space for the aux data\n");
	}

	if (ZIP_file_comment_length)
	{
		//printf (" Comment: \"");
		//fwrite (src + 22, ZIP_file_comment_length, 1, stdout);
		//printf ("\"\n");
	}

	if (retval)
	{
		return retval;
	}
	return 22 + ZIP_file_comment_length;
}

static int64_t zip64_end_of_central_directory_record (uint8_t *src, uint64_t len,
                                                      uint32_t *Number_of_the_disk_with_the_start_of_the_end_of_central_directory,
                                                      uint64_t *Total_number_of_entries_in_the_central_directory_on_this_disk,
                                                      uint64_t *Total_number_of_entries_in_the_central_directory,
                                                      uint64_t *Size_of_the_central_directory,
                                                      uint64_t *Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number)
{
	uint64_t size_of_zip64_end_of_central_directory_record;
	//uint64_t real_data_size;
	//uint32_t number_of_this_disk;

	if (len < (4 * 8 + 2 * 4 + 2 * 2 + 12))
	{
		DEBUG_PRINT ("len=%"PRIu64" is too small to fit zip64 end of central directory record\n", len);
		return -1;
	}

	if ((src[3] != 0x06) ||
	    (src[2] != 0x06) ||
	    (src[1] != 0x4b) ||
	    (src[0] != 0x50))
	{
		DEBUG_PRINT ("invalid header signature for a zip64 end of central directory record\n");
		return -1;
	}

	size_of_zip64_end_of_central_directory_record = ((uint64_t)src[11] << 56) | ((uint64_t)src[10] << 48) | ((uint64_t)src[9] << 40) | ((uint64_t)src[8] << 32) | (src[7] << 24) | (src[6] << 16) | (src[5] << 8) | src[4];
	//real_data_size = size_of_zip64_end_of_central_directory_record - (4 * 8 + 2 * 4 + 2 * 2);

	//number_of_this_disk = (src[19] << 24) | (src[18] << 16) | (src[17] << 8) | src[16];

	*Number_of_the_disk_with_the_start_of_the_end_of_central_directory = (src[23] << 24) | (src[22] << 16) | (src[21] << 8) | src[20];

	*Total_number_of_entries_in_the_central_directory_on_this_disk = ((uint64_t)src[31] << 56) | ((uint64_t)src[30] << 48) | ((uint64_t)src[29] << 40) | ((uint64_t)src[28] << 32) | (src[27] << 24) | (src[26] << 16) | (src[25] << 8) | src[24];

	*Total_number_of_entries_in_the_central_directory = ((uint64_t)src[39] << 56) | ((uint64_t)src[38] << 48) | ((uint64_t)src[37] << 40) | ((uint64_t)src[36] << 32) | (src[35] << 24) | (src[34] << 16) | (src[33] << 8) | src[32];

	*Size_of_the_central_directory = ((uint64_t)src[47] << 56) | ((uint64_t)src[46] << 48) | ((uint64_t)src[45] << 40) | ((uint64_t)src[44] << 32) | (src[43] << 24) | (src[42] << 16) | (src[41] << 8) | src[40];

	*Offset_of_start_of_central_directory_with_respect_to_the_starting_disk_number = ((uint64_t)src[55] << 56) | ((uint64_t)src[54] << 48) | ((uint64_t)src[53] << 40) | ((uint64_t)src[52] << 32) | (src[51] << 24) | (src[50] << 16) | (src[49] << 8) | src[48];
	// we could dump the data here... it has a known initial structure...

	if ((size_of_zip64_end_of_central_directory_record + 12) > len)
	{
		DEBUG_PRINT ("No space for the aux data\n");
		return -1;
	}

	return size_of_zip64_end_of_central_directory_record + 12;
}

static int64_t zip64_end_of_central_directory_locator (uint8_t *src, uint64_t len,
                                                       uint32_t *Number_of_the_disk_with_the_start_of_the_zip64_end_of_central_directory,
                                                       uint64_t *Relative_offset_of_the_zip64_end_of_central_directory_record,
                                                       uint32_t *Total_number_of_disks)
{
	if (len < 20)
	{
		DEBUG_PRINT ("len=%"PRIu64" is too small to fit zip64 end of central directory locator\n", len);
		return -1;
	}

	if ((src[3] != 0x07) ||
	    (src[2] != 0x06) ||
	    (src[1] != 0x4b) ||
	    (src[0] != 0x50))
	{
		DEBUG_PRINT ("invalid header signature for a zip64 end of central directory locator\n");
		return -1;
	}

	*Number_of_the_disk_with_the_start_of_the_zip64_end_of_central_directory = (src[7] << 24) | (src[6] << 16) | (src[5] << 8) | src[4];

	*Relative_offset_of_the_zip64_end_of_central_directory_record = (((uint64_t)src[15]) << 56) | (((uint64_t)src[14]) << 48) | (((uint64_t)src[13]) << 40) | (((uint64_t)src[12]) << 32) | (src[11] << 24) | (src[10] << 16) | (src[9] << 8) | src[8];

	*Total_number_of_disks = (src[19] << 24) | (src[18] << 16) | (src[17] << 8) | src[16];

	return 20;
}

static void extra_data (uint8_t *src, uint64_t len,
                        uint64_t  *CompressedSize,
                        uint64_t  *UncompressedSize,
                        uint32_t  *DiskNumber,
                        uint64_t  *OffsetLocalHeader)
{
	while (len)
	{
		uint16_t e_len;
		uint16_t e_id;

		if (len < 4)
		{
			return;
		}

		e_id = (src[1] << 8) | src[0];
		e_len = (src[3] << 8) | src[2];

		len -= 4;
		src += 4;

		if (e_id == 0x0001)
		{
			if (e_len >= 8)
			{
				*UncompressedSize = (((uint64_t)src[7]) << 56) | (((uint64_t)src[6]) << 48) | (((uint64_t)src[5]) << 40) | (((uint64_t)src[4]) << 32) | (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | (src[0]);
			}

			if (e_len >= 16)
			{
				*CompressedSize = (((uint64_t)src[15]) << 56) | (((uint64_t)src[14]) << 48) | (((uint64_t)src[13]) << 40) | (((uint64_t)src[12]) << 32) | (src[11] << 24) | (src[10] << 16) | (src[9] << 8) | (src[8]);
			}

			if (e_len >= 24)
			{
				*OffsetLocalHeader = (((uint64_t)src[23]) << 56) | (((uint64_t)src[22]) << 48) | (((uint64_t)src[21]) << 40) | (((uint64_t)src[20]) << 32) | (src[19] << 24) | (src[18] << 16) | (src[17] << 8) | (src[16]);
			}

			if (e_len >= 28)
			{
				*DiskNumber = (src[27] << 24) | (src[26] << 16) | (src[25] << 8) | (src[24]);
			}
		}
		src += e_len;
		len -= e_len;
	}
}

static int32_t central_directory_header (uint8_t   *src, uint64_t len,
                                         uint64_t  *CompressedSize,
                                         uint64_t  *UncompressedSize,
                                         uint32_t  *DiskNumber,
                                         uint64_t  *OffsetLocalHeader,
                                         uint32_t  *CRC,
                                         char     **Filename,
                                         int       *Filename_FlaggedUTF8)
{
	uint16_t general_purpose_flags;
	//uint16_t compression_method;
	//uint16_t last_mod_file_time;
	//uint16_t last_mod_file_date;
	uint16_t filename_length;
	uint16_t extra_field_length;
	uint16_t file_comment_length;
	//uint16_t internal_file_attributes;
	//uint32_t external_file_attributes;

	*Filename = 0;

	if (len < 26)
	{
		DEBUG_PRINT ("len=%"PRIu64" is too small to fit a central directory header\n", len);
		return -1;
	}
	if ((src[3] != 0x02) ||
	    (src[2] != 0x01) ||
	    (src[1] != 0x4b) ||
	    (src[0] != 0x50))
	{
		DEBUG_PRINT ("invalid header signature for a central directory header\n");
		return -1;
	}

	//printf (" Version made by:           (0x%02x%02x) %s - %d.%d\n", src[5], src[4], madeby (src[5]), src[4] / 10, src[4] % 10); -- we might need this to detect chinese encoding
	//printf (" Version needed to extract: (0x%02x%02x) %d.%d\n", src[7], src[6], src[6] / 10, src[6] % 10);

	general_purpose_flags = (src[9] << 8) | src[8];
	*Filename_FlaggedUTF8 = !!(general_purpose_flags & 0x0800);

	//compression_method = (src[11] << 8) | src[10];
	//print_method (compression_method, general_purpose_flags);

	//last_mod_file_time = (src[13] << 8) | src[12];

	//last_mod_file_date = (src[15] << 8) | src[14];

	*CRC = (src[19] << 24) | (src[18] << 16) | (src[17] << 8) | src[16];

	*CompressedSize = (uint32_t)((src[23] << 24) | (src[22] << 16) | (src[21] << 8) | src[20]);
	if (*CompressedSize == UINT32_MAX)
	{
		//compressed_missing = 1;
		//printf (" (size is too big, see Zip64)");
	}

	*UncompressedSize = (uint32_t)((src[27] << 24) | (src[26] << 16) | (src[25] << 8) | src[24]);
	if (*UncompressedSize == UINT32_MAX)
	{
		//uncompressed_missing = 1;
		//printf (" (size is too big, see Zip64)");
	}

	filename_length = (src[29] << 8) | src[28];

	extra_field_length = (src[31] << 8) | src[30];

	file_comment_length = (src[33] << 8) | src[32];
#if 0
	printf (" File comment length:       0x%"PRIx16, file_comment_length);
	if (len >= (46 + filename_length + extra_field_length + file_comment_length))
	{
		int i;
		for (i=0; i < file_comment_length; i++)
		{
			printf (" 0x%02x", src[46 + filename_length + i]);
		}
	}
	printf ("\n");
#endif

	*DiskNumber = (src[35] << 8) | src[34];

	//internal_file_attributes = (src[37] << 8) | src[36];

	//external_file_attributes = (src[41] << 24) | (src[40] << 16) | (src[39] << 8) | src[38];

	*OffsetLocalHeader = (src[45] << 24) | (src[44] << 16) | (src[43] << 8) | src[42];
	if (*OffsetLocalHeader == UINT32_MAX)
	{
		//printf (" (size is too big, see Zip64)");
	}

	if (len < (46 + filename_length + extra_field_length + file_comment_length))
	{
		return -1;
	}

	*Filename = malloc (filename_length + 1);
	memcpy (*Filename, src + 46, filename_length);
	Filename[0][filename_length] = 0;

	extra_data (src + 46 + filename_length, extra_field_length, CompressedSize, UncompressedSize, DiskNumber, OffsetLocalHeader);

	/* comment we ignore:
	parse_comment (src + 46 + filename_length + extra_field_length, file_comment_length);;
	*/

	return 46 + filename_length + extra_field_length + file_comment_length;
}
