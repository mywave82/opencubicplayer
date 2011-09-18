#ifndef __IRQ_H
#define __IRQ_H

int irqInit(int signum, void (*rout)(int), int recursive); /* WE run on the same stack */
void irqDone(int signum);

#endif
