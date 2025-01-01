/* OpenCP Module Player
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Wrapper around curl, downloading files via HTTP/HTTPS
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
# include <winbase.h>
#else
# include <sys/wait.h>
#endif
#include "types.h"

#include "download.h"

#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-textfile.h"
#include "stuff/piperun.h"

static struct ocpfilehandle_t *download_request_resolve (struct download_request_t *req, const char *filename);

struct download_request_t *download_request_spawn (const struct configAPI_t *configAPI, void *tag, const char *URL)
{
	static unsigned int sequence = 0;
	struct download_request_t *req = calloc (sizeof (*req), 1);
	size_t l1, l2, l3, l4;

	if (!req)
	{
		return 0;
	}

	req->tag = tag;
	req->configAPI = configAPI;
	req->httpcode = -1;
	req->errcode = -1;

	req->tempheader_filename = malloc (l1 = (20 + 20 + 21));
	req->tempdata_filename   = malloc (l2 = (20 + 20 + 19));
	req->tempheader_filepath = malloc (l3 = (strlen (configAPI->TempPath) + 20 + 20 + 21));
	req->tempdata_filepath   = malloc (l4 = (strlen (configAPI->TempPath) + 20 + 20 + 19));

	if ((!req->tempheader_filename) || (!req->tempdata_filename) || (!req->tempheader_filepath) || (!req->tempdata_filepath))
	{
		free (req->tempheader_filename);
		free (req->tempdata_filename);
		free (req->tempheader_filepath);
		free (req->tempdata_filepath);
		free (req);
		return 0;
	}

	snprintf (req->tempheader_filename, l1, "ocp-headertemp-%d-%d.txt", getpid(), ++sequence);
	snprintf (req->tempdata_filename,   l2, "ocp-datatemp-%d-%d.dat",   getpid(),   sequence);
	snprintf (req->tempheader_filepath, l3, "%s%s", configAPI->TempPath, req->tempheader_filename);
	snprintf (req->tempdata_filepath,   l4, "%s%s", configAPI->TempPath, req->tempdata_filename);

	{
		const char *command_line[] =
		{
#ifdef _WIN32
			"curl.exe",
#else
			"curl",
#endif
			"-L",
			"--user-agent", "opencubicplayer/" PACKAGE_VERSION " ( stian.skjelstad@gmail.com )",
			"--max-redirs", "10",
			"--max-time", "120",
			"--dump-header", req->tempheader_filepath,
			"--output", req->tempdata_filepath,
			URL,
			0
		};

		req->pipehandle = ocpPipeProcess_create (command_line);
		if (!req->pipehandle)
		{
			free (req->tempheader_filename);
			free (req->tempdata_filename);
			free (req->tempheader_filepath);
			free (req->tempdata_filepath);
			free (req);
			return 0;
		}
		return req;
	}
}

static int parse_http_date (const char *input, unsigned int *_Year, unsigned char *_Month, unsigned char *_Day, unsigned char *_Hour, unsigned char *_Minute, unsigned char *_Second)
{
	char *Date = (char *)input; // strtol endptr is non-const
	long Year, Month, Day, Hour, Minute, Second;

	while (*Date == ' ') Date++;

	if (strlen (Date) < 5) return -1;
	if (Date[3] == ',') /* IMF-fixdate, "Sun, 06 Nov 1994 08:49:37 GMT" */
	{
		Date += 4;
		Day = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		if (!strncasecmp (Date, "Jan ", 4)) Month =  1; else
		if (!strncasecmp (Date, "Feb ", 4)) Month =  2; else
		if (!strncasecmp (Date, "Mar ", 4)) Month =  3; else
		if (!strncasecmp (Date, "Apr ", 4)) Month =  4; else
		if (!strncasecmp (Date, "May ", 4)) Month =  5; else
		if (!strncasecmp (Date, "Jun ", 4)) Month =  6; else
		if (!strncasecmp (Date, "Jul ", 4)) Month =  7; else
		if (!strncasecmp (Date, "Aug ", 4)) Month =  8; else
		if (!strncasecmp (Date, "Sep ", 4)) Month =  9; else
		if (!strncasecmp (Date, "Oct ", 4)) Month = 10; else
		if (!strncasecmp (Date, "Nov ", 4)) Month = 11; else
		if (!strncasecmp (Date, "Dec ", 4)) Month = 12; else return -1;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0] == ' ') Date++;
		Year = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		Hour = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Minute = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Second = strtol (Date, &Date, 10);
	} else if (Date[3] == ' ') /* ANSI C's asctime() format, "Sun Nov  6 08:49:37 1994" */
	{
		while (Date[0] && (Date[0] != ' ')) Date++;
		if (Date[0] == ' ') Date++;
		if (!strncasecmp (Date, "Jan ", 4)) Month =  1; else
		if (!strncasecmp (Date, "Feb ", 4)) Month =  2; else
		if (!strncasecmp (Date, "Mar ", 4)) Month =  3; else
		if (!strncasecmp (Date, "Apr ", 4)) Month =  4; else
		if (!strncasecmp (Date, "May ", 4)) Month =  5; else
		if (!strncasecmp (Date, "Jun ", 4)) Month =  6; else
		if (!strncasecmp (Date, "Jul ", 4)) Month =  7; else
		if (!strncasecmp (Date, "Aug ", 4)) Month =  8; else
		if (!strncasecmp (Date, "Sep ", 4)) Month =  9; else
		if (!strncasecmp (Date, "Oct ", 4)) Month = 10; else
		if (!strncasecmp (Date, "Nov ", 4)) Month = 11; else
		if (!strncasecmp (Date, "Dec ", 4)) Month = 12; else return -1;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		while (Date[0] == ' ') Date++;
		Day = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		Hour = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Minute = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Second = strtol (Date, &Date, 10);
		if (Date[0] == ' ') Date++;
		Year = strtol (Date, &Date, 10);
	} else { /* RFC 850 format, "Sunday, 06-Nov-94 08:49:37 GMT" */
		while (Date[0] && (Date[0] != ' ')) Date++;
		if (Date[0] == ' ') Date++;
		Day = strtol (Date, &Date, 10);
		if (Date[0] == '-') Date++;
		if (!strncasecmp (Date, "Jan-", 4)) Month =  1; else
		if (!strncasecmp (Date, "Feb-", 4)) Month =  2; else
		if (!strncasecmp (Date, "Mar-", 4)) Month =  3; else
		if (!strncasecmp (Date, "Apr-", 4)) Month =  4; else
		if (!strncasecmp (Date, "May-", 4)) Month =  5; else
		if (!strncasecmp (Date, "Jun-", 4)) Month =  6; else
		if (!strncasecmp (Date, "Jul-", 4)) Month =  7; else
		if (!strncasecmp (Date, "Aug-", 4)) Month =  8; else
		if (!strncasecmp (Date, "Sep-", 4)) Month =  9; else
		if (!strncasecmp (Date, "Oct-", 4)) Month = 10; else
		if (!strncasecmp (Date, "Nov-", 4)) Month = 11; else
		if (!strncasecmp (Date, "Dec-", 4)) Month = 12; else return -1;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0]) Date++;
		if (Date[0] == '-') Date++;
		Year = strtol (Date, &Date, 10);
		if ((Year >= 0) && (Year < 100)) Year += 1900;
		if (Date[0] == ' ') Date++;
		Hour = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Minute = strtol (Date, &Date, 10);
		if (Date[0] == ':') Date++;
		Second = strtol (Date, &Date, 10);
	}

	if (Year < 1970) return -1;
	if ((Day < 1) || (Day > 31)) return -1;
	if ((Hour < 0) || (Hour >= 24)) return -1;
	if ((Minute < 0) || (Minute >= 60)) return -1;
	if ((Second < 0) || (Second > 60)) return -1; /* do we care about leap-seconds ? */

	*_Year = Year;
	*_Month = Month;
	*_Day = Day;
	*_Hour = Hour;
	*_Minute = Minute;
	*_Second = Second;

	return 0;
}

