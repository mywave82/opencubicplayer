#ifndef __SETS_H
#define __SETS_H

struct settings
{
  int16_t amp;
  int16_t speed;
  int16_t pitch;
  int16_t pan;
  int16_t bal;
  int16_t vol;
  int16_t srnd;
  int16_t filter;
  uint16_t useecho;
  int16_t reverb;
  int16_t chorus;
};

extern struct settings set;

#endif
