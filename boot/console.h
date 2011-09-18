#ifndef _CONSOLE
#define _CONSOLE 1

extern void (*_vga13)(void);
extern void (*_plSetTextMode)(uint8_t x);
extern void (*_plSetBarFont)(void);
extern void (*_plDisplaySetupTextMode)(void);
extern const char *(*_plGetDisplayTextModeName)(void);

extern void (*_displaystr)(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);
extern void (*_displaystrattr)(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len);
extern void (*_displaystrattrdi)(uint16_t y, uint16_t x, const char *txt, const char *attr, uint16_t len);
extern void (*_displayvoid)(uint16_t y, uint16_t x, uint16_t len);

extern int (*_plSetGraphMode)(int size);
extern void (*_gdrawchar)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
extern void (*_gdrawchart)(uint16_t x, uint16_t y, uint8_t c, uint8_t f);
extern void (*_gdrawcharp)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
extern void (*_gdrawchar8)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, uint8_t b);
extern void (*_gdrawchar8t)(uint16_t x, uint16_t y, uint8_t c, uint8_t f);
extern void (*_gdrawchar8p)(uint16_t x, uint16_t y, uint8_t c, uint8_t f, void *picp);
extern void (*_gdrawstr)(uint16_t y, uint16_t x, const char *s, uint16_t len, uint8_t f, uint8_t b);
extern void (*_gupdatestr)(uint16_t y, uint16_t x, const uint16_t *str, uint16_t len, uint16_t *old);
extern void (*_gupdatepal)(uint8_t color, uint8_t red, uint8_t green, uint8_t blue);
extern void (*_gflushpal)(void);

extern int (*_ekbhit)(void);
extern int (*_egetch)(void);
extern int (*_validkey)(uint16_t);

extern void (*_drawbar)(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);
extern void (*_idrawbar)(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c);

extern void (*_Screenshot)(void);
extern void (*_TextScreenshot)(int scrType);
extern void (*_RefreshScreen)(void);

extern void (*_setcur)(uint8_t y, uint8_t x);
extern void (*_setcurshape)(uint16_t shape);

extern int (*_conRestore)(void);
extern void (*_conSave)(void);

extern void (*_plDosShell)(void);

#endif
