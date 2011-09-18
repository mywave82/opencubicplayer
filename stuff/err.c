/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * error strings (doesn't really help...)
 *   feel free to add better descriptions.
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
 *  -fd990818   Felix Domke <tmbinc@gmx.net>
 *    -first release
 */

#include "config.h"
#include "types.h"
#include "err.h"

const char *errGetShortString(int err)
{
	switch(err)
	{
		case errOk:
			return "no error";
		case errGen:
			return "generic error";
		case errAllocMem:
			return "not enough memory";
		case errAllocSamp:
			return "not enough memory on soundcard";
		case errFileOpen:
			return "file couldn't be opened";
		case errFileRead:
			return "read error";
		case errFileWrite:
			return "write error";
		case errFileMiss:
			return "file is missing";
		case errFormStruc:
			return "file structure corrupted";
		case errFormSig:
			return "signature not found";
		case errFormOldVer:
			return "too old version of file";
		case errFormNewVer:
			return "too new version of file";
		case errFormSupp:
			return "format feature not supported";
		case errFormMiss:
			return "something missing in file (corrupted?)";
		case errPlay:
			return "couldn't play (device error?)";
		case errSymSym:
			return "symbol not found";
		case errSymMod:
			return "dll not found";
	}
	return "unknown error";
}

const char *errGetLongString(int err)
{
	switch(err)
	{
		case errOk:
			return "No error occured.";
		case errGen:
			return "Generic, unspecified error.";
		case errAllocMem:
			return "There was not enough memory, or possible an invalid/corrupted filestructure.";
		case errAllocSamp:
			return "Out of memory on allocating memory for samples.";
		case errFileOpen:
			return "The file could not be opened.";
		case errFileRead:
			return "Could not read from the file.";
		case errFileWrite:
			return "Could not write to the file.";
		case errFileMiss:
			return "A file is missing.";
		case errFormStruc:
			return "There was an serious error in the file-format-structure.";
		case errFormSig:
			return "A file-format-signature was not found. Maybe it's a wrong fileformat?";
		case errFormOldVer:
			return "Too old version of file. Please re-save in a newer version of the tracker.";
		case errFormNewVer:
			return "Too new version of file.";
		case errFormSupp:
			return "A feature supported by the fileformat is not supported.";
		case errFormMiss:
			return "A section was missing inside the file. Maybe it's corrupted.";
		case errPlay:
			return "Couldn't play. Maybe a device-error occured.";
		case errSymSym:
			return "A symbol inside a DLL could not be found.";
		case errSymMod:
			return "A DLL could not be found.";
	}
	return "Another, unknown error occured.";
}
