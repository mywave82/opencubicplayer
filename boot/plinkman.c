/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Link manager - contains high-level routines to load and handle
 *                external DLLs
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -added lnkReadInfoReg() to read out _dllinfo entries
 *  -fd981014   Felix Domke    <tmbinc@gmx.net>
 *    -increased the dllinfo-buffersize from 256 to 1024 chars in parseinfo
 *  -fd981206   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 *  -ryg981206  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -added DLL autoloader (DOS only)
 *  -ss040613   Stian Skjelstad  <stian@nixia.no>
 *    -rewritten for unix
 *  -ss040831   Stian Skjelstad  <stian@nixia.no>
 *    -made lnkDoLoad more strict, and work correct when LD_DEBUG is not defined
 *  -doj040907  Dirk Jagdmann  <doj@cubic.org>
 *    -better error message of dllextinfo is not found
 */

#include "config.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "types.h"

#include "psetting.h"
#include "plinkman.h"

#include "stuff/compat.h"

static int handlecounter;
static struct dll_handle loadlist[MAXDLLLIST];
int loadlist_n;

#ifdef SUPPORT_STATIC_PLUGINS
const DLLEXTINFO_PREFIX struct linkinfostruct staticdlls = {.name = "static", .desc = "Compiled in plugins (c) 2009-'22 Stian Skjelstad", .ver = DLLVERSION};
#endif

static char reglist[1024];
static void parseinfo (char *pi, const char *key)
{
	char s[1024];
	char *dip;
	char keyok;
	char kstate;

	strcpy (s,pi);

	s[strlen(s)+1]=0;

	dip=s;
	keyok=0;
	kstate=0;
	while (*dip)
	{
		char *d2p=dip;
		while (*dip)
		{
			char t;

			d2p++;
			t=*d2p;
			if (t==';' || t==' ' || !t)
			{
				*d2p=0;
				if (!kstate)
				{
					keyok = !strcmp(dip,key);
					kstate= 1;
				} else if (keyok)
				{
					strcat(reglist,dip);
					strcat(reglist," ");
				}

				if (t==';')
					kstate=keyok=0;

				do
					dip=++d2p;
				while (*dip && (*dip==' ' || *dip==';'));
			}
		}
	}
}

char *lnkReadInfoReg(const int id, const char *key)
{
	char **pi=0;
	int i;

	*reglist=0;

	for (i=loadlist_n-1;i>=0;i--)
		if (loadlist[i].id==id)
			if ((pi=dlsym(loadlist[i].handle, "dllinfo")))
				parseinfo(*pi, key);
	if (*reglist)
		reglist[strlen(reglist)-1]=0;

	return reglist;
}

static int _lnkDoLoad(char *file)
{
	int i;

	for (i=0; i < loadlist_n; i++)
	{
		if (!strcmp (loadlist[i].file, file))
		{
			loadlist[i].refcount++;
			free (file);
			return loadlist[i].id;
		}
	}

	if (loadlist_n>=MAXDLLLIST)
	{
		fprintf(stderr, "Too many open shared objects\n");
		free (file);
		return -1;
	}

	if (!(loadlist[loadlist_n].handle=dlopen(file, RTLD_NOW|RTLD_GLOBAL)))
	{
		fprintf(stderr, "%s\n", dlerror());
		free (file);
		return -1;
	}

	loadlist[loadlist_n].id=++handlecounter;
	loadlist[loadlist_n].refcount=1;
	loadlist[loadlist_n].file=file;

	if (!(loadlist[loadlist_n].info=(struct linkinfostruct *)dlsym(loadlist[loadlist_n].handle, "dllextinfo")))
	{
		fprintf(stderr, "lnkDoLoad(%s): dlsym(dllextinfo): %s\n", file, dlerror());
		free (file);
		dlclose (loadlist[loadlist_n].handle);
		loadlist[loadlist_n].handle=0;
		loadlist[loadlist_n].file=0;
		return -1;
	}

	{
		struct stat st;
		if (stat(file, &st))
			st.st_size=0;
		loadlist[loadlist_n].size=st.st_size;
	}

	loadlist_n++;

	return handlecounter;
}

static int lnkDoLoad(const char *file)
{
	int retval;
	char *buffer;

#ifdef LD_DEBUG
	fprintf(stderr, "Request to load %s\n", file);
#endif
	if (!strncmp (file, "autoload/", 9))
	{
		makepath_malloc (&buffer, 0, cfProgramDirAutoload, file + 9, LIB_SUFFIX);
	} else {
		makepath_malloc (&buffer, 0, cfProgramDir, file, LIB_SUFFIX);
	}
#ifdef LD_DEBUG
	fprintf(stderr, "Attempting to load %s\n", buffer);
#endif
	retval = _lnkDoLoad(buffer); // steals the string
	return retval;
}

#ifdef HAVE_QSORT
static int cmpstringp(const void *p1, const void *p2)
{
	return strcmp(*(char * const *)p1, *(char * const *)p2);
}
#else
static void bsort(char **files, int n)
{
	/* old classic bouble sort */
	int m;
	int c;

	if (n<=1)
		return;
	c=1;
	n--;
	while (c)
	{
		c=0;
		for (m=0;m<n;m++)
		{
			if (strcmp(files[m], files[m+1])>0)
			{
				char *t = files[m];
				files[m]=files[m+1];
				files[m+1]=t;
				c=1;
			}
		}
	}
}
#endif

