#ifndef __TIMER_H
#define __TIMER_H

#include <time.h>

int  tmInit(void (*)(void), int timeval);
void tmClose(void);
void tmSetNewRate(int);
time_t tmGetTimer(void);
int  tmGetCpuUsage(void);
void tmSetSecure(void);
void tmReleaseSecure(void);
#ifdef DISABLE_SIGALRM
void tmTimerHandler(void);
#endif

#endif