static int download_parse_header_textfile (struct download_request_t *req, struct textfile_t *textfile)
{
	const char *line;

	int fresh_request = 1;

	while ((line = textfile_fgets (textfile)))
	{	/* header can contain multiple header sequences, we want the last one */

		if (!strlen (line))
		{
			fresh_request = 1;
		} else if (fresh_request)
		{
			char *end;
			long newcode;

			/* line should start with HTTP/ */
			if (strncmp (line, "HTTP/", 5))
			{
				req->errmsg = "invalid HTTP header syntax (1)";
				return -1;
			}

			/* contain version and the a space */
			line = strchr (line, ' ');
			if (!line)
			{
				req->errmsg = "invalid HTTP header syntax (2)";
				return -1;
			}
			line++;

			/* before ending with number */
			newcode = strtol (line, &end, 10);
			if (end == line)
			{
				req->errmsg = "invalid HTTP header syntax (3)";
				return -1;
			}

			if ((newcode <= 100) && (newcode >= 600))
			{
				req->errmsg = "invalid HTTP header syntax (4)";
				return -1;
			}
			req->httpcode = newcode;
			fresh_request = 0;
		} else if (!strncasecmp ("Last-Modified: ", line, 15))
		{
			if (parse_http_date (line + 15, &req->Year, &req->Month, &req->Day, &req->Hour, &req->Minute, &req->Second))
			{
				req->errmsg = "Failed to parse HTTP header date";
				return -1;
			}
		} else if (!strncasecmp ("content-length: ", line, 16))
		{
			req->ContentLength = strtoul (line + 16, 0, 10);
		}
	}

	switch (req->httpcode)
	{
		case 200:
			break;
		case 400: req->errmsg = "400 - Bad Request";              return -1;
		case 401: req->errmsg = "401 - Unauthorized";             return -1;
		case 403: req->errmsg = "403 - Forbidden";                return -1;
		case 404: req->errmsg = "404 - Not found";                return -1;
		case 408: req->errmsg = "408 - Request Timeout";          return -1;
		case 426: req->errmsg = "426 - Upgrade Required";         return -1;
		case 429: req->errmsg = "429 - Too Many Requests";        return -1;
		case 500: req->errmsg = "500 - Internal Server Error";    return -1;
		case 502: req->errmsg = "502 - Bad Gateway";              return -1;
		case 503: req->errmsg = "503 - Service Unavailable";      return -1;
		case 504: req->errmsg = "504 - Gateway Timeout";          return -1;
		case 521: req->errmsg = "512 - Web Server Is Down";       return -1;
		case 522: req->errmsg = "522 - Connection Timed Out";     return -1;
		case 523: req->errmsg = "523 - Origin Is Unreachable";    return -1;
		case 524: req->errmsg = "524 - A Timeout Occurred";       return -1;
		default:  req->errmsg = "Not the expected 200 HTTP code"; return -1;
	}
	return 0;
}

