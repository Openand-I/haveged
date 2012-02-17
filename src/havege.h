/**
 ** Simple entropy harvester based upon the havege RNG
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
 */
#ifndef HAVEGE_H
#define HAVEGE_H
/**
 * Limits
 */
#define NDSIZECOLLECT   (128*1024)        /* Collection size: 128K U_INT = .5M byte */
#define MININITRAND     32                /* Number of initial fills to prime RNG   */
#define MAX_BIT_IDX     8                 /* Size of resource bitmaps               */
#define MAX_CACHES      8                 /* Max cache types                        */
#define MAX_CPUS        8                 /* Max cpu types                          */
#ifndef FILENAME_MAX
#define FILENAME_MAX    256               /* Max path length                        */
#endif
/**
 * Options flags
 */
#define VERBOSE         0x001             /* Show config info           */
#define DEBUG_TIME      0x004             /* Show collection times      */
#define DEBUG_LOOP      0x008             /* Show loop parameters       */
#define DEBUG_COMPILE   0x010             /* Show assembly info         */
/**
 * The following are available only if RAW functionality is built.
 */
#define DEBUG_RAW_OUT   0x100
#define DEBUG_RAW_IN    0x200
/**
 * Debugging definitions
 */
#define DEBUG_OUT(...)  fprintf(stderr,__VA_ARGS__)
/**
 * Basic types
 */
typedef unsigned int U_INT;
typedef unsigned int U_CHAR;
/**
 * Object used to represent a set of objects
 */
typedef struct {
  U_INT        bits[MAX_BIT_IDX];
  int          msw;
  U_INT        source;
} TOPO_MAP;
/**
 * Cache instance
 */
typedef struct {
   TOPO_MAP    cpuMap;                    /* what cpus have this cache                    */
   U_CHAR      type;                      /* 'I'nstruction, 'D'ata, 'U'nified, 'T'race    */
   U_CHAR      level;                     /* 0-15................                         */
   U_INT       size;                      /* size in KB                                   */
} CACHE_INST;
/**
 * Sources for CACHE_INST TOPO_MAP
 */
#define  SRC_DEFAULT          0x00001
#define  SRC_PARAM            0x00002
#define  SRC_CPUID_AMD6       0x00004
#define  SRC_CPUID_AMD5       0x00008
#define  SRC_CPUID_INTEL2     0x00010
#define  SRC_CPUID_INTEL4     0x00020
#define  SRC_VFS_INDEX        0x00040
/**
 * CPU instance
 */
typedef struct {
   TOPO_MAP    cpuMap;                    /* what cpus have this config    */
   U_INT       signature;                 /* processor signature           */
   U_INT       flags;
   U_INT       maxFn;
   U_INT       maxFnx;
   char        vendor[16];
} CPU_INST;
/**
 * Sources for CPU_INST TOPO_MAP
 */
#define  SRC_CPUID_PRESENT    0x00100
#define  SRC_CPUID_HT         0x00200
#define  SRC_CPUID_AMD        0x00400
#define  SRC_CPUID_AMD8       0x00800
#define  SRC_CPUID_LEAFB      0x01000
#define  SRC_CPUID_LEAF4      0x02000
#define  SRC_VFS_STATUS       0x04000
#define  SRC_VFS_ONLINE       0x08000
#define  SRC_VFS_CPUINFO      0x10000
#define  SRC_VFS_CPUDIR       0x20000
/**
 * The result of tuning
 */
typedef struct {
   U_INT       szCollect;                 /* collection buffer size     */
   char        *procfs;                   /* where proc is mounted      */
   char        *sysfs;                    /* where sys is mounted       */
   TOPO_MAP    pAllowed;                  /* allowed processors         */
   TOPO_MAP    pOnline;                   /* processors online          */
   TOPO_MAP    pCpuInfo;                  /* processors with info       */
   TOPO_MAP    pCacheInfo;                /* processors with cache info */
   TOPO_MAP    mAllowed;                  /* allowed memory             */
   U_INT       a_cpu;                     /* suggested cpu              */
   U_INT       i_tune;                    /* suggested i cache value    */
   U_INT       d_tune;                    /* suggested d cache value    */
   int         ctCpu;                     /* number of cpu types        */
   int         ctCache;                   /* number of cache items      */
   CPU_INST    cpus[MAX_CPUS];            /* cpu instances              */
   CACHE_INST  caches[MAX_CACHES+2];      /* cache instances            */
} HOST_CFG;
/**
 * Injection call-back for RAW diagnostics
 */
typedef int (*pRawIn)(volatile U_INT *pData, U_INT szData);
/**
 * Metering call-back
 */
typedef void (*pMeter)(U_INT nCollect, U_INT event);
/**
 * Invocation parameters
 */
