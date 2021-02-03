#ifndef _ADB_META_H
#define _ADB_META_H

// Some archievers might need to store some information that in not quickly accessible, like the uncompressed filesize, location of frames, ..

extern int adbMetaInit(void);
extern void adbMetaCommit(void);
extern void adbMetaClose(void);
                                                                            // ZIP, GZ, BZ2, etc - who owns this meta-data
extern int adbMetaAdd    (const char *filename, const size_t filesize, const char *SIG, const unsigned char  *data, const size_t  datasize); // Replaces too
extern int adbMetaRemove (const char *filename, const size_t filesize, const char *SIG);

// when done, use free()
extern int adbMetaGet    (const char *filename, const size_t filesize, const char *SIG,       unsigned char **data,       size_t *datasize);

#endif
