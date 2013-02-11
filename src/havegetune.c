/**
 ** Determine HAVEGE environment
 **
 ** Copyright 2009-2012 Gary Wuertz gary@issiweb.com
 ** Copyright 2011-2012 BenEleventh Consulting manolson@beneleventh.com
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include "havege.h"
#include "havegecollect.h"
/**
 * Local debugging
 */
#if 0
#define TUNE_DEBUG(...) fprintf(stderr,__VA_ARGS__)
#define TUNE_DUMP 1
static void   cfg_dump(HOST_CFG *anchor);
#else
#define TUNE_DEBUG(...)
#endif

#define VFS 1
/**
 * The configuration allocation
 */
HOST_CFG  theConfig;
/**
 * Local prototypes
 */
static void   cfg_cacheAdd(HOST_CFG *anchor, U_CHAR src, U_CHAR cpu,
                U_CHAR level, U_CHAR  type, U_INT kb);
static void   cfg_cpuAdd(HOST_CFG *anchor, U_CHAR src, CPU_INST *INST);


#ifdef CPUID
/************************* CPUID support ***************************************/
/**
 * Register names
 */
typedef enum {
  EAX, EBX, ECX, EDX
} CPUID_REGNAMES;

#define CPUID_CONFIG(a)   cpuid_config(a)
#define CPUID_VENDOR(r)   *((U_INT *)s) = regs[r];s+= sizeof(U_INT) 
/**
 * Local CPUID prototypes
 */
#if defined (GCC_VERSION) && GCC_VERSION >= 40400
static void   cpuid(int fn, int sfn, U_INT *regs) __attribute__((optimize(0)));
#else
static void   cpuid(int fn, int sfn, U_INT *regs);
#endif
static void   cpuid_config(HOST_CFG *anchor);
static void   cpuid_configAmd(HOST_CFG *anchor, CPU_INST *w);
static void   cpuid_configIntel(HOST_CFG *anchor, CPU_INST *w);
static void   cpuid_configIntel2(HOST_CFG *anchor, CPU_INST *w, U_INT *regs);
static void   cpuid_configIntel4(HOST_CFG *anchor, CPU_INST *w, U_INT *regs);
#else
#define CPUID_CONFIG(a)
#endif
/************************* CPUID support ***************************************/
/*************************  VFS support  ***************************************/
#ifdef VFS

#define  VFS_LINESIZE   256
/**
 * Filter function used by configuration
 */
typedef int (*pFilter)(HOST_CFG *pAnchor, char *input);
typedef int (*pDirFilter)(HOST_CFG *pAnchor, char *input, U_INT *pArg);
/**
 * Definitions
 */
#define VFS_CONFIG(a)   vfs_config(a)
/**
 * Local filesystem prototypes
 */
static void   vfs_config(HOST_CFG *anchor);
static int    vfs_configCpuDir(HOST_CFG *anchor, char *input, U_INT *pArg);
static int    vfs_configCpuInfo(HOST_CFG *anchor, char *input);
static int    vfs_configDir(HOST_CFG *pAnchor, char *path, pDirFilter filter, U_INT *pArg);
static int    vfs_configFile(HOST_CFG *anchor, char *path, pFilter filter);
static int    vfs_configInfoCache(HOST_CFG *pAnchor, char *input, U_INT *pArg);
static int    vfs_configOnline(HOST_CFG *anchor, char *input);
static int    vfs_configInt(HOST_CFG *anchor, char *input);
static int    vfs_configStatus(HOST_CFG *anchor, char *input);
static int    vfs_configType(HOST_CFG *anchor, char *input);

static void   vfs_parseList(TOPO_MAP *map, char *input);
static void   vfs_parseMask(TOPO_MAP *map, char *input);
#else
#define VFS_CONFIG(a)
#endif
/*************************  VFS support  ***************************************/

/**
 * Get tuning values for collector
 */
