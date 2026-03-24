#ifndef _POLL_H
#define _POLL_H

enum pollType
{
	pollTypeAudio = 0,
	pollTypeVideo = 1,
};

int pollInit (void (*)(void), enum pollType type);
void pollClose (enum pollType type);

void tmTimerHandler (enum pollType type);

#endif
