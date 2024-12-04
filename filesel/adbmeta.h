#ifndef _ADB_META_H
#define _ADB_META_H

// Some archievers might need to store some information that in not quickly accessible, like the uncompressed filesize, location of frames, ..

struct configAPI_t;
int adbMetaInit (const struct configAPI_t *configAPI);
void adbMetaCommit (void);
void adbMetaClose (void);
                                                                            // ZIP, GZ, BZ2, etc - who owns this meta-data
int adbMetaAdd    (const char *filename, const uint64_t filesize, const char *SIG, const unsigned char  *data, const uint32_t  datasize); // Replaces too
int adbMetaRemove (const char *filename, const uint64_t filesize, const char *SIG);

// when done, use free()
int adbMetaGet    (const char *filename, const uint64_t filesize, const char *SIG,       unsigned char **data,       uint32_t *datasize);

#endif
