#ifndef __PSETTING_H
#define __PSETTING_H

int cfGetConfig (int argc, char *argv[]);

#define cfStoreConfig configAPI.StoreConfig

void cfCloseConfig (void);

#define cfGetProfileString  configAPI.GetProfileString
#define cfGetProfileString2 configAPI.GetProfileString2
#define cfSetProfileString  configAPI.SetProfileString

#define cfGetProfileBool    configAPI.GetProfileBool
#define cfGetProfileBool2   configAPI.GetProfileBool2
#define cfSetProfileBool    configAPI.SetProfileBool

#define cfGetProfileInt     configAPI.GetProfileInt
#define cfGetProfileInt2    configAPI.GetProfileInt2
#define cfSetProfileInt     configAPI.SetProfileInt

#define cfGetProfileComment configAPI.GetProfileComment
#define cfSetProfileComment configAPI.SetProfileComment

#define cfRemoveEntry       configAPI.RemoveEntry
#define cfRemoveProfile     configAPI.RemoveProfile
#define cfHomeDir           configAPI.HomeDir
#define cfConfigHomeDir     configAPI.ConfigHomeDir
#define cfDataHomeDir       configAPI.DataHomeDir
#define cfDataDir           configAPI.DataDir
#define cfTempDir           configAPI.TempDir
#define cfConfigSec         configAPI.ConfigSec
#define cfSoundSec          configAPI.SoundSec
#define cfScreenSec         configAPI.ScreenSec

#define cfCountSpaceList    configAPI.CountSpaceList
#define cfGetSpaceListEntry configAPI.GetSpaceListEntry

extern char *cfProgramDir;
extern char *cfProgramDirAutoload;

struct configAPI_t
{
	int (*StoreConfig) (void);

	const char *(*GetProfileString) (const char *app, const char *key, const char *def);
	const char *(*GetProfileString2)(const char *app, const char *app2, const char *key, const char *def);
	void        (*SetProfileString) (const char *app, const char *key, const char *str);

	int  (*GetProfileBool) (const char *app, const char *key, int def, int err);
	int  (*GetProfileBool2)(const char *app, const char *app2, const char *key, int def, int err);
	void (*SetProfileBool) (const char *app, const char *key, const int str);

	int  (*GetProfileInt) (const char *app, const char *key, int def, int radix);
	int  (*GetProfileInt2)(const char *app, const char *app2, const char *key, int def, int radix);
	void (*SetProfileInt) (const char *app, const char *key, int str, int radix);

	const char *(*GetProfileComment) (const char *app, const char *key, const char *def);
	void        (*SetProfileComment) (const char *app, const char *key, const char *comment);

	void (*RemoveEntry)(const char *app, const char *key);

	void (*RemoveProfile)(const char *app);

	char *HomeDir;
	char *ConfigHomeDir;
	char *DataHomeDir;
	char *DataDir;
	char *TempDir;
	const char *ConfigSec;
	const char *SoundSec;
	const char *ScreenSec;

	int (*CountSpaceList)(const char *str, int maxlen);
	int (*GetSpaceListEntry)(char *buf, const char **str, int maxlen);
};

extern struct configAPI_t configAPI;

#endif
