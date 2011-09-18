// WuerfelAnimator *PROFESSIONAL*,
//  a totally useless tool,
//   by Felix Domke
//
//     missing features: - delta-compression (dltrle)
//                       - startframe
//                       - palette optimization(!), not just a "cut-the-first-16-colors", a "search-16-irrelevant-colors-and-remove-them" would be better
//             features: - input format: pcx
//                       - palette "optimization"
//                       - output format: CPANIM 1.0, 2.0
//                       - rle-compressed
//                       - strangeframeoptimization for small & slow anims

// code needs some cleanup, but works quite perfect ;)

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <memory.h>
#include "ptypes.h"

int LoadPCX(FILE *fp, uint1 *outpix, uint1 * pal, int* width, int* height);
void OptimizePalette(char *picture, int bytes, char *srcpal, char *pal);
int GetClosestColor(char *pal, int r, int g, int b);

struct cpaniheaderstruct
{
 char id[8];            // CPANI<1a><0><0>
 char junk[32];
 short wuerfelframes;
 short wuerfelstframes;
 short flags;           // rle=1, dlt=2, 2.0=4
 short junk2;
 short codelenslen;
 short pallen;
};

void main(int argc, char **argv)
{
 printf("WuerfelAnimator *PROFESSIONAL* (c) 1998 by Felix Domke\n");
 if(argc!=3)
 {
  printf("Usage: %s <script> <anim.dat>\n", *argv);
  exit(1);
 }
 FILE *was;
 if(!(was=fopen(argv[1], "rt")))
 {
  perror(argv[1]);
  exit(2);
 }
 FILE *anim;
 if(!(anim=fopen(argv[2], "wb")))
 {
  perror(argv[1]);
  exit(3);
 }
 int wuerfelversion=-1, strangeframeoptimization=0, rlecomp=0;
 fscanf(was, "%d\n", &wuerfelversion);
 if((wuerfelversion<0)||(wuerfelversion>1))
 {
  printf("illegal wurfel-version.\n");
  fclose(was);
  fclose(anim);
  exit(4);
 }

 if(wuerfelversion)
  fscanf(was, "%d\n", &rlecomp);
 else
  fscanf(was, "%d %d\n", &strangeframeoptimization, &rlecomp);
 rlecomp=!!rlecomp;
 char desc[100];
 fgets(desc, 100, was);
 desc[31]=0;
 strlen(desc)&&(desc[strlen(desc)-1]=0);


 int curpic=0, numpics=0, physpics=0;

 while(!feof(was))
 {
  int num=-1, length=-1;
  char name[_MAX_PATH];
  fscanf(was, "%d %d %s\n", &num, &length, name);
  if(num<1)
  {
   printf("section must have at least one pic.\n");
   fclose(was);
   fclose(anim);
   exit(5);
  }
  if(length<1)
  {
   printf("length must be at least one!.\n");
   fclose(was);
   fclose(anim);
   exit(6);
  }
  if(num>1)
  {
   char test1[_MAX_PATH], test2[_MAX_PATH];
   sprintf(test1, name, num);
   sprintf(test2, name, 1);
   if(!strcmp(test1, test2))
   {
    printf("please use some %%ds in your name when more than one frame in section!\n");
    fclose(was);
    fclose(anim);
    exit(7);
   }
  }
  numpics+=wuerfelversion?(num):(num*length);
 }

 if(!numpics)
 {
  printf("length must be at least one!.\n");
  fclose(was);
  fclose(anim);
  exit(8);
 }

 char *cpic=(char*)malloc(wuerfelversion?(320*200):(160*100));
 if(!cpic)
 {
  printf("malloc error.\n");
  fclose(was);
  fclose(anim);
  exit(9);
 }

 cpaniheaderstruct header;
 memcpy(header.id, "CPANI\x1A\x00\x00", 8);
 memset(header.junk, 0, 32);
 strcpy(header.junk, desc);
 header.wuerfelframes=numpics;
 header.wuerfelstframes=0;
 header.junk2=303;
 header.flags=(wuerfelversion?4:0)|(rlecomp?1:0);
 if(wuerfelversion) header.codelenslen=numpics*2; else header.codelenslen=0;
 header.pallen=720;
 fwrite(&header, sizeof(header), 1, anim);
 short *framelens=new short[numpics];
 short *lens;
 if(!framelens)
 {
  printf("malloc error.\n");
  fclose(was);
  fclose(anim);
  exit(10);
 }
 if(wuerfelversion)
 {
  lens=new short[numpics];
  if(!lens)
  {
   printf("malloc error.\n");
   delete[] framelens;
   fclose(was);
   fclose(anim);
   exit(10);
  }
 }

 int framelenspos=ftell(anim);

 fwrite(framelens, numpics, 2, anim);
 if(wuerfelversion) fwrite(lens, numpics, 2, anim);

 char pal[768], upal[768];
 fseek(was, 0, SEEK_SET);
 fgets(desc, 100, was);
 fgets(desc, 100, was);
 rlecomp=!!rlecomp;
 fgets(desc, 100, was);

 while(curpic<numpics)
 {
  int num=-1, length=-1;
  char name[_MAX_PATH];
  fscanf(was, "%d %d %s\n", &num, &length, name);
  for(int i=0; i<num; i++)
  {
   if(wuerfelversion) lens[curpic]=length;
   printf("pic %d/%d\r", curpic, numpics-1);
   fflush(stdout);
   char fname[_MAX_PATH];
   sprintf(fname, name, i);
   FILE *fpcx;
   if(!(fpcx=fopen(fname, "rb")))
   {
    free(cpic);
    if(wuerfelversion) delete[] lens;
    delete[] framelens;
    perror(fname);
    fclose(was);
    fclose(anim);
    exit(11);
   }
   int width, height;
   LoadPCX(fpcx, 0, (uint1*)pal, &width, &height);
   if(!curpic)
   {
    memcpy(upal, pal, 768);
    fwrite(upal+16*3, 240, 3, anim);
   }
   if((width!=(wuerfelversion?320:160))||(height!=(wuerfelversion?200:100)))
   {
    printf("error: %s has wrong size! (%dx%d, %dx%d req.)\n", fname, width, height, wuerfelversion?320:160, wuerfelversion?200:100);
    free(cpic);
    if(wuerfelversion) delete[] lens;
    delete[] framelens;
    fclose(was);
    fclose(anim);
    exit(12);
   }
   if(!LoadPCX(fpcx, (uint1*)cpic, 0, &width, &height))
   {
    printf("LoadPCX(%s); failed!\n", fname);
    free(cpic);
    if(wuerfelversion) delete[] lens;
    delete[] framelens;
    fclose(was);
    fclose(anim);
    exit(13);
   }
   fclose(fpcx);
   OptimizePalette(cpic, wuerfelversion?64000:16000, pal, upal);
   for(int i=0; i<((wuerfelversion||strangeframeoptimization)?1:(length)); i++)
   {
    if(!rlecomp)
    {
     fwrite(cpic, wuerfelversion?64000:16000, 1, anim);
     framelens[curpic]=wuerfelversion?64000:16000;
    } else
    {
     int rlecount=1, rledata=*cpic;
     int bytes=0;
     for(int i=1; i<(wuerfelversion?64000:16000); i++)
     {
      if((cpic[i]!=rledata)||(rlecount==15))
      {
       if(rlecount>=3)
       {
        bytes+=2;
        fputc(rlecount-3, anim);
        fputc(rledata, anim);
       } else
       {
        bytes+=rlecount;
        for(int i=0; i<rlecount; i++) fputc(rledata, anim);
       }
       rledata=cpic[i];
       rlecount=1;
      } else
       rlecount++;
     }
     if(rlecount>=3)
     {
      bytes+=2;
      fputc(rlecount-3, anim);
      fputc(rledata, anim);
     } else
     {
      bytes+=rlecount;
      for(int i=0; i<rlecount; i++) fputc(rledata, anim);
     }
     framelens[curpic]=bytes;
    }

    curpic++;
    physpics++;
   }
   if(strangeframeoptimization) for(int i=1; i<length; i++) framelens[curpic++]=0;
  }
 }
 printf("everything done, seems to be ok!\n");
 printf("stat:\n");
 if(strangeframeoptimization)
 {
  printf("logical frames:                %d\n", numpics);
  printf("physical frames:               %d\n", physpics);
  printf("strangeframeoptimization:      on, saved %dkb\n", (numpics-physpics)*(wuerfelversion?64000:16000)/1024);
 } else
 {
  printf("frames:                        %d\n", numpics);
  if(!wuerfelversion) printf("strangeframeoptimization:      off\n");
 }
 printf("name:                          %s\n", header.junk);
 printf("compression:                   %s%s%s\n", (header.flags&1)?"rle":"", (header.flags&2)?"dlt":"", (header.flags&3)?"":"none");
 printf("size:                          %s\n", wuerfelversion?"320x200":"160x100");
 if(header.flags&1)
 {
  if(strangeframeoptimization)
   printf("uncompressed (phys. only):     %dkb\n", physpics*(wuerfelversion?64000:16000)/1024);
  else
   printf("uncompressed:                  %dkb\n", numpics*(wuerfelversion?64000:16000)/1024);
  printf("compressed:                    %dkb\n", ftell(anim)/1024);
  printf("ratio:                         %d%%\n", (ftell(anim)*100/(physpics*(wuerfelversion?64000:16000))));
 } else
 printf("                               %dkb\n", ftell(anim)/1024);

 fseek(anim, framelenspos, SEEK_SET);
 fwrite(framelens, numpics, 2, anim);
 if(wuerfelversion) fwrite(lens, numpics, 2, anim);
 if(wuerfelversion) delete[] lens;
 delete[] framelens;
 free(cpic);
 fclose(anim);
 fclose(was);
}

