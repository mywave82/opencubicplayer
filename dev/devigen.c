/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * DevIGen - devices system overhead
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
 *    -changed INI reading of driver symbols to _dllinfo lookup
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "imsdev.h"
#include "boot/psetting.h"
#include "boot/plinkman.h"
#include "devigen.h"

int deviReadDevices(const char *list, struct devinfonode **devs)
{
	int handlfn=1;

	while (1)
	{
		char drvhand[9];
		struct devinfonode *n;
		int i;
		char dname[20];
		char *dsym;
		struct sounddevice *dev;
		int bypass;

		if (!cfGetSpaceListEntry(drvhand, &list, 8))
			break;

		n=malloc(sizeof(struct devinfonode));
		if (!n)
			return 0;
		n->next=0;
		strcpy(n->handle, drvhand);

		fprintf(stderr, " %s", drvhand);
		for (i=strlen(drvhand); i<8; i++)
			fprintf(stderr, " ");
		fprintf(stderr, ": ");

		strncpy(dname,cfGetProfileString(drvhand, "link", ""),19);

		n->linkhand=lnkLink(dname);
		if (n->linkhand<=0)
		{
			fprintf(stderr, "link error\n");
			free(n);
			continue;
		}

		if (!(dsym=lnkReadInfoReg(n->linkhand, "driver")))
		{
			fprintf(stderr, "not a driver\n");
			lnkFree(n->linkhand);
			free(n);
			continue;
		}
		if (!*dsym) /* can this shit occure? that a driver string is zero-size long... and why care? */
		{
			fprintf(stderr, "no driver found\n");
			lnkFree(n->linkhand);
			free(n);
			continue;
		}
#ifdef LD_DEBUG
		fprintf(stderr, "dsym: \"%s\"\n", dsym);
#endif
		if (!(dev=(struct sounddevice*)_lnkGetSymbol(dsym)))
		{
			fprintf(stderr, "sym error\n");
			lnkFree(n->linkhand);
			free(n);
			continue;
		}

		bypass=cfGetProfileBool(drvhand, "bypass", 0, 0);
		n->ihandle=handlfn++;
		n->keep=dev->keep;
		n->devinfo.port=cfGetProfileInt(drvhand, "port", -1, 16);
		n->devinfo.port2=cfGetProfileInt(drvhand, "port2", -1, 16);
/*
		n->dev.irq=cfGetProfileInt(drvhand, "irq", -1, 10);
		n->dev.irq2=cfGetProfileInt(drvhand, "irq2", -1, 10);
		n->dev.dma=cfGetProfileInt(drvhand, "dma", -1, 10);
		n->dev.dma2=cfGetProfileInt(drvhand, "dma2", -1, 10);
*/
		n->devinfo.subtype=cfGetProfileInt(drvhand, "subtype", -1, 10);
		strncpy(n->devinfo.path, cfGetProfileString(drvhand, "path", ""), sizeof(n->devinfo.path));
		n->devinfo.path[sizeof(n->devinfo.path)-1]=0;
		strncpy(n->devinfo.mixer, cfGetProfileString(drvhand, "mixer", ""), sizeof(n->devinfo.mixer));
		n->devinfo.mixer[sizeof(n->devinfo.mixer)-1]=0;
		n->devinfo.chan=0;
		n->devinfo.mem=0;
		n->devinfo.opt=0;

		strcpy(n->name, dev->name);
		if (dev->addprocs)
			if (dev->addprocs->GetOpt)
				n->devinfo.opt=dev->addprocs->GetOpt(drvhand);
		n->devinfo.opt|=cfGetProfileInt(drvhand, "options", 0, 16);

		fprintf(stderr, "%s", n->name);
		for (i=strlen(n->name); i<32; i++)
			fprintf(stderr, ".");
		if (!bypass)
		{
			if (!dev->Detect(&n->devinfo))
			{
				fprintf(stderr, " not found: optimize ocp.ini!\n");
				lnkFree(n->linkhand);
				free(n);
				continue;
			}
		} else
			n->devinfo.devtype=dev;

		if (!n->keep)
		{
			lnkFree(n->linkhand);
			n->linkhand=-1;
		}

		fprintf(stderr, " (#%d", n->ihandle);
		if (n->devinfo.subtype!=-1)
			fprintf(stderr, " t%d", n->devinfo.subtype);
		if (n->devinfo.port!=-1)
			fprintf(stderr, " p%x", n->devinfo.port);
		if (n->devinfo.port2!=-1)
			fprintf(stderr, " q%x", n->devinfo.port2);
		if (n->devinfo.mem)
			fprintf(stderr, " m%d", (int)(n->devinfo.mem>>10));
		fprintf(stderr, ")\n");

/*
		strcpy(str, " (");
		strcat(str, "#");
		ltoa(n->ihandle, str+strlen(str), 10);
		if (n->dev.subtype!=-1)
		{
			strcat(str, " t");
			ltoa(n->dev.subtype, str+strlen(str), 10);
		}
		if (n->dev.port!=-1)
		{
			strcat(str, " p");
			ltoa(n->dev.port, str+strlen(str), 16);
		}
		if (n->dev.port2!=-1)
		{
			strcat(str, " q");
			ltoa(n->dev.port2, str+strlen(str), 16);
		}
		if (n->dev.irq!=-1)
		{
			strcat(str, " i");
			ltoa(n->dev.irq, str+strlen(str), 10);
		}
		if (n->dev.irq2!=-1)
		{
			strcat(str, " j");
			ltoa(n->dev.irq2, str+strlen(str), 10);
		}
		if (n->dev.dma!=-1)
		{
			strcat(str, " d");
			ltoa(n->dev.dma, str+strlen(str), 10);
		}
		if (n->dev.dma2!=-1)
		{
			strcat(str, " e");
			ltoa(n->dev.dma2, str+strlen(str), 10);
		}
		if (n->dev.mem)
		{
			strcat(str, " m");
			ltoa(n->dev.mem>>10, str+strlen(str), 10);
		}
		printf("%s)\n", str);*/

		*devs=n;
		devs=&(*devs)->next;
	}
	return 1;
}

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {"devi", "OpenCP Devices Auxiliary Routines (c) 1994-10 Niklas Beisert, Tammo Hinrichs", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