int lnkLinkDir(const char *dir)
{
	DIR *d;
	struct dirent *de;
	char *filenames[1024];
	char *buffer;
	int files=0;
	int n;
	if (!(d=opendir(dir)))
	{
		perror("opendir()");
		return -1;
	}
	while ((de=readdir(d)))
	{
		size_t len=strlen(de->d_name);
		if (len<strlen(LIB_SUFFIX))
			continue;
		if (strcmp(de->d_name+len-strlen(LIB_SUFFIX), LIB_SUFFIX))
			continue;
		if (files>=1024)
		{
			fprintf(stderr, "lnkLinkDir: Too many libraries in directory %s\n", dir);
			closedir(d);
			return -1;
		}
		filenames[files++]=strdup(de->d_name);
	}
	closedir(d);
	if (!files)
		return 0;
#ifdef HAVE_QSORT
	qsort(filenames, files, sizeof(char *), cmpstringp);
#else
	bsort(filenames, files);
#endif
	for (n=0;n<files;n++)
	{
		makepath_malloc (&buffer, 0, dir, filenames[n], 0);
		if (_lnkDoLoad(buffer)<0) // steals the string
		{
			for (;n<files;n++)
				free(filenames[n]);
			return -1;
		}
		free (filenames[n]);
	}
	return 0; /* all okey */
}

int lnkLink(const char *files)
{
	int retval=0;
	char *tmp=strdup(files);
	char *tmp2=tmp;
	char *tmp3;
	while ((tmp3=strtok(tmp2, " ")))
	{
		tmp2=NULL;
		tmp3[strlen(tmp3)]=0;
		if (strlen(tmp3))
		{
			if ((retval=lnkDoLoad(tmp3))<0)
				break;
		}
	}
	free(tmp);
	return retval;
}

void lnkFree(const int id)
{
	int i;

	if (!id)
	{
		for (i=loadlist_n-1;i>=0;i--)
		{
#ifndef NO_DLCLOSE
			if (loadlist[i].handle) /* this happens for static plugins */
				dlclose(loadlist[i].handle);
#endif
			free (loadlist[i].file);
		}
		loadlist_n=0;
	} else {
		for (i=loadlist_n-1;i>=0;i--)
			if (loadlist[i].id==id)
			{
				loadlist[i].refcount--;
				if (loadlist[i].refcount)
				{
					return;
				}
#ifndef NO_DLCLOSE
				if (loadlist[i].handle) /* this happens for static plugins */
					dlclose(loadlist[i].handle);
#endif
				free (loadlist[i].file);
				memmove(&loadlist[i], &loadlist[i+1], (MAXDLLLIST-i-1)*sizeof(loadlist[0]));
				loadlist_n--;
				return;
			}
	}
}

#ifdef SUPPORT_STATIC_PLUGINS
static void lnkLoadStatics(void)
{
	struct linkinfostruct *iterator = &staticdlls;
	while (iterator->name)
	{
		#ifdef LD_DEBUG
		fprintf(stderr, "[lnk] Adding static module: \"%s\"\n", iterator->name);
		#endif

		if (loadlist_n>=MAXDLLLIST)
		{
			fprintf(stderr, "[lnk] Too many open shared objects\n");
			return; /* this is really not reachable, but nice to test... */
		}

		loadlist[loadlist_n].handle=NULL;

		loadlist[loadlist_n].refcount=1;;

		loadlist[loadlist_n].id=++handlecounter;

		loadlist[loadlist_n].info=iterator;

		loadlist[loadlist_n].info->size=0;

		loadlist_n++;

		iterator++;
	}
	return;
}
#endif

void lnkInit(void)
{
	loadlist_n=0;
	handlecounter=0;
	memset(loadlist, 0, sizeof(loadlist));
#ifdef SUPPORT_STATIC_PLUGINS
	lnkLoadStatics();
#endif
}

void *lnkGetSymbol(const int id, const char *name)
{
	int i;
	if (!id)
	{
		void *retval;
		for (i=loadlist_n-1;i>=0;i--)
			if ((retval=dlsym(loadlist[i].handle, name)))
				return retval;
	} else
		for (i=loadlist_n-1;i>=0;i--)
			if (loadlist[i].id==id)
				return dlsym(loadlist[i].handle, name);
	return NULL;
}

int lnkCountLinks(void)
{
	return loadlist_n;
}

int lnkGetLinkInfo(struct linkinfostruct *l, off_t *size, int index)
{
	if (index<0)
		return 0;
	if (index>=loadlist_n)
		return 0;
	if (!loadlist[index].info)
		return 0;
	memcpy(l, loadlist[index].info, sizeof(struct linkinfostruct));
	*size = loadlist[index].size;

	return 1;
}

int lnkInitAll (void)
{
	int i;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->PreInit)
			if (loadlist[i].info->PreInit()<0)
				return 1;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->Init)
			if (loadlist[i].info->Init()<0)
				return 1;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->LateInit)
			if (loadlist[i].info->LateInit()<0)
				return 1;

	return 0;
}

void lnkCloseAll (void)
{
	int i;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->PreClose)
			loadlist[i].info->PreClose();

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->Close)
			loadlist[i].info->Close();

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->LateClose)
			loadlist[i].info->LateClose();
}

