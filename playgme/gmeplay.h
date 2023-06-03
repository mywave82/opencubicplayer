#ifndef _GMEPLAY_H
#define _GMEPLAY_H

struct moduletype;
struct gmeinfo
{
	int track;
	int numtracks;
	const char* system;
	const char* game;
	const char* song;
	const char* author;
	const char* copyright;
	const char* comment;
	const char* dumper;
        int length;
	int introlength;
	int looplength;
	int playlength;
};


OCP_INTERNAL extern struct moduletype gme_mt;
OCP_INTERNAL extern const char *gme_filename;

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;

OCP_INTERNAL int gmeOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void gmeClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void gmeIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void gmeSetLoop (int s);
OCP_INTERNAL int gmeIsLooped (void);
OCP_INTERNAL void gmeGetInfo (struct gmeinfo *);
OCP_INTERNAL void gmeStartSong (struct cpifaceSessionAPI_t *cpifaceSession, int song);

OCP_INTERNAL void gmeInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void gmeInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
