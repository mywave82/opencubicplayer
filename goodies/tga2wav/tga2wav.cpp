// TGA 2 WAV Converter
// written sometime in 1995 by Dirk Jagdmann <doj@cubic.org>
// then patched by Niklas Beisert to support different phases
// then lost
// and found again in 2002 by Dirk Jagdmann

#ifdef __WATCOMC__
#include <io.h>
#include <iostream.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace std;
#define _MAX_PATH PATH_MAX
#endif

#ifdef __unix__
#include <unistd.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(_WIN32) || __GNUC__ >= 3 || defined(__INTEL_COMPILER) || defined(__WATCOMC__)
#define P
#pragma pack(push, 1)
#else
#define P __attribute__ ((packed))
#endif

struct WAVfmt
{
  char name[4];                 /* "fmt " */
  unsigned long length;         /* 0x10 */
  unsigned short type;          /* 1=WAVE_FORMAT_PCM see below for more WAVE_FORMAT_... */
  unsigned short channels;      /* commonly 1,2 but can be >2 */
  unsigned long rate;
  unsigned long bytesPerSecond; /* rate*bytesPerSample */
  union {
    unsigned short blockAlign;
    unsigned short bytesPerSample; /* channels*(bitsPerSample/8), but bitsPerSample/8 has to be rounded up! */
  };
  unsigned short bitsPerSample; /* 8,12,16,24 */
} P;

struct WAVdata
{
  char name[4];                 /* "data" */
  unsigned long length;         /* length of following sample stream = WAV.length - 0x24 = filelength - 8 - 0x24 */
} P;

struct WAV
{
  char name[4];                 /* "RIFF" */
  unsigned long length;         /* filelength - 8 */
  char type[4];                 /* "WAVE" */
  struct WAVfmt fmt;
  struct WAVdata data;
} P;

struct TGA
{
  unsigned char idlen;          // 0

  unsigned char cmtype;         // 1

  unsigned char type;           // 2

  unsigned short cmstart;       // 3
  unsigned short cmlen;         // 5
  unsigned char cmsize;         // 7

  unsigned short imgx;          // 8
  unsigned short imgy;          // 10
  unsigned short width;         // 12
  unsigned short height;        // 14
  unsigned char imgbits;        // 15

  unsigned char attrbits :4;    // 16.0
  unsigned char reserved :1;    // 16.4
  unsigned char origin :1 ;     // 16.5
  unsigned char interleave :2;  // 16.6
} P;

#if defined(_WIN32) || __GNUC__ >= 3 || defined(__INTEL_COMPILER) || defined(__WATCOMC__)
#pragma pack(pop)
#endif

#undef P

FILE *infile=0, *outfile=0;
char ofiles[_MAX_PATH+1];
bool cleanexit=false;
unsigned char *bild=0;
bool bildSwap=false;
TGA tga;

unsigned char pixel(int x, int y)
{
  if(bildSwap)
    y=tga.height-1-y;
  return bild[y*tga.width+x];
}

void cleanup()
{
  if(infile)
    fclose(infile);
  if(outfile)
    fclose(outfile);
  if(!cleanexit)
    unlink(ofiles);
  if(bild)
    delete [] bild;
  cout << endl;
}


int main(int argc, char **argv)
{
  int samplerate=44100;
  int len=256;
  int i;
  double amplify=0.0;
  char ifiles[_MAX_PATH+1];

  atexit(cleanup);

  if (argc>2)
    {
      i=atoi(argv[2]);
      if(i>0)
	{
	  amplify=i;
	  goto jumpparam;
	}
      strcpy(ifiles,argv[1]);
      strcpy(ofiles,argv[2]);
    }
  if (argc==2)
    {
    jumpparam:
      strcpy(ifiles,argv[1]);
      strcat(ifiles,".tga");
      strcpy(ofiles,argv[1]);
      strcat(ofiles,".wav");
    }
  if (argc==1)
    {
      cout << "filename without suffix: ";
      char file[_MAX_PATH+1];
      cin >> file;
      strcpy(ifiles,file);
      strcat(ifiles,".tga");
      strcpy(ofiles,file);
      strcat(ofiles,".wav");
    }

  if(amplify==0.0)
    {
      cout << "Amplify: ";
      cin >> amplify;
    }
  if (amplify<1.0)
    {
      cerr << "amplification too low!" << endl;
      return 1;
    }

  FILE *infile=fopen(ifiles, "rb");
  if(infile==0)
    {
      cerr << "could not read " << ifiles << endl;
      return 1;
    }

  fread(&tga, sizeof(tga), 1, infile);

  if(tga.type!=1)
    {
      cerr << "tga file is not of uncompressed palette type" << endl;
      return 1;
    }

  bildSwap=!tga.origin;
  cout << "height: " << tga.height << endl;
  cout << "width: " << tga.width << endl;
  const int wavgros=tga.width*len * 2;
  cout << "size of wav file: " << wavgros+0x24+8 << endl;

  const int groesse=tga.width*tga.height;
  bild=new unsigned char [groesse];
  if(!bild)
    {
      cerr << "could not alloc memory. Perhaps the picture is too large?" << endl;
      return 1;
    }

  unsigned char palette[256];
  for (i=0; i<256; i++)
    {
      unsigned char blau,grun,rot;
      fread(&blau,1,1,infile);
      fread(&grun,1,1,infile);
      fread(&rot,1,1,infile);
      palette[i]=(blau+grun+rot)/3;
    }

  fread(bild,groesse, 1, infile);

  FILE *outfile=fopen(ofiles, "wb");

  WAV w = {
    {'R','I','F','F'},
    0x24 + wavgros,
    {'W','A','V','E'},
    {
      {'f','m','t',' '},
      0x10,
      1,
      1,
      samplerate,
      samplerate*2,
      {2},
      16,
    },
    {
      {'d','a','t','a'},
      wavgros,
    },
  };

  fwrite(&w, sizeof(w), 1, outfile);

  double *sample=new double[len];
  if(sample==0)
    {
      cerr << "could not alloc memory" << endl;
      return 1;
    }

  int offset=0;
  int oldperc=0;
  for (int x=0; x<tga.width; x++, offset+=len)
    {
      if(x*100/tga.width > oldperc)
	{
	  oldperc=x*100/tga.width;
	  cout << "\r" << oldperc << "%";
	  fflush(stdout);
	}

      for (i=0; i<len; i++)
	sample[i]=0;

      for (int y=0; y<tga.height; y++)
      {
	const int p=palette[pixel(x,y)];
	if(p==0)
	  continue;
	const double scaler=double(y)/double(tga.height);
	const double factor=amplify * p /* /(4.0*sqrt(double(y))) */ ;
	for(i=0; i<len; i++)
	  sample[i]+=factor * sin(scaler*(i+offset+y*2));
      }

      for (i=0; i<len; i++)
	{
	  signed short a=short(sample[i]);
	  if(sample[i]>32700.0)
	    a=32700;
	  else if(sample[i]<-32700.0)
	    a=-32700;
	  fwrite(&a, sizeof(a), 1, outfile);
	}
    }
  delete [] sample;

  cleanexit=true;
  return 0;
}
