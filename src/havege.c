/**
 ** Simple entropy harvester based upon the havege RNG
 **
 ** Copyright 2009-2011 Gary Wuertz gary@issiweb.com
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
 ** This source is an adaptation of work released as
 **
 ** Copyright (C) 2006 - André Seznec - Olivier Rochecouste
 **
 ** under version 2.1 of the GNU Lesser General Public License
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "havege.h"
#include "havegecollect.h"
/**
 * State
 */
static struct  hinfo info;                               // configuration
MSC_DATA;

int havege_collect(volatile H_PTR hptr);
/**
 ** local prototypes
 */
static int  cache_configure(void);
#ifdef CPUID
static int           configure_amd(void);
static int           configure_intel(unsigned int lsfn);
static void          cpuid(int fn, unsigned int *p, char * tag);
#endif
#ifdef CPUID
/**
 * Configure the collector for x86 architectures. If a cpuid instruction is present
 * use it to determine the sizes of the data and instruction caches. If these cannot
 * be used supply "generic" defaults.
 */
static int cache_configure(void)
{
   unsigned char regs[4*sizeof(int)];
   unsigned int *p = (unsigned int *)regs;
   int f;

   if (info.i_cache>0 && info.d_cache>0)
      return 1;
   HASCPUID(f);
   if (f)
      cpuid(0,p,"cache_configure");
   else p[0] = 0;
   switch(p[1]) {
      case 0x68747541:  info.vendor = "amd";       break;
      case 0x69727943:  info.vendor = "cyrix";     break;
      case 0x746e6543:  info.vendor = "centaur";   break;   // aka via
      case 0x756e6547:  info.vendor = "intel";     break;
      case 0x646f6547:  info.vendor = "natsemi";   break;
      case 0x52697365:
      case 0x65736952:  info.vendor = "rise";      break;   // now owned by sis
      case 0x20536953:  info.vendor = "sis";       break;
      }
   info.arch = ARCH;
   info.generic = 0;
   if (!strcmp(info.vendor,"amd") && configure_amd())
      ;
   else if (configure_intel(p[0]))
      ;
   else {
      info.generic = 1;
      if (info.d_cache<1)  info.d_cache = GENERIC_DCACHE;
      if (info.i_cache<1)  info.i_cache = GENERIC_ICACHE;
      }
   return 1;
}
/**
 * Automatic configuration for amd
 *
 * As per AMD document 2541, April 2008
 */
static int configure_amd(void)
{
   unsigned char regs[4*sizeof(int)];
   unsigned int *p = (unsigned int *)regs;

   cpuid(0x80000000,p,"configure_amd");
   if ((p[0]&15)>=5) {                       // We want the L1 info
      cpuid(0x80000005,p,"configure_amd");
      info.d_cache   =  (p[2]>>24) & 0xff;   // l1 data cache
      info.i_cache   =  (p[3]>>24) & 0xff;   // l1 instruction cache
      return 1;
      }
   return 0;
}
/**
 * Automatic configuration for Intel x86 chips
 *
 * Notes: The "pentium hack" is to use the trace cache size for the instruction cache
 *        if no instruction cache value is found.
 *
 *        Recent Intel processor handbooks, hint that a processor may return a
 *        cache descriptor of 0xff to say in effect "buzz-off use leaf 4". My
 *        limited testing with leaf 4 indicates that it does not return the
 *        same information as leaf 2 - so in this code leaf 4 is only used as
 *        a fallback....
 */
static int configure_intel(unsigned int lsfn)
{
   unsigned char regs[4*sizeof(int)];
   unsigned int *p = (unsigned int *)regs;
  /**
   * As per Intel application note 485, August 2009 the following table contains
   * triples descriptor#, type (0=instruction,1=data), size (kb)
   * This table contains only L1 instruction(0), data(1), and trace(2) items.
   */
   static const int desc[] = {
      0x06, 0,  8 , // 4-way set assoc, 32 byte line size
      0x08, 0, 16 , // 4-way set assoc, 32 byte line size
      0x09, 0, 32 , // 4-way set assoc, 64 byte line size +
      0x0a, 1,  8 , // 2 way set assoc, 32 byte line size
      0x0c, 1, 16 , // 4-way set assoc, 32 byte line size
      0x0d, 1, 16 , // 4-way set assoc, 64 byte line size +
      0x10, 1, 16 , // 4-way set assoc, 64 byte line size
      0x15, 0, 16 , // 4-way set assoc, 64 byte line size
      0x2c, 1, 32 , // 8-way set assoc, 64 byte line size
      0x30, 0, 32 , // 8-way set assoc, 64 byte line size
      0x60, 1, 16 , // 8-way set assoc, sectored cache, 64 byte line size
      0x66, 1,  8 , // 4-way set assoc, sectored cache, 64 byte line size
      0x67, 1, 16 , // 4-way set assoc, sectored cache, 64 byte line size
      0x68, 1, 32 , // 4-way set assoc, sectored cache, 64 byte line size
      0x70, 2, 12 , // 8-way set assoc
      0x71, 2, 16 , // 8-way set assoc
      0x72, 2, 32 , // 8-way set assoc
      0x73, 2, 64 , // 8-way set assoc
      0x77, 0, 16 , // 4-way set assoc, sectored cache, 64 byte line size
      0x00, 0,  0   // sentinel
      };
   int i,j,k,n,sizes[] = {0,0,0};

   cpuid(2,p,"configure_intel");
   n = p[0]&0xff;
   for(i=0;i<n;i++) {
      for(j=0;j<4;j++)
         if (p[j] & 0x80000000) p[j] = 0;
      for(j=0;j<sizeof(regs);j++) {
         if (!regs[j]) continue;
         for(k=0;desc[k]!=0;k+=3)
            if (desc[k]==regs[j]) {
               sizes[desc[k+1]] += desc[k+2];
               break;
               }
         if (DEBUG_ENABLED(DEBUG_CPUID))
            DEBUG_OUT("lookup %x %d %d\n", regs[j], desc[k+1], desc[k+2]);
         }
      if ((i+1)!=n)
         cpuid(2,p,"configure_intel(2)");
      }
   if (sizes[0]<sizes[2])	                  // pentium4 hack
      sizes[0] = sizes[2];
   if ((sizes[0]==0||sizes[1]==0) && lsfn>3) {
      int level, type, ways, parts, lines;
      for(i=0;i<15;i++) {
         p[3] = i;
         cpuid(4,p,"configure_intel(3)");
         if ((type=p[0]&0x1f)==0) break;     // No more info
         level = (p[0]>>5)&7;
         lines = p[1] & 0xfff;
         parts = (p[1]>>12) & 0x3ff;
         ways  = (p[1]>>22) & 0x3ff;
         n     = ((ways+1)*(parts+1)*(lines+1)*(p[3]+1))/1024;
         if (DEBUG_ENABLED(DEBUG_CPUID))
            DEBUG_OUT("type=%d,level=%d,ways=%d,parts=%d,lines=%d,sets=%d: %d\n",
               type,level,ways+1,parts+1,lines+1,p[3]+1,n);
         if (level==1)
            switch(type) {
               case 1:  sizes[1] = n;  break;      // data
               case 2:  sizes[0] = n;  break;      // instruction
               case 3:  sizes[2] = n;  break;      // unified
               }
         }
      }
   if (info.i_cache<1)
      info.i_cache   = sizes[0];
   if (info.d_cache<1)
      info.d_cache   = sizes[1];
   if (info.i_cache>0 && info.d_cache>0)
      return 1;
   return 0;
}
/**
 * Wrapper around the cpuid macro to assist in debugging
 */