/*

wuerfelscript:

<wuerfelversion>                                        ; 0 for old, 1 for new
<strangeframeoptimization>                              ; strangeframeoptimization
<title>                                                 ; max. 31 bytes
<num> <wuerfelversion?framelength:times> pic%03d.pcx    ; num from this section, framlength in ms, times in strange-sonstwas-scheisse.
...

wurfelformat:

struct
{
 char id[8];    // CPANI<1a><0><0>
 char junk[32];
 short wuerfelframes;
 short wuerfelstframes;
 short flags;   // rle=1, dlt=2
 short junk2;
 short codelenslen;
 short pallen;
 short framelens[wuerfelframes+wuerfelstframes];
 char  codelens[codelenslen];
 char  pal[pallen];

 ... wuerfel ...
}


*/

typedef struct {
    uint1 id;
    uint1 version;
    uint1 rle;
    uint1 bpp;
    short xstart;
    short ystart;
    short xend;
    short yend;
    short hres;
    short vres;
    uint1 pal[48];
    uint1 rsvd1;
    uint1 nbitp;
    short uint1sperline;
    short paltype;
    short hsize;
    short vsize;
    uint1 rsvd2[54];
} PCXHeader;

// a really UGLY pcx-reader, ripped somewhere, somewhen.
// i don't know where, and i don't want to know where it was...
// just one thing about this loader: it works.

