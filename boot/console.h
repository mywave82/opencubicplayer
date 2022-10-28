#ifndef _CONSOLE
#define _CONSOLE 1

struct consoleDriver_t;
struct consoleStatus_t;

extern const struct consoleDriver_t *conDriver;
extern struct consoleStatus_t conStatus;

extern const struct consoleDriver_t dummyConsoleDriver;

#endif