HOST_CFG *havege_tune(     /* RETURN: the configuration  */
  CMD_PARAMS *param)       /* IN: config parameters      */
{
   HOST_CFG *anchor = &theConfig;
   int i;
 
   memset(anchor, 0, sizeof(HOST_CFG));
   anchor->procfs = param->procFs==NULL? "/proc" : param->procFs;
   anchor->sysfs  = param->sysFs==NULL? "/sys"   : param->sysFs;
   /**
    * The order determines preference
    */
   if (param->icacheSize != 0)
      cfg_cacheAdd(anchor, SRC_PARAM, -1, 1, 'I', param->icacheSize);
   if (param->dcacheSize != 0)
      cfg_cacheAdd(anchor, SRC_PARAM, -1, 1, 'D', param->dcacheSize);
   /**
    * Bypass configuration if entirely specified
    */
   if (param->icacheSize == 0 || param->dcacheSize == 0) {
      CPUID_CONFIG(anchor);
      VFS_CONFIG(anchor);
      cfg_cacheAdd(anchor, SRC_DEFAULT, -1, 1, 'I', GENERIC_ICACHE);
      cfg_cacheAdd(anchor, SRC_DEFAULT, -1, 1, 'D', GENERIC_DCACHE);
      }
   /**
    * Make sure there is at least 1 cpu instance
    */
   if (0 == anchor->ctCpu)
      cfg_cpuAdd(anchor, 0, NULL);
 #ifdef TUNE_DUMP
   cfg_dump(anchor);
 #endif
   anchor->d_tune = anchor->i_tune = MAX_CACHES+2;
   for (i=0;i<anchor->ctCache;i++) {
      if (anchor->caches[i].level==1) {
            switch(anchor->caches[i].type) {
              case 'I':   case 'T':
                  if (i < anchor->i_tune)
                     anchor->i_tune = i;
                  break;
              case 'D':
                  if (i < anchor->d_tune)
                     anchor->d_tune = i;
                  break;
              }
         }
      }
   TUNE_DEBUG("havege_tune %d/%d\n", anchor->i_tune, anchor->d_tune);
   return anchor;
}

/**
 * Return number of bits set in map
 */
void cfg_bitClear(         /* RETURN : None  */
  TOPO_MAP *m)             /* IN: bitmap     */
{
   memset(&m->bits[0], 0, MAX_BIT_IDX * sizeof(U_INT));
}
/**
 * Return number of bits set in map
 */
int cfg_bitCount(          /* RETURN : None  */
  TOPO_MAP *m)             /* IN: bitmap     */
{
   int n, ct=0;
   
   for(n=-1;(n=cfg_bitNext(m,n))!=-1;ct++) ;
   return ct;
}
/**
 * Display topo bit map - cpuset(7) convention is big-endian
 */
void cfg_bitDisplay(       /* RETURN : None  */
   TOPO_MAP *m)            /* IN: bitmap     */
{
   int n;
 
   for(n=m->msw;n>=0 && n < MAX_BIT_IDX;n--)
      printf(" %08x", m->bits[n]);
}
/**
 * Test if maps intersect
 */
int cfg_bitIntersect(      /* RETURN: None   */
  TOPO_MAP *m,             /* OUT: bitmap    */
  TOPO_MAP *t)             /* IN: bit to set */
{
   int i;
 
   for (i=0;i < MAX_BIT_IDX;i++)
      if (0!=(m->bits[i] & t->bits[i]))
         return 1;
   return 0;
}
/**
 * Test if maps intersect
 */
void cfg_bitMerge(         /* RETURN: None      */
  TOPO_MAP *m,             /* OUT: bitmap       */
  TOPO_MAP *t)             /* IN: bits to set   */
{
   int i;
   
   for (i=0;i<MAX_BIT_IDX;i++) {
      m->bits[i] |= t->bits[i];
      if (0 != m->bits[i] && i > m->msw)
         m->msw = i;
      }
}
/**
 * Find next bit in topo bit map
 */
int cfg_bitNext (          /* RETURN: index of next bit or -1  */
  TOPO_MAP *m,             /* IN: bitmap                       */
  int n)                   /* IN: prev bit  use -1 for first   */
{
   int bit, word;
 
   bit  = (n+1) % 32;
   for(word = (n+1) / 32;word <= m->msw && word < MAX_BIT_IDX;word++) {
      for(;bit<32; bit++)
         if (m->bits[word] & 1<< bit)
            return word * 32 + bit;
      bit = 0;
      }
   return -1;
}
/**
 * Set a bit in the topo bit map
 */