static int download_parse_header (struct download_request_t *req)
{
	struct ocpfilehandle_t *filehandle;
	struct textfile_t *textfile;
	int retval;

	filehandle = download_request_resolve (req, req->tempheader_filename);
	if (!filehandle)
	{
		req->errmsg = "Unable to open file";
		return -1;
	}

	textfile = textfile_start (filehandle);
	filehandle->unref (filehandle);
	filehandle = 0;

	if (!textfile)
	{
		req->errmsg = "failed to hand over HTTP header file";
		return -1;
	}

	retval = download_parse_header_textfile (req, textfile);

	textfile_stop (textfile);
	textfile = 0;

	return retval;
}

int download_request_iterate (struct download_request_t *req)
{
	int a, b;

	if ((!req) || (!req->pipehandle))
	{
		return 0;
	}

	a = ocpPipeProcess_read_stdout (req->pipehandle, req->stdoutbin, sizeof (req->stdoutbin));
	b = ocpPipeProcess_read_stderr (req->pipehandle, req->stderrbin, sizeof (req->stderrbin));

	if ((a >= 0) || (b >= 0))
	{
		struct stat st;
		if (!stat (req->tempdata_filepath, &st))
		{
			req->ContentLength = st.st_size;
		}
		return 1;
	}

	req->errcode = ocpPipeProcess_destroy (req->pipehandle);
	req->pipehandle = 0;

	if (req->errcode)
	{
		req->errmsg = "Curl failed"; // todo, get better messages...?

#ifndef _WIN32
		if (WIFSIGNALED(req->errcode))
		{
			req->errmsg="CURL was aborted due to a signal";
		} else switch (WEXITSTATUS(req->errcode))
#else
		switch (req->errcode)
#endif
		{
			case 1: req->errmsg="CURL: Unsupported protocol. This build of curl has no support for this protocol."; break;
			case 2: req->errmsg="Failed to initialize."; break;
			case 3: req->errmsg="URL malformed. The syntax was not correct."; break;
			case 4: req->errmsg="A feature or option that was needed to perform the desired request was not enabled or was explicitly disabled at build‐time. To make curl able to do this, you probably need another build of libcurl."; break;
			case 5: req->errmsg="Could not resolve proxy. The given proxy host could not be resolved."; break;
			case 6: req->errmsg="Could not resolve host. The given remote host could not be resolved."; break;
			case 7: req->errmsg="Failed to connect to host."; break;
			case 8: req->errmsg="Weird server reply. The server sent data curl could not parse."; break;
			case 9: req->errmsg="FTP access denied. The server denied login or denied access to the particular resource or directory you wanted to reach. Most often you tried to change to a directory that does not exist on the server."; break;
			case 10: req->errmsg="FTP accept failed. While waiting for the server to connect back when an active FTP session is used, an error code was sent over the control connection or similar."; break;
			case 11: req->errmsg="FTP weird PASS reply. Curl could not parse the reply sent to the PASS request."; break;
			case 12: req->errmsg="During an active FTP session while waiting for the server to connect back to curl, the timeout expired."; break;
			case 13: req->errmsg="FTP weird PASV reply, Curl could not parse the reply sent to the PASV request."; break;
			case 14: req->errmsg="FTP weird 227 format. Curl could not parse the 227‐line the server sent."; break;
			case 15: req->errmsg="FTP cannot use host. Could not resolve the host IP we got in the 227‐line."; break;
			case 16: req->errmsg="HTTP/2 error. A problem was detected in the HTTP2 framing layer. This is somewhat generic and can be one out of several problems, see the error message for details."; break;
			case 17: req->errmsg="FTP could not set binary. Could not change transfer method to binary."; break;
			case 18: req->errmsg="Partial file. Only a part of the file was transferred."; break;
			case 19: req->errmsg="FTP could not download/access the given file, the RETR (or similar) command failed."; break;
			case 21: req->errmsg="FTP quote error. A quote command returned error from the server."; break;
			case 22: req->errmsg="HTTP page not retrieved. The requested URL was not found or returned another error with the HTTP error code being 400 or above. This return code only appears if -f, --fail is used."; break;
			case 23: req->errmsg="Write error. Curl could not write data to a local filesystem or similar."; break;
			case 25: req->errmsg="FTP could not STOR file. The server denied the STOR operation, used for FTP uploading."; break;
			case 26: req->errmsg="Read error. Various reading problems."; break;
			case 27: req->errmsg="Out of memory. A memory allocation request failed."; break;
			case 28: req->errmsg="Operation timeout. The specified time‐out period was reached according to the conditions."; break;
			case 30: req->errmsg="FTP PORT failed. The PORT command failed. Not all FTP servers support the PORT command, try doing a transfer using PASV instead!"; break;
			case 31: req->errmsg="FTP could not use REST. The REST command failed. This command is used for resumed FTP transfers."; break;
			case 33: req->errmsg="HTTP range error. The range \"command\" did not work."; break;
			case 34: req->errmsg="HTTP post error. Internal post‐request generation error."; break;
			case 35: req->errmsg="SSL connect error. The SSL handshaking failed."; break;
			case 36: req->errmsg="Bad download resume. Could not continue an earlier aborted download."; break;
			case 37: req->errmsg="FILE could not read file. Failed to open the file. Permissions?"; break;
			case 38: req->errmsg="LDAP cannot bind. LDAP bind operation failed."; break;
			case 39: req->errmsg="LDAP search failed."; break;
			case 41: req->errmsg="Function not found. A required LDAP function was not found."; break;
			case 42: req->errmsg="Aborted by callback. An application told curl to abort the operation."; break;
			case 43: req->errmsg="Internal error. A function was called with a bad parameter."; break;
			case 45: req->errmsg="Interface error. A specified outgoing interface could not be used."; break;
			case 47: req->errmsg="Too many redirects. When following redirects, curl hit the maximum amount."; break;
			case 48: req->errmsg="Unknown option specified to libcurl. This indicates that you passed a weird option to curl that was passed on to libcurl and rejected. Read up in the manual!"; break;
			case 49: req->errmsg="Malformed telnet option."; break;
			case 52: req->errmsg="The server did not reply anything, which here is considered an error."; break;
			case 53: req->errmsg="SSL crypto engine not found."; break;
			case 54: req->errmsg="Cannot set SSL crypto engine as default."; break;
			case 55: req->errmsg="Failed sending network data."; break;
			case 56: req->errmsg="Failure in receiving network data."; break;
			case 58: req->errmsg="Problem with the local certificate."; break;
			case 59: req->errmsg="Could not use specified SSL cipher."; break;
			case 60: req->errmsg="Peer certificate cannot be authenticated with known CA certificates."; break;
			case 61: req->errmsg="Unrecognized transfer encoding."; break;
			case 63: req->errmsg="Maximum file size exceeded."; break;
			case 64: req->errmsg="Requested FTP SSL level failed."; break;
			case 65: req->errmsg="Sending the data requires a rewind that failed."; break;
			case 66: req->errmsg="Failed to initialise SSL Engine."; break;
			case 67: req->errmsg="The user name, password, or similar was not accepted and curl failed to log in."; break;
			case 68: req->errmsg="File not found on TFTP server."; break;
			case 69: req->errmsg="Permission problem on TFTP server."; break;
			case 70: req->errmsg="Out of disk space on TFTP server."; break;
			case 71: req->errmsg="Illegal TFTP operation."; break;
			case 72: req->errmsg="Unknown TFTP transfer ID."; break;
			case 73: req->errmsg="File already exists (TFTP)."; break;
			case 74: req->errmsg="No such user (TFTP)."; break;
			case 77: req->errmsg="Problem reading the SSL CA cert (path? access rights?)."; break;
			case 78: req->errmsg="The resource referenced in the URL does not exist."; break;
			case 79: req->errmsg="An unspecified error occurred during the SSH session."; break;
			case 80: req->errmsg="Failed to shut down the SSL connection."; break;
			case 82: req->errmsg="Could not load CRL file, missing or wrong format."; break;
			case 83: req->errmsg="Issuer check failed."; break;
			case 84: req->errmsg="The FTP PRET command failed."; break;
			case 85: req->errmsg="Mismatch of RTSP CSeq numbers."; break;
			case 86: req->errmsg="Mismatch of RTSP Session Identifiers."; break;
			case 87: req->errmsg="Unable to parse FTP file list."; break;
			case 88: req->errmsg="FTP chunk callback reported error."; break;
			case 89: req->errmsg="No connection available, the session will be queued."; break;
			case 90: req->errmsg="SSL public key does not matched pinned public key."; break;
			case 91: req->errmsg="Invalid SSL certificate status."; break;
			case 92: req->errmsg="Stream error in HTTP/2 framing layer."; break;
			case 93: req->errmsg="An API function was called from inside a callback."; break;
			case 94: req->errmsg="An authentication function returned an error."; break;
			case 95: req->errmsg="A problem was detected in the HTTP/3 layer. This is somewhat generic and can be one out of several problems, see the error message for details."; break;
			case 96: req->errmsg="QUIC connection error. This error may be caused by an SSL library error. QUIC is the protocol used for HTTP/3 transfers."; break;
			case 97: req->errmsg="Proxy handshake error."; break;
			case 98: req->errmsg="A client‐side certificate is required to complete the TLS handshake."; break;
			case 99: req->errmsg="Poll or select returned fatal error."; break;
			default: req->errmsg="CURL failed, unknown reason."; break;
		}
		return 0;
	}
	if (download_parse_header (req))
	{
		return 0;
	}

	return 0;
}

