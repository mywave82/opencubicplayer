/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Archive handler for ZIP archives
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -added _dllinfo record
 *  -fd981206   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 */

#include "config.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>
#include "types.h"
#include "adb.h"
#include "mdb.h"
#include "mif.h"
#include "modlist.h"
#include "pfilesel.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "stuff/compat.h"
#include "stuff/pagesize.inc.c"


static char mdbScanBuf[1084];
static uint8_t adbScanBuf[2048];

struct __attribute__((packed)) local_file_header
{
	uint32_t sig;
	uint16_t ver;
	uint16_t opt;
	uint16_t method;
	uint32_t d1;
	uint32_t d2;
	uint32_t csize;
	uint32_t osize;
	uint16_t flen;
	uint16_t xlen;
};

static int method_supported(int method)
{
	return (method==0)||(method==8)||(method==9);
}

static int decode_partial(char *dest, size_t destlen, uint8_t *src, size_t srclen, int method)
{
	switch (method)
	{
		case 0:
			{
				if (destlen > srclen )
					destlen = srclen;
				memcpy(dest, src, destlen);
				break;
			}
		case 8:
		case 9:
			{
				z_stream strm;
				int res;

				memset(&strm, 0, sizeof(strm));
				strm.next_in=src;
				strm.avail_in=(uLong)srclen;
				strm.next_out=(Bytef *)dest;
				strm.avail_out=(uLongf)destlen;

				if ((res=inflateInit2(&strm, -15)))
				{
					fprintf(stderr, "arcZIP: InflateInit2 failed (%d)\n", res);
					destlen=0;
				} else {
					res=inflate(&strm, 1);
					destlen=strm.total_out;
					inflateEnd(&strm);
				}
				break;
			}
	}
	return destlen;
}

static int decode_to_fd(char *src, ssize_t srclen, int dest, int method)
{
	switch (method)
	{
		case 0:
			{
				return (write(dest, src, srclen)==srclen);
				break;
			}
		case 8:
		case 9:
			{
				Bytef buffer[65536];

				z_stream strm;
				int res;

				memset(&strm, 0, sizeof(strm));
				strm.next_in=(Bytef *)src;
				strm.avail_in=srclen;
				strm.next_out=buffer;
				strm.avail_out=sizeof(buffer);
#ifdef ZIP_DEBUG
				fprintf(stderr, "pre\n");
				fprintf(stderr, "strm.next_in=%p\n", strm.next_in);
				fprintf(stderr, "strm.avail_in=%d\n", strm.avail_in);
#endif
				if ((res=inflateInit2(&strm, -15)))
				{
					fprintf(stderr, "arcZIP: InflateInit2 failed (%d)\n", res);
					return 0;
				}
#ifdef ZIP_DEBUG
				fprintf(stderr, "after init\n");
				fprintf(stderr, "strm.next_in=%p\n", strm.next_in);
				fprintf(stderr, "strm.avail_in=%d\n", strm.avail_in);
#endif
				while (1)
				{
					int len;
#ifdef ZIP_DEBUG
					fprintf(stderr, "before inflate\n");
					fprintf(stderr, "strm.next_in=%p\n", strm.next_in);
					fprintf(stderr, "strm.avail_in=%d\n", strm.avail_in);
#endif
					switch (inflate(&strm, 1))
					{
						case Z_OK:
							len=sizeof(buffer)-strm.avail_out;
						#ifdef ZIP_DEBUG
							fprintf(stderr, "Z_OK, len=%d\n", len);
						#endif
							if (write(dest, buffer, len)!=len)
							{
								perror("arcZIP: write()");
								return 0;
							}
							strm.next_out=buffer;
							strm.avail_out=sizeof(buffer);
							break;
						case Z_STREAM_END:
							len=sizeof(buffer)-strm.avail_out;
						#ifdef ZIP_DEBUG
							fprintf(stderr, "Z_STREAM_END: len=%d\n", len);
						#endif
							if (write(dest, buffer, len)!=len)
							{
								perror("arcZIP: write()");
								return 0;
							}
							strm.next_out=buffer;
							strm.avail_out=sizeof(buffer);
							inflateEnd(&strm);
							return 1;
							break;
						default:
							if (strm.msg)
								fprintf(stderr, "arcZIP: inflate(): %s\n", strm.msg);
							else
								fprintf(stderr, "arcZIP: inflate(): unknown error\n");
							inflateEnd(&strm);
							return 0;
					}
				}
			}
			break;
		default:
			fprintf(stderr, "arcZIP: Invalid method (%d)\n", method);
	}
	return 0;
}

static int openZIP(const char *path)
{
	struct stat st;
	int extfd=-1;

	if ((extfd=open(path, O_RDONLY))<0)
	{
		perror("arcZIP: open(path, O_RDONLY)");
		return -1;
	}

	if ((fstat(extfd, &st))<0)
	{
		perror("arcZIP: fstat(extfd, &st)");
		close(extfd);
		return -1;
	}

	if (!(S_ISREG(st.st_mode)))
	{
		fprintf(stderr, "arcZIP: Not a regular file\n");
		close(extfd);
		return -1;
	}

	return extfd;
}