void cfg_bitSet(           /* RETURN: None   */
  TOPO_MAP *m,             /* OUT: bitmap    */
  int n)                   /* IN: bit to set */
{
   int word;
 
   word = n / 32;
   if (word < MAX_BIT_IDX) {
      if (word > m->msw)
         m->msw = word;
      m->bits[word] |= 1 << (n % 32);
      }
}
/**
 * Add cache description to configuration
 */
static void cfg_cacheAdd(  /* RETURN: None            */
  HOST_CFG *anchor,        /* IN-OUT: configuration   */
  U_CHAR src,              /* IN: source              */
  U_CHAR cpu,              /* IN: use -1 for all      */
  U_CHAR level,            /* IN: cache level         */
  U_CHAR  type,            /* IN: cache type          */
  U_INT kb)                /* IN: cache size in kb    */
{
   int i;
 
   TUNE_DEBUG("cacheAdd(%x, %d,%d,%d,%c)\n", src, cpu, level, kb, type);
   if (3 < level || 0 ==kb) return;
   for(i = 0;i < anchor->ctCache;i++)
      if (anchor->caches[i].level == level &&
          anchor->caches[i].type  == type &&
          anchor->caches[i].size  == kb)
            break;
   if (i >= MAX_CACHES) return;
   if (-1 == cpu)
      cfg_bitMerge(&anchor->caches[i].cpuMap, &anchor->pCacheInfo);
   else  cfg_bitSet(&anchor->caches[i].cpuMap, cpu);
   anchor->caches[i].cpuMap.source |= src;
   if (i < anchor->ctCache) return;
   anchor->caches[i].level = level;
   anchor->caches[i].type  = type;
   anchor->caches[i].size  = kb;
   anchor->ctCache += 1;
}
/**
 * Add cpu description to configuration
 */
static void cfg_cpuAdd(    /* RETURN: None            */
  HOST_CFG *anchor,        /* IN-OUT: configuration   */
  U_CHAR src,              /* IN: source              */
  CPU_INST *inst)          /* IN: instance            */
{
   int i=0;
   
   TUNE_DEBUG("cpuAdd(%x)\n", src);
   if (NULL==inst) {
      cfg_bitSet(&anchor->cpus[i].cpuMap, 0);
      anchor->ctCpu = 1;
      return;
      }
   for(i=0;i < anchor->ctCpu;i++)
      if (0 != cfg_bitIntersect(&anchor->cpus[i].cpuMap, &inst->cpuMap)) {
         cfg_bitMerge(&anchor->cpus[i].cpuMap, &inst->cpuMap);
         anchor->cpus[i].cpuMap.source |= src;
         return;
         }
   if (i >= MAX_CPUS)  return;
   memcpy(&anchor->cpus[i], inst, sizeof(CPU_INST));
   anchor->cpus[i].cpuMap.source = src;
   anchor->ctCpu += 1;
}
#ifdef TUNE_DUMP
/**
 * Diagnostic dump
 */
static void cfg_dump(      /* RETURN: None      */
  HOST_CFG *anchor)        /* IN: configuration */
{
  int i;

  for(i=0;i < anchor->ctCpu;i++)
      printf("Cpu: %08x\n",
         anchor->caches[i].cpuMap.source
         );
  for(i = 0;i < anchor->ctCache;i++)
      printf("Cache: %08x %d, %c, %d\n",
         anchor->caches[i].cpuMap.source,
         anchor->caches[i].level,
         anchor->caches[i].type,
         anchor->caches[i].size
         );
}
#endif

/************************* CPUID support ***************************************/
/**
 * CPUID support
 */
#ifdef CPUID
/**
 * Wrapper around the cpuid macro to assist in debugging
 */
static void cpuid(         /* RETURN: none               */
   int fn,                 /* IN: function code          */
   int sfn,                /* IN: subfunction            */
   U_INT *regs)            /* IN-OUT: Workspace          */
{
   regs[2] = sfn;
   CPUID(fn, regs);
}
/**
 * Get configuration
 */
