#ifndef _FB_OCP
#define _FB_OCP 1

struct consoleDriver_t;

int fb_init (int minor, struct consoleDriver_t *driver);
void fb_done (void);

#endif