static int adbZIPScan(const char *path)
{
	char arcname[ARC_PATH_MAX+1];
	int extfd=-1;
	uint32_t arcref;
	struct arcentry a;

	{
		char *ext;
		char *name;
		splitpath4_malloc (path, 0, 0, &name, &ext);
		if ((strlen(name)+strlen(ext)+1)>ARC_PATH_MAX)
		{
			free (name);
			free (ext);
			return 0;
		}
		snprintf (arcname, sizeof (arcname), "%s%s", name, ext);
		free (name);
		free (ext);
	}

	if ((extfd=openZIP(path))<0)
		return 0;

	memset(a.name, 0, sizeof(a.name));
	strncpy(a.name, arcname, sizeof(a.name)-1);
	lseek(extfd, 0, SEEK_END);
	a.size=lseek(extfd, 0, SEEK_CUR);
	lseek(extfd, 0, SEEK_SET);
	a.flags=ADB_ARC;

	if (!adbAdd(&a))
	{
		close(extfd);
		return 0;
	}
	arcref=adbFind(arcname);
	while (1)
	{
		off_t nextpos;
		struct local_file_header hdr;
		if (read(extfd, &hdr, sizeof(hdr))!=sizeof(hdr))
			break;
		hdr.sig    = uint32_little (hdr.sig);
		hdr.ver    = uint16_little (hdr.ver);
		hdr.opt    = uint16_little (hdr.opt);
		hdr.method = uint16_little (hdr.method);
		hdr.d1     = uint32_little (hdr.d1);
		hdr.d2     = uint32_little (hdr.d2);
		hdr.csize  = uint32_little (hdr.csize);
		hdr.osize  = uint32_little (hdr.osize);
		hdr.flen   = uint16_little (hdr.flen);
		hdr.xlen   = uint16_little (hdr.xlen);
		if (hdr.sig!=0x04034B50)
			break;
		nextpos=lseek(extfd, 0, SEEK_CUR)+hdr.flen+hdr.xlen+hdr.csize;
		if (((hdr.flen+1)<ARC_PATH_MAX)&&((hdr.flen+1)<NAME_MAX)&&!(hdr.opt&0x1))
		{
			char *name, *ext;

			memset(a.name, 0, sizeof(arcname));
			if (read(extfd, a.name, hdr.flen)!=hdr.flen)
			{
				close(extfd);
				return 0;
			}
			splitpath4_malloc (a.name, NULL, NULL, &name, &ext);
		#ifdef ZIP_DEBUG
			fprintf(stderr, "arcZIP: About to do %s as %s\n", arcname, a.name);
		#endif
			lseek(extfd, hdr.xlen, SEEK_CUR);
			if (fsIsModule(ext))
			{
				a.size=hdr.osize;
				a.parent=arcref;
				a.flags=0;
				if (!adbAdd(&a))
				{
					free (name);
					free (ext);
					close(extfd);
					return 0;
				}

				snprintf (a.name, sizeof (a.name), "%s%s", name, ext);

				if (fsScanInArc&&method_supported(hdr.method))
				{
					char shortname[12];
					uint32_t mdb_ref;
					fs12name(shortname, a.name);
					mdb_ref=mdbGetModuleReference(shortname, a.size);
					if (mdb_ref==0xffffffff)
					{
						free (name);
						free (ext);
						close(extfd);
						return 0;
					}
					if (!mdbInfoRead(mdb_ref))
					{
						struct moduleinfostruct mi;
						int destlen = sizeof(mdbScanBuf);
						int srclen;

						memset(adbScanBuf, 0, sizeof(adbScanBuf));
						srclen=(hdr.csize>sizeof(adbScanBuf))?sizeof(adbScanBuf):hdr.csize;

						if (read(extfd, adbScanBuf, srclen)!=srclen)
						{
							free (name);
							free (ext);
							close(extfd);
							return 0;
						}

						if ((destlen = decode_partial(mdbScanBuf, destlen, adbScanBuf, srclen, hdr.method)))
							if (mdbGetModuleInfo(&mi, mdb_ref))
							{
								mdbReadMemInfo(&mi, mdbScanBuf, destlen); /* we do not care about the return-value. We do not want to decompress the file further */
								mdbWriteModuleInfo(mdb_ref, &mi);
							}
					}
				}
			}
			/* if ((!stricmp(ext, MIF_EXT)) && (hdr.osize<65536))
			{
				char *obuffer=new char[hdr.osize],
				     *cbuffer=new char[hdr.csize];
				file.read(cbuffer, hdr.csize);
				if (hdr.method==8)
					inflatemax(obuffer, cbuffer, hdr.osize);
				else
					memcpy(obuffer, cbuffer, hdr.osize);
				mifMemRead(a.name, hdr.osize, obuffer);
				delete[] obuffer;
				delete[] cbuffer;
			}*/
			free (name);
			free (ext);
		}
		lseek(extfd, nextpos, SEEK_SET);
	}
	close(extfd);
	return 1;
}