int LoadPCX(FILE *fp, uint1 *outpix, uint1 * pal, int* width, int* height)
{
 int i, w, h;
 PCXHeader hdr;
 long pos;
 int ret=0;

 pos=ftell(fp);
 fseek(fp, 0, SEEK_END);
 int filesize=ftell(fp);
 fseek(fp, 0, SEEK_SET);
 fread(&hdr, 1, sizeof(hdr), fp);
 if((hdr.id!=0x0A)||(hdr.version!=5)||(hdr.rle!=1)||(hdr.bpp==8&&hdr.nbitp!=1))
  goto bye;

 w=hdr.xend-hdr.xstart+1;
 if(width)
  *width=w;
 h=hdr.yend-hdr.ystart+1;
 if(height)
  *height=h;
 if(pal)
 {
  if((hdr.bpp==1)&&(hdr.nbitp==4))
  {
   memset(pal, 0, 768);
   for(i=0; i<48; i++)
    pal[i]=hdr.pal[i]>>2;
  } else if((hdr.bpp==8)&&(hdr.nbitp==1))
  {
   fseek(fp, pos + filesize-768, SEEK_SET);
   for(i=0; i<768; i++)
    pal[i]=((uint1)fgetc(fp))>>2;
  } else
   goto bye;
 }
 ret=1;
 if(!outpix)
  goto bye;
 fseek(fp, pos+sizeof(PCXHeader), SEEK_SET);

 while(h-->0)
 {
  uint1 c;
  uint1 *outpt;
  int np=0;

  outpt=outpix;
  memset(outpix, 0, w);
  for(np=0; np<hdr.nbitp; np++)
  {
   i=0;
   outpix=outpt;
   do
   {
    c=(char)fgetc(fp);
    if(((c&0xC0)!=0xC0)||(!hdr.rle))
    {
     if(hdr.bpp==1)
     {
      int k;
      for(k=7; k>=0; k--)
       *outpix++|=((c>>k)&1)<<np;
      i+=8;
     } else
     {
      *outpix++=c;
      i++;
     }
    } else
    {
     uint1 v;
     v=(char)fgetc(fp);
     c&=~0xC0;
     while(c>0&&i<w)
     {
      if(hdr.bpp==1)
      {
       int k;
       for(k=7; k>=0; k--)
       *outpix++|=((v>>k)&1)<<np;
       i+=8;
      } else
      {
       *outpix++=v;
       i++;
      }
      c--;
     }
    }
   } while (i < w);
  }
 }

 bye:
fseek(fp, 0, SEEK_SET);
 return ret;
}

void OptimizePalette(char *picture, int bytes, char *srcpal, char *pal)
{
 char newpal[256];
 for(int col=0; col<256; col++)
 {
  newpal[col]=GetClosestColor(pal, srcpal[col*3], srcpal[col*3+1], srcpal[col*3+2]);
 }
 for(int i=0; i<bytes; i++) picture[i]=newpal[picture[i]];
}

int GetClosestColor(char *pal, int r, int g, int b)
{
 int cc=16, ccd=0x7FFFFFFF, d;
 for(int i=16; i<256; i++)
 {
  d=(abs(pal[i*3]-r)+abs(pal[i*3+1]-g)+abs(pal[i*3+2]-b));
  if(d<ccd) { cc=i; ccd=d; }
 }
 return(cc);
}