static void cpuid_config(  /* RETURN: none            */
   HOST_CFG *anchor)       /* IN-OUT: result          */
{
   CPU_INST    wsp;
   U_INT       regs[4];
   char        *s;
 
   if (HASCPUID(regs)) {
      memset(&wsp, 0, sizeof(CPU_INST));
      wsp.flags |= SRC_CPUID_PRESENT;
      cpuid(0x00,0,regs);
      wsp.maxFn  = regs[EAX];
      s = wsp.vendor;
      CPUID_VENDOR(EBX);
      CPUID_VENDOR(EDX);
      CPUID_VENDOR(ECX);
      if (regs[EBX] == 0x68747541)
         wsp.flags |= SRC_CPUID_AMD;
      (void)cpuid(0x80000000,0,regs);
      wsp.maxFnx = regs[EAX];
      (void)cpuid(0x01,0,regs);
      wsp.signature = regs[EAX];
      if ((regs[EDX] & (1<<28)) != 0)
         wsp.flags |= SRC_CPUID_HT;
      TUNE_DEBUG("cpuid_config %s fn=%d fnx=%x flags=%x\n", (char *)wsp.vendor,
         wsp.maxFn, wsp.maxFnx, wsp.flags);
      if (0!=(wsp.flags & SRC_CPUID_AMD))
         cpuid_configAmd(anchor, &wsp);
      else cpuid_configIntel(anchor, &wsp);
   }
}
/**
 * AMD cpuid Configuration. Reference: Publication 25481, Revision 2.34, Sept 2010
 */
static void cpuid_configAmd(
   HOST_CFG *anchor,       /* IN-OUT: result             */
   CPU_INST *w)            /* IN-OUT: Workspace          */
{
   U_INT regs[4];
   int  i, n;

   switch((w->maxFnx&15)) {
      case 8:
         cpuid(0x80000008,0,regs);
         n = 1 + (regs[ECX] & 0xff);
         for(i=0;i<n;i++)
            cfg_bitSet(&w->cpuMap, i);
         cfg_cpuAdd(anchor, SRC_CPUID_AMD8, w);
      case 6:
         cpuid(0x80000006,0,regs);
         cfg_cacheAdd(anchor, SRC_CPUID_AMD6, -1, 2, 'U', (regs[ECX]>>16) & 0xffff);
         cfg_cacheAdd(anchor, SRC_CPUID_AMD6, -1, 3, 'U', ((regs[EDX]>>18) & 0x3fff)<<9);
      case 5:
         cpuid(0x80000005,0,regs);
         cfg_cacheAdd(anchor, SRC_CPUID_AMD5, -1, 1, 'D', (regs[ECX]>>24) & 0xff);
         cfg_cacheAdd(anchor, SRC_CPUID_AMD5, -1, 1, 'I', (regs[EDX]>>24) & 0xff);
         break;
        }
}
/**
 * Intel cpuid configuration. Reference: Publication 241618
 * Processor Identification and CPUID Instruction
 * Application node 485, Jan 2011
 */
static void cpuid_configIntel(
   HOST_CFG *anchor,       /* IN-OUT: result             */
   CPU_INST *w)            /* IN-OUT: Workspace          */
{
   U_INT  regs[4];
 
   if (w->maxFn >=0x0b) {
      regs[ECX] = 0;
      cpuid(0x0b,0,regs);
      if (regs[EBX]!=0)
         w->flags |= SRC_CPUID_LEAFB;
     }
   if (w->flags & SRC_CPUID_HT)
      ;
   else
      ;
   if (w->maxFn >=4)
      cpuid_configIntel4(anchor, w, regs);
   if (w->maxFn >= 2)
      cpuid_configIntel2(anchor, w, regs);
}
/**
 * Configure caches using cpuid leaf 2. This is a legacy method that only determines
 * level 1 cache. Still needed because trace cache is not reported elsewhere.
 */
