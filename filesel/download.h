#ifndef OCP_DOWNLOAD_H
#define OCP_DOWNLOAD_H 1

struct configAPI_t;
struct ocpfile_t;

struct download_request_t
{
	void *tag;

	const struct configAPI_t *configAPI;

	void *pipehandle;
	char stdoutbin[32];
	char stderrbin[32];

	int httpcode; /* we only accept 200 */
	int errcode; /* exit-code from curl, if any */
	const char *errmsg;

	char *tempheader_filepath;
	char *tempdata_filepath;
	char *tempheader_filename;
	char *tempdata_filename;

/* result */
	unsigned long ContentLength; // updated with filesize as curl downloads
	unsigned int Year;
	unsigned char Month;
	unsigned char Day;
	unsigned char Hour;
	unsigned char Minute;
	unsigned char Second;

	/* In windows, files can not be deleted while open, and debugging is easier if files are deleted first when closed */
	int free;            /* free requested from topside */
	int wrapfilehandles; /* inhibits free from finialize */
};

struct download_request_t *download_request_spawn (const struct configAPI_t *configAPI, void *tag, const char *URL); /* "https://modland.com/allmods.zip" */

char *urlencode(const char *src);

int download_request_iterate (struct download_request_t *req); /* repeat calling this until it returns zero */

void download_request_cancel (struct download_request_t *req);

struct ocpfilehandle_t *download_request_getfilehandle (struct download_request_t *req); /* use with req->tempdata_filename */

void download_request_free (struct download_request_t *req); /* on Windows, do not call until all files are closed */

#endif