typedef struct {
   U_INT       ioSz;                      /* size of write buffer      */
   U_INT       collectSize;               /* size of collection buffer */
   U_INT       icacheSize;                /* Instruction cache size    */
   U_INT       dcacheSize;                /* Data cache size           */
   U_INT       poolsize;                  /* size of random pool              */
   U_INT       options;                   /* Other options             */
   U_INT       nCores;                    /* If multi-core             */
   pMeter      metering;                  /* meterming method          */
   pRawIn      injection;                 /* injection  method         */
   char        *procFs;                   /* proc mount point override */
   char        *sysFs;                    /* sys mount point override  */
   char        *version;                  /* package version           */
} CMD_PARAMS;
/**
 * Status/monitoring information
 */
typedef struct {
   U_INT       n_fill;                    /* number times filled              */
   U_INT       etime;                     /* microseconds for last collection */
   U_INT       tmp_sec;                   /* scratch area for calculation     */
   U_INT       tmp_usec;                  /* scratch area for calculation     */
} H_STATUS;
/**
 * The collection context
 */
typedef struct h_collect {
   void    *havege_app;                   /* Application block             */
   U_INT    havege_idx;                   /* Identifer                     */
   U_INT    havege_szCollect;             /* Size of collection buffer     */
   U_INT    havege_raw;                   /* RAW mode control flags        */
   U_INT    havege_szFill;                /* Fill size                     */
   U_INT    havege_nptr;                  /* Input pointer                 */
   pRawIn   havege_rawInput;              /* Injection function            */
   pMeter   havege_meter;                 /* Metering function             */
   U_INT    havege_cdidx;                 /* normal mode control flags     */
   U_INT   *havege_pwalk;                 /* Instance variable             */
   U_INT    havege_andpt;                 /* Instance variable             */
   U_INT    havege_hardtick1;             /* Instance variable             */
   U_INT    havege_hardtick2;             /* Instance variable             */
   U_INT    havege_PT;                    /* Instance variable             */
   U_INT    havege_PT2;                   /* Instance variable             */
   U_INT    havege_pt2;                   /* Instance variable             */
   U_INT    havege_PTtest;                /* Instance variable             */
   U_INT    havege_bigarray[1];           /* collection buffer             */
} volatile H_COLLECT;
/**
 * Error diagnostics
 */
typedef enum {
   H_NOERR,          /* No error                         */
   H_NOBUF,          /* Output buffer allocation failed  */
   H_NOINIT,         /* semaphore init failed            */
   H_NOCOLLECT,      /* H_COLLECT allocation failed      */
   H_NOWALK,         /* Walk buffer allocation failed    */
   H_NOTIMER,        /* Timer failed self test           */
   H_NOTASK,         /* Unable to create child task      */
   H_NOWAIT,         /* sem_wait failed                  */
   H_NOPOST,         /* sem_post failed                  */
   H_NODONE,         /* sem_post done failed             */
   H_NORQST,         /* sem_post request failed          */
   H_NOCOMP,         /* wait for completion failed       */
   H_EXIT            /* Exit signal                      */
} H_ERR;
/**
 * Anchor for the application
 */
typedef struct hinfo {
   void        *io_buf;                   /* output buffer                    */
   char        *arch;                     /* "x86","sparc","ppc","ia64",etc   */
   CMD_PARAMS  *params;                   /* Invocation parameters            */
   U_INT       havege_opts;               /* option flags                     */
   CPU_INST    *cpu;                      /* information on the cpu           */
   CACHE_INST  *instCache;                /* instruction cache info           */
   CACHE_INST  *dataCache;                /* data cache info                  */
   U_INT       i_maxidx;                  /* maximum instruction loop index   */
   U_INT       i_maxsz;                   /* maximum code size                */
   U_INT       i_idx;                     /* code index used                  */
   U_INT       i_sz;                      /* code size used                   */
   U_INT       i_collectSz;               /* size of collection buffer        */
   U_INT       n_cores;                   /* number of cores                  */
   U_INT       error;                     /* H_ERR enum for status            */
   H_COLLECT   *collector;                /* single thread collector          */
   void        *threads;                  /* multi thread collectors          */
} *H_PTR;
/**
 * Public prototypes
 */
int         havege_create(H_PTR hptr);
void        havege_exit(int level);
H_PTR       havege_init(CMD_PARAMS *params);
int         havege_rng(H_PTR hptr, U_INT *buf, U_INT sz);
void        havege_status(H_PTR hptr, char *buf, int sz);
/**
 ** The public collection interface
 */
H_COLLECT   *havege_ndcreate(H_PTR hptr, U_INT nCollector);
U_INT       havege_ndread(H_COLLECT *rdr);
void        havege_ndsetup(H_PTR hptr);
/**
 * Tuning interface
 */
void        cfg_bitClear(TOPO_MAP *m);
int         cfg_bitCount(TOPO_MAP *m);
void        cfg_bitDisplay(TOPO_MAP *m);
int         cfg_bitNext(TOPO_MAP *m, int n);
void        cfg_bitSet(TOPO_MAP *m, int n);
HOST_CFG    *havege_tune(CMD_PARAMS *params);

#endif