static void cpuid_configIntel2(
  HOST_CFG *anchor,       /* IN-OUT: result             */
  CPU_INST *w,              /* IN-OUT: Workspace          */
  U_INT *regs)              /* IN-OUT: registers          */
{
   /* L1 and Trace as per Intel application note 485, January 2011 */
   static const U_CHAR defs[] = {
     0x06, 'I',  8 , /* 4-way set assoc, 32 byte line size                 */
     0x08, 'I', 16 , /* 4-way set assoc, 32 byte line size                 */
     0x09, 'I', 32 , /* 4-way set assoc, 64 byte line size +               */
     0x0a, 'D',  8 , /* 2 way set assoc, 32 byte line size                 */
     0x0c, 'D', 16 , /* 4-way set assoc, 32 byte line size                 */
     0x0d, 'D', 16 , /* 4-way set assoc, 64 byte line size +               */
     0x0e, 'D', 24 , /* 6-way set assoc, 64 byte line size                 */
     0x10, 'D', 16 , /* 4-way set assoc, 64 byte line size                 */
     0x15, 'I', 16 , /* 4-way set assoc, 64 byte line size                 */
     0x2c, 'D', 32 , /* 8-way set assoc, 64 byte line size                 */
     0x30, 'I', 32 , /* 8-way set assoc, 64 byte line size                 */
     0x60, 'D', 16 , /* 8-way set assoc, sectored cache, 64 byte line size */
     0x66, 'D',  8 , /* 4-way set assoc, sectored cache, 64 byte line size */
     0x67, 'D', 16 , /* 4-way set assoc, sectored cache, 64 byte line size */
     0x68, 'D', 32 , /* 4-way set assoc, sectored cache, 64 byte line size */
     0x70, 'T', 12 , /* 8-way set assoc, trace cache                       */
     0x71, 'T', 16 , /* 8-way set assoc, trace cache                       */
     0x72, 'T', 32 , /* 8-way set assoc, trace cache                       */
     0x73, 'T', 64 , /* 8-way set assoc, trace cache                       */
     0x00, 0,  0     /* sentinel                                           */
     };
   U_INT   i, j, n, m;
 
   cpuid(0x02,0,regs);
   n = regs[EAX]&0xff;
   while(n--) {
      for (i=0;i<4;i++) {
         if (0==(regs[i] & 0x80000000)) {
            while(0 != regs[i]) {
               m = regs[i] & 0xff;
               if (m==0xff)
                  w->flags |= SRC_CPUID_LEAF4;
               else for (j=0;0 != defs[j];j += 3)
                  if (defs[j]==m) {
                     cfg_cacheAdd(anchor, SRC_CPUID_INTEL2, -1, 1, defs[j+1], defs[j+2]);
                     break;
                     }
               regs[i]>>=8;
               }
            }
         }
      if (n) cpuid(0x02,0,regs);
      }
}
/**
 * Configure caches using cpuid leaf 4
 */
static void cpuid_configIntel4(
   HOST_CFG *anchor,       /* IN-OUT: result             */
   CPU_INST *w,            /* IN-OUT: Workspace          */
   U_INT *regs)            /* IN-OUT: registers          */
{
   U_CHAR  level, type;
   U_INT   i, j, lineSz, nParts, nWays, sz;
 
   for(i=0;i<MAX_CACHES;i++) {
      cpuid(0x04,i,regs);
      if (0==i) {
         int n = 1 + (regs[EAX]>>26);
         for(j=0;j<n;j++)
            cfg_bitSet(&w->cpuMap, j);
         cfg_cpuAdd(anchor, SRC_CPUID_INTEL4, w);
        }
      switch(regs[EAX] & 31) {
         case 0:  type =  0 ; break;
         case 1:  type = 'D'; break;
         case 2:  type = 'I'; break;
         case 3:  type = 'U'; break;
         default: type = '?'; break;
         }
      if (0==type) break;
      regs[EAX] >>= 5;
      level = (U_CHAR)(regs[EAX] & 7);
      lineSz = 1 + (regs[EBX] & 0xfff);
      regs[EBX] >>= 12;
      nParts  = 1 + (regs[EBX] & 0x3ff);
      regs[EBX] >>= 10;
      nWays   = 1 + regs[EBX];
      sz = (nWays * nParts * lineSz * (regs[ECX]+1)) / 1024;
      cfg_cacheAdd(anchor, SRC_CPUID_INTEL4, -1, level, type, sz);
      }
}
#endif
/************************* CPUID support ***************************************/
/*************************  VFS support  ***************************************/
#ifdef VFS
/**
 * Get configuration
 */
