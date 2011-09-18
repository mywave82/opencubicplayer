#ifndef __PSETTING_H
#define __PSETTING_H

extern int cfGetConfig(int argc, char *argv[]);
extern int cfStoreConfig(void);
extern void cfCloseConfig(void);
extern int cfCountSpaceList(const char *str, int maxlen);
extern int cfGetSpaceListEntry(char *buf, const char **str, int maxlen);
extern const char *cfGetProfileString(const char *app, const char *key, const char *def);
extern const char *cfGetProfileString2(const char *app, const char *app2, const char *key, const char *def);
extern void cfSetProfileString(const char *app, const char *key, const char *str);
extern int cfGetProfileBool(const char *app, const char *key, int def, int err);
extern int cfGetProfileBool2(const char *app, const char *app2, const char *key, int def, int err);
extern void cfSetProfileBool(const char *app, const char *key, const int str);
extern int cfGetProfileInt(const char *app, const char *key, int def, int radix);
extern int cfGetProfileInt2(const char *app, const char *app2, const char *key, int def, int radix);
extern void cfSetProfileInt(const char *app, const char *key, int str, int radix);
extern void cfRemoveEntry(const char *app, const char *key);

extern char cfConfigDir[];
extern char cfDataDir[];
extern char cfTempDir[];
extern char cfProgramDir[];
extern const char *cfConfigSec;
extern const char *cfSoundSec;
extern const char *cfScreenSec;


#endif
