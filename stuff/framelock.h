#ifndef _FRAMELOCK_H
#define _FRAMELOCK_H 1

void framelock(void); /* sleeps until next timeout */

int poll_framelock(void); /* returns non-zero at framelock intervals, to be used for culling when I/O is expected to be high */
void preemptive_framelock (void); /* called by I/O intensive sections, but return return value is delayed until the next call to poll_framelock */

extern int fsFPS;

#endif