static void vfs_config(
   HOST_CFG *anchor)       /* IN-OUT: result          */
{
   char      path[FILENAME_MAX];
   CPU_INST  inst;
   U_INT     args[2];
   int       n;
 
   args[0] = args[1] = 0;
   snprintf(path, FILENAME_MAX, "%s/self/status", anchor->procfs);
   if (-1 != vfs_configFile(anchor, path, vfs_configStatus))
      args[0] |= SRC_VFS_STATUS;
   snprintf(path, FILENAME_MAX, "%s/devices/system/cpu/online", anchor->sysfs);
   if (-1 != vfs_configFile(anchor, path, vfs_configOnline))
      args[0] |= SRC_VFS_ONLINE;
   snprintf(path, FILENAME_MAX, "%s/cpuinfo", anchor->procfs);
   if (-1 != vfs_configFile(anchor, path, vfs_configCpuInfo))
      args[0] |= SRC_VFS_CPUINFO;
   snprintf(path, FILENAME_MAX, "%s/devices/system/cpu", anchor->sysfs);
   if (-1 != vfs_configDir(anchor, path, vfs_configCpuDir, args)) {
      memset(&inst, 0, sizeof(CPU_INST));
      args[0] |= SRC_VFS_CPUDIR;
      for(n=-1;(n=cfg_bitNext(&anchor->pCpuInfo,n))!=-1;)
         cfg_bitSet(&inst.cpuMap, n);
      if (cfg_bitCount(&inst.cpuMap)>0)
         cfg_cpuAdd(anchor, SRC_VFS_CPUINFO, &inst);
     }
   for(n=-1;(n=cfg_bitNext(&anchor->pCacheInfo,n))!=-1;) {
      snprintf(path, FILENAME_MAX, "%s/devices/system/cpu/cpu%d/cache", anchor->sysfs, n);
      args[1] = n;
      vfs_configDir(anchor, path, vfs_configInfoCache, args);
      }
}
/**
 * Call back to get cpus and from cpu subdirectories
 */
static int vfs_configCpuDir(
  HOST_CFG *anchor,        /* IN-OUT: result          */
  char *input,             /* filename                */
  U_INT *pArg)             /* parameter               */
{
   char  term[32];
   int   cpu;
   
   if (strlen(input)> 3) {
      cpu = atoi(input+3);
      (void)snprintf(term, 32, "cpu%d", cpu);
       if (!strcmp(term, input))
         cfg_bitSet(&anchor->pCacheInfo, cpu);
      }
   return 0;
}
/**
 * Call back to get cpus from cpuinfo
 */
static int vfs_configCpuInfo(
  HOST_CFG *anchor,        /* IN-OUT: result          */
  char *input)             /* input text              */
{
   char    key[32], value[32], *s;
 
   s = strchr(input, ':');
   if (NULL != s) {
      *s++ = '\0';
      if (1==sscanf(input, "%32s", key) && 1==sscanf(s, "%32s", value)) {
         if (!strcmp("processor",key))
            cfg_bitSet(&anchor->pCpuInfo, atoi(value));
         }
      } 
   return 0;
}
/**
 * Process a configuration directory
 */
static int vfs_configDir(
  HOST_CFG *pAnchor,       /* IN-OUT: result          */
  char *path,              /* IN: directory path      */
  pDirFilter filter,       /* IN: entry filter        */
  U_INT *pArg)             /* IN: filter arg          */
{
   DIR              *d;
   struct dirent    *ent;
   int              rv=-1;
 
   if (NULL != (d = opendir(path))) {
      while (NULL!=(ent = readdir(d))) {
         if (0!=(rv = (*filter)(pAnchor, ent->d_name, pArg)))
            break;
         }
      (void)closedir(d);
      }
   return rv;
}
/**
 * Process a configuration file
 */
static int vfs_configFile(
  HOST_CFG *pAnchor,       /* IN-OUT: result          */
  char *path,              /* IN: file path           */
  pFilter filter)          /* IN: input filter        */
{
   char buf[VFS_LINESIZE];
   FILE *f;
   int rv=-1;
   
   if (NULL != (f = fopen(path, "rb"))) {
      while(fgets(buf, VFS_LINESIZE, f))
         if (0!=(rv = (*filter)(pAnchor, buf)))
            break;
      fclose(f);
      }
   return rv;
}
/**
 * Call back to get cache details
 */