static void download_request_real_free (struct download_request_t *req)
{
#ifdef _WIN32
	DeleteFile (req->tempheader_filepath);
	DeleteFile (req->tempdata_filepath);
#else
	unlink (req->tempheader_filepath);
	unlink (req->tempdata_filepath);
#endif
	free (req->tempheader_filename);
	free (req->tempdata_filename);
	free (req->tempheader_filepath);
	free (req->tempdata_filepath);
	free (req);
}

void download_request_free (struct download_request_t *req)
{
	if (!req)
	{
		return;
	}

	if (req->wrapfilehandles)
	{
		req->free++;
		return;
	} else {
		download_request_real_free (req); /* delay until files are no longer in use */
	}
}

struct download_wrap_ocpfilehandle_t
{
	struct ocpfilehandle_t head;
	struct ocpfilehandle_t *filehandle;
	struct download_request_t *owner;
};
static void download_wrap_ocpfilehandle_ref (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	f->head.refcount++;
}
static void download_wrap_ocpfilehandle_unref (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	if (!(--f->head.refcount))
	{
		f->head.origin->unref (f->head.origin);
		f->head.origin = 0;

		f->filehandle->unref (f->filehandle);
		f->filehandle = 0;

		f->owner->wrapfilehandles--;
		if (f->owner->free)
		{
			download_request_free (f->owner);
		}
		f->owner = 0;

		free (f);
	}
}
static int download_wrap_ocpfilehandle_seek_set (struct ocpfilehandle_t *_f, int64_t pos)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->seek_set (f->filehandle, pos);
}
static uint64_t download_wrap_ocpfilehandle_getpos (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->getpos (f->filehandle);
}
static int download_wrap_ocpfilehandle_eof (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->eof (f->filehandle);
}
static int download_wrap_ocpfilehandle_error (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->error (f->filehandle);
}
static int download_wrap_ocpfilehandle_read (struct ocpfilehandle_t *_f, void *dst, int len)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->read (f->filehandle, dst, len);
}
static int download_wrap_ocpfilehandle_ioctl (struct ocpfilehandle_t *_f, const char *cmd, void *ptr)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->ioctl (f->filehandle, cmd, ptr);
}
static uint64_t download_wrap_ocpfilehandle_filesize (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->filesize (f->filehandle);
}
static int download_wrap_ocpfilehandle_filesize_ready (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->filesize_ready (f->filehandle);
}
static const char *download_wrap_ocpfilehandle_filename_override (struct ocpfilehandle_t *_f)
{
	struct download_wrap_ocpfilehandle_t *f = (struct download_wrap_ocpfilehandle_t *)_f;
	return f->filehandle->filename_override (f->filehandle);
}
static struct ocpfilehandle_t *download_request_resolve (struct download_request_t *req, const char *filename)
{
	uint32_t headerfile_ref;
	struct ocpfile_t *file = 0;
	struct ocpfilehandle_t *filehandle = 0;
	struct download_wrap_ocpfilehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	if (!retval)
	{
		return 0;
	}