static void cpuid(int fn, unsigned int *p, char * tag)
{
   CPUID(fn,p);
   if (DEBUG_ENABLED(DEBUG_CPUID)) {
      char *rn = "ABDC";
      char d[sizeof(int)+1];int i,j;

      DEBUG_OUT("%s:%d\n",tag,fn);
      for (i=0;i<4;i++) {
         int t = p[i];
         int c = 0;
         for (j=sizeof(unsigned int);j>=0;j--) {
            d[j] = (char)c;
            c = t&0xff;
            if (!isprint(c)) c = '.';
            t >>= 8;
            }
         DEBUG_OUT("E%cX %10x %s\n", rn[i], p[i], d);
         }
      }
}
#else
 /*
 * Configure the collector for other architectures. If command line defaults are not
 * supplied provide "generic" defaults.
 */
static int cache_configure(void)
{
   if (info.i_cache>0 && info.d_cache>0)
      ;
   else {
      info.generic = 1;
      if (info.d_cache<1)  info.d_cache = GENERIC_DCACHE;
      if (info.i_cache<1)  info.i_cache = GENERIC_ICACHE;
      }
   return 1;
}
#endif
/**
 * Debug setup code
 */
void  havege_debug(H_PTR hptr, char **havege_pts, int *pts)
{
   int i;

   if (DEBUG_ENABLED(DEBUG_COMPILE))
      for (i=0;i<=(LOOP_CT+1);i++)
         printf("Address %d=%p\n", i, havege_pts[i]);
   if (DEBUG_ENABLED(DEBUG_LOOP))
      for(i=1;i<(LOOP_CT+1);i++)
         DEBUG_OUT("Loop %d: offset=%d, delta=%d\n", i,pts[i],pts[i]-pts[i-1]);
}
/**
 * Configure the collector
 *
 * Initialize the entropy collector. An intermediate walk table twice the size
 * of the L1 data cache is allocated to be used in permutting processor time
 * stamp readings. This is meant to exercies processort TLBs.
 */
int havege_init(int icache, int dcache, int flags)
{
   info.i_cache = icache;
   info.d_cache = dcache;
   info.havege_opts = flags;
   if (cache_configure() && havege_collect(&info)!= 0) {
      const int max = MININITRAND*CRYPTOSIZECOLLECT/NDSIZECOLLECT;
      int i;

      for (i = 0; i < max; i++) {
         MSC_START();
         havege_collect(&info);
         MSC_STOP();
         info.etime = MSC_ELAPSED();
         }
      info.havege_ndpt = 0;
      return 1;
      }
   return 0;
}
/**
 * Limit access to our state variable to those who explicity ask
 */
H_RDR havege_state(void)
{
   return &info;
}
/**
 * Debug dump
 */
void havege_status(char *buf)
{
   const char *fmt =
      "arch:        %s\n"
      "vendor:      %s\n"
      "generic:     %d\n"
      "i_cache:     %d\n"
      "d_cache:     %d\n"
      "loop_idx:    %d\n"
      "loop_idxmax: %d\n"
      "loop_sz:     %d\n"
      "loop_szmax:  %d\n"
      "etime:       %d\n"
      "havege_ndpt  %d\n";
   sprintf(buf,fmt,
      info.arch,
      info.vendor,
      info.generic,
      info.i_cache,
      info.d_cache,
      info.loop_idx,
      info.loop_idxmax,
      info.loop_sz,
      info.loop_szmax,
      info.etime,
      info.havege_ndpt
      );
}
/**
 * Main access point
 */
int ndrand()
{
   if (info.havege_ndpt >= NDSIZECOLLECT) {
      MSC_START();
      havege_collect(&info);
      info.havege_ndpt = 0;
      MSC_STOP();
      info.etime = MSC_ELAPSED();
      }
   return info.havege_buf[info.havege_ndpt++];
}