static int vfs_configInfoCache(
  HOST_CFG *pAnchor,       /* IN-OUT: result          */
  char *input,             /* IN: path name           */
  U_INT *pArgs)
{
   char    path[FILENAME_MAX];
   int     idx , plen, ctype, level, size;
 
   if (strlen(input)> 5) {
     idx = atoi(input+5);
     (void)snprintf(path, 32, "index%d", idx);
      if (!strcmp(path, input)) {
         plen = snprintf(path, FILENAME_MAX, "%s/devices/system/cpu/cpu%d/cache/index%d/level",
            pAnchor->sysfs, pArgs[1], idx) - 5;
         level = vfs_configFile(pAnchor, path, vfs_configInt);
         strcpy(path+plen, "type");
         ctype = vfs_configFile(pAnchor, path, vfs_configType);
         strcpy(path+plen, "size");
         size  = vfs_configFile(pAnchor, path, vfs_configInt);
         cfg_cacheAdd(pAnchor, SRC_VFS_INDEX,  pArgs[1], level, ctype, size);
         }
     }
  return 0;
}
/**
 * Call back to get cpus from online file
 */
static int vfs_configOnline(
  HOST_CFG *anchor,        /* IN-OUT:  result   */
  char *input)             /* IN: input text    */
{
  vfs_parseList(&anchor->pOnline, input);
  return 1;
}
/**
 * Call back to return a single integer input
 */
static int vfs_configInt(
  HOST_CFG *anchor,        /* IN-OUT:  result   */
  char *input)             /* IN: input text    */
{
   return atoi(input);
}
/**
 * Call back to get cpus and memory from status file
 */
static int vfs_configStatus(
  HOST_CFG *anchor,        /* IN-OUT:  result   */
  char *input)             /* IN: input text    */
{
   char key[32], value[VFS_LINESIZE-32], *s;
 
   s = strchr(input, ':');
   if (NULL != s) {
      *s++ = '\0';
      if (1==sscanf(input, "%s", key) && 1==sscanf(s, "%s", value)) {
         if (!strcmp("Cpus_allowed", key))
            vfs_parseMask(&anchor->pAllowed, value);
         else if (!strcmp("Mems_allowed", key))
            vfs_parseMask(&anchor->mAllowed, value);
         }
      }
   return 0;
}
/**
 * Call back to return the first byte of input
 */
int vfs_configType(
  HOST_CFG *anchor,        /* IN-OUT:  result   */
  char *input)             /* IN: input text    */
{
   return input[0];
}
/**
* Parse the List format described in cpuset(7)
*/
static void vfs_parseList( /* RETURN: None               */
  TOPO_MAP *map,           /* OUT: bit map               */
  char *input)             /* IN: list text              */
{
   char *term, c;
   int  bounds[2], n;
 
   cfg_bitClear(map);
   for(term = strtok(input, ",");term != NULL;term = strtok(NULL, ",")) {
      n = bounds[0] = bounds[1] = 0;
      while(0 != (int)(c = *term++))
         switch(c) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
               bounds[n] = bounds[n] * 10 + (int)(c - '0');
               break;
            case '-':
               n = 1;
            }
      cfg_bitSet(map, bounds[0]);
      if (0 != n)
         while(++bounds[0] <= bounds[1])
            cfg_bitSet(map, bounds[0]);
      }
}
/**
 * Parse the Mask format described in cpuset(7)
 */
static void vfs_parseMask( /* RETURN: None               */
  TOPO_MAP *map,           /* OUT: bit map               */
  char *input)             /* IN: list text              */
{
   char *term, c;
   int  m, n;
 
   cfg_bitClear(map);
   for(term = strtok(input, ",");term != NULL;term = strtok(NULL, ",")) {
      m = 0;
      while(0 != (int)(c = *term++))
        switch(c) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
               m = m * 16 + (int)(c - '0');
               break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
               m = m * 16 + (int)(c - 'A') + 10;
               break;
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
               m = m * 16 + (int)(c - 'a') + 10;
            }
   for(n=map->msw;n>=0;n--)
      map->bits[n+1] = map->bits[n];
   if (0 != map->bits[map->msw+1])
      map->msw++;
   map->bits[0] = 0;
   for(n=0;n<32;n++)
      if (m & (1<<n))
         cfg_bitSet(map, n);
   }
}
#endif

/*************************  VFS support  ***************************************/

