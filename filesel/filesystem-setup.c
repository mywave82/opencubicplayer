/* OpenCP Module Player
 * copyright (c) 2020-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to handle the virtual drive SETUP:
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

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-dir-mem.h"
#include "filesystem-drive.h"
#include "filesystem-setup.h"
#include "pfilesel.h"

struct dmDrive *dmSetup;
struct ocpdir_mem_t *setup_root;

void filesystem_setup_register (void)
{
	struct ocpdir_t *t;
	setup_root = ocpdir_mem_alloc (0, "setup:");
	if (!setup_root)
	{
		fprintf (stderr, "filesystem_setup_register(): out of memory!\n");
		return;
	}

	t = ocpdir_mem_getdir_t (setup_root);
	dmSetup = RegisterDrive("setup:", t, t);
	t->unref (t);
}

void filesystem_setup_register_dir (struct ocpdir_t *dir)
{
	ocpdir_mem_add_dir (setup_root, dir);
}

void filesystem_setup_unregister_dir (struct ocpdir_t *dir)
{
	ocpdir_mem_remove_dir (setup_root, dir);
}

void filesystem_setup_register_file (struct ocpfile_t *file)
{
	ocpdir_mem_add_file (setup_root, file);
}

void filesystem_setup_unregister_file (struct ocpfile_t *file)
{
	ocpdir_mem_remove_file (setup_root, file);
}