	headerfile_ref = dirdbFindAndRef (req->configAPI->TempDir->dirdb_ref, filename, dirdb_use_file);
	file = req->configAPI->TempDir->readdir_file (req->configAPI->TempDir, headerfile_ref);
	dirdbUnref (headerfile_ref, dirdb_use_file);

	if (!file)
	{
		free (retval);
		return 0;
	}
	filehandle = file->open (file);
	if (!filehandle)
	{
		free (retval);
		return 0;
	}
	ocpfilehandle_t_fill
	(
		&retval->head,
		download_wrap_ocpfilehandle_ref,
		download_wrap_ocpfilehandle_unref,
		file, /* takes over the ref count */
		download_wrap_ocpfilehandle_seek_set,
		download_wrap_ocpfilehandle_getpos,
		download_wrap_ocpfilehandle_eof,
		download_wrap_ocpfilehandle_error,
		download_wrap_ocpfilehandle_read,
		download_wrap_ocpfilehandle_ioctl,
		download_wrap_ocpfilehandle_filesize,
		download_wrap_ocpfilehandle_filesize_ready,
		download_wrap_ocpfilehandle_filename_override,
		filehandle->dirdb_ref,
		1
	);
	retval->filehandle = filehandle;
	retval->owner = req;
	retval->owner->wrapfilehandles++;
	return &retval->head;
}

