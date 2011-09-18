#ifndef _PLAYLIST_H
#define _PLAYLIST_H

struct modlist;
extern void fsAddPlaylist(struct modlist *ml, const char *path, const char *mask, unsigned long opt, const char *source);

#endif