static int adbZIPCall(const int act, const char *apath, const char *fullname, const int fd)
{
	switch (act)
	{
		case adbCallGet:
		{
			int extfd;
#ifdef ZIP_DEBUG
			fprintf(stderr, "STIAN act=adbCallGet, apath=%s, fullname=%s, fd=%d\n", apath, fullname, fd);
#endif
			if ((extfd=openZIP(apath))<0)
				return 0;

			/* TODO, read EOC */

			while (1)
			{
				struct local_file_header hdr;
				char name[ARC_PATH_MAX+1];
				if (read(extfd, &hdr, sizeof(hdr))!=sizeof(hdr))
					break;
				hdr.sig    = uint32_little (hdr.sig);
				hdr.ver    = uint16_little (hdr.ver);
				hdr.opt    = uint16_little (hdr.opt);
				hdr.method = uint16_little (hdr.method);
				hdr.d1     = uint32_little (hdr.d1);
				hdr.d2     = uint32_little (hdr.d2);
				hdr.csize  = uint32_little (hdr.csize);
				hdr.osize  = uint32_little (hdr.osize);
				hdr.flen   = uint16_little (hdr.flen);
				hdr.xlen   = uint16_little (hdr.xlen);
				if (hdr.sig!=0x04034B50)
					break;
				if (method_supported(hdr.method))
				{
					if ((hdr.flen<=ARC_PATH_MAX)&&!(hdr.opt&0x1))
					{
						memset(name, 0, sizeof(name));
						if (read(extfd, name, hdr.flen)!=hdr.flen)
						{
							fprintf(stderr, "arcZIP: Premature EOF\n");
							close(extfd);
							return 0;
						}
						name[hdr.flen]=0;
						lseek(extfd, hdr.xlen, SEEK_CUR);
						if (!strcmp(fullname, name))
						{
							char *data;

							off_t start  = lseek(extfd, 0, SEEK_CUR);
							off_t length = hdr.csize;

							off_t mmap_start=start&~((off_t)(pagesize()-1));
							off_t mmap_start_pad=start-mmap_start;
							off_t mmap_length=(mmap_start_pad+length+pagesize()-1)&~((off_t)(pagesize()-1));

							if ((data=mmap(0, mmap_length, PROT_READ, MAP_SHARED, extfd, mmap_start))==MAP_FAILED)
							{
								perror("arcZIP mmap()");
								close(extfd);
								return 0;
							}
							close(extfd);

							if (!decode_to_fd(data+mmap_start_pad, length, fd, hdr.method))
							{
								munmap(data, mmap_length);
								fprintf(stderr, "arcZIP: Failed to decompress\n");
								return 0;
							}
							munmap(data, mmap_length);
							return 1; /* all OK */
						} else
							lseek(extfd, hdr.csize, SEEK_CUR);
					} else
						lseek(extfd, hdr.csize+hdr.flen+hdr.xlen, SEEK_CUR);
				} else
					lseek(extfd, hdr.csize+hdr.flen+hdr.xlen, SEEK_CUR);
			}
			fprintf(stderr, "arcZIP: File not found in arc\n");
			break;
			/*
			const char *temp;
			fprintf(stderr, "adbZIPCall::adbCallGet(apath=%s, fullname=%s file=%s dpath=%s\n", apath, fullname, file, dpath);
			temp = cfGetProfileString("arcZIP", "get", "pkunzip %a %d %n");
			return !adbCallArc(temp, apath, file, dpath);*/
		}
#if 0
		case adbCallPut:
			return !adbCallArc(cfGetProfileString("arcZIP", "moveto", "pkzip %a %n"), apath, file, dpath);
		case adbCallDelete:
			if (adbCallArc(cfGetProfileString("arcZIP", "delete", "pkzip -d %a %n"), apath, file, dpath))
				return 0;
			if (cfGetProfileBool("arcZIP", "deleteempty", 0, 0))
			{
				struct stat st;
				if (stat(apath, &st))
					return 1;
				if (st.st_size==22)
					unlink(apath);
			}
			return 1;
		case adbCallMoveFrom:
			if (cfGetProfileString("arcZIP", "movefrom", 0))
				return !adbCallArc(cfGetProfileString("arcZIP", "movefrom", ""), apath, file, dpath);
			if (!adbZIPCall(adbCallGet, apath, fullname, file, dpath))
				return 0;
			return adbZIPCall(adbCallDelete, apath, fullname, file, dpath);
		case adbCallMoveTo:
			if (cfGetProfileString("arcZIP", "moveto", 0))
				return !adbCallArc(cfGetProfileString("arcZIP", "moveto", "pkzip -m %a %n"), apath, file, dpath);
			if (!adbZIPCall(adbCallPut, apath, fullname, file, dpath))
				return 0;
			unlink(file);
			return 1;
#endif
	}
	return 0;
}


static struct adbregstruct adbZIPReg = {".ZIP", adbZIPScan, adbZIPCall ADBREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	adbRegister(&adbZIPReg);
}

static void __attribute__((destructor))done(void)
{
	adbUnregister(&adbZIPReg);
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "arczip", .desc = "OpenCP Archive Reader: .ZIP (c) 1994-09 Niklas Beisert", .ver = DLLVERSION, .size = 0};