struct ocpfilehandle_t *download_request_getfilehandle (struct download_request_t *req)
{
	if (!req)
	{
		return 0;
	}
	if (req->pipehandle)
	{
		return 0;
	}
	return download_request_resolve (req, req->tempdata_filename);
}

void download_request_cancel (struct download_request_t *req)
{
	if ((!req) || (!req->pipehandle))
	{
		return;
	}

	ocpPipeProcess_terminate (req->pipehandle);

	do
	{
		int a, b;

		a = ocpPipeProcess_read_stdout (req->pipehandle, req->stdoutbin, sizeof (req->stdoutbin));
		b = ocpPipeProcess_read_stderr (req->pipehandle, req->stderrbin, sizeof (req->stderrbin));

		if ((a >= 0) || (b >= 0))
		{
			usleep(10000);
			continue;
		}
	} while (0);

	ocpPipeProcess_destroy (req->pipehandle);
	req->pipehandle = 0;
}

char *urlencode(const char *src)
{
	const char *h = "0123456789abcdef";
	char *retval = malloc (strlen (src) * 3 + 1);
	const char *s;
	char *d;

	if (!retval)
	{
		return 0;
	}

	for (d = retval, s = src; *s; s++)
	{
		if ( ((*s >= '0') && (*s <= '9')) ||
		     ((*s >= 'a') && (*s <= 'z')) ||
		     ((*s >= 'A') && (*s <= 'Z')) ||
		     (*s == '/') )
		{
			*d = *s; d++;
		} else {
			*d = '%'; d++;
			*d = h[(*(unsigned char *)s) >> 4]; d++;
			*d = h[(*(unsigned char *)s) & 15]; d++;
		}
	}
	*d = 0;
	return retval;
}

