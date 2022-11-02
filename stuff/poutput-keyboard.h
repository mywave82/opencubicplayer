#ifndef __POUTPUT_KEYBOARD_H
#define __POUTPUT_KEYBOARD_H

#ifdef _CONSOLE_DRIVER

/* kbhit and getch will be called on ekbhit() and egetch() - they can be dummy callbacks to refresh the console.
 *
 * Console drivers that implemented getch() are expected to return keyboard/console escape codes.
 *
 * Console drivers that decode keyboard themself should use ___push_key(), and international characters are split up into UTF-8 bytes
 */
void ___setup_key (int(*kbhit)(void), int(*getch)(void));
void ___push_key (uint16_t);
#endif

int consoleHasKey (uint16_t key);

int ekbhit (void);
int egetch (void);

#endif
