/**
 ** Simple entropy harvester based upon the havege RNG
 **
 ** Copyright 2009-2014 Gary Wuertz gary@issiweb.com
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
/**
 * This compile unit isolates the operation of the HAVEGE algorithm to better
 * deal with compiler issues. Extensive macro expansion used to deal with
 * hardware variations.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "havegecollect.h"
#include "havegetest.h"
#include "havegetune.h"
/**
 * Injection and capture diagnostics
 */
#if defined(RAW_IN_ENABLE) || defined(RAW_OUT_ENABLE)
#define  DIAGNOSTICS_ENABLE
#endif
/**
 * Option to use clockgettime() as timer source
 */
#if defined(ENABLE_CLOCK_GETTIME)
#include <time.h>

#undef  HARDCLOCK
#define HARDCLOCK(x) x = havege_clock()
/**
 * Provide a generic timer fallback
 */

/* Credits https://en.wikipedia.org/wiki/Xorshift */

uint64_t x, y;


static void set_S_0(void)
{
	struct timespec ts_I, ts_II;

	clock_gettime(CLOCK_REALTIME, &ts_I);
	clock_gettime(CLOCK_REALTIME, &ts_II);
	
	x = ts_I.tv_nsec;
	y = ts_II.tv_nsec;
		
}

static uint64_t havege_clock(void)
{
	unsigned int count = 0;
    static int init = 0;
	
	if ( init == 0 ) {
//		set_S_0();
//		x = 22337203685;
//		y = 74586302733;
		x = random();
		y = random();
	}
	
	init++;
		
	x = x  + ( y * 1000000000LL );
	
	for ( count = 0; count < 81; count++ ) {

	x ^= x >> 12; // a
	x ^= x << 25; // b
	x ^= x >> 27; // c
		
	x = x * UINT64_C(2685821657736338717);
		
//fprintf(stderr,"x = %llu ; y = %llu ; z = %llu ; w = %llu", x, y, z, w);	
	}	

//fprintf(stderr,"x = %llu ; count = %d", x, count);	
	return x;
}
#endif

/**
 * Memory allocation sizing
 */
#define  SZH_INIT       sizeof(H_COLLECT)+sizeof(char *)*(LOOP_CT + 2)
#define  SZH_COLLECT(a) sizeof(H_COLLECT)+sizeof(H_UINT)*(a+16384-1)
/**
 * The HAVEGE collector is created by interleaving instructions generated by
 * oneiteration.h with the LOOP() output to control the sequence. At each
 * LOOP() point, the following actions are possible.
 */
typedef enum {
   LOOP_NEXT,        /* Next loop  */
   LOOP_ENTER,       /* First loop */
   LOOP_EXIT         /* Last loop  */
} LOOP_BRANCH;
/**
 * The LOOP macro labels calculation sequences generated by oneiteration.h in
 * decreasing order from LOOPCT down to 0. During a normal collection, the
 * loop construct only introduces an extra conditional branch into the instruction
 * stream. For the exceptional conditions (initialization, end-of-loop, and
 * raw HARDCLOCK capture), the return from havege_cp() is used to determine
 * the action to be taken.
 */
#define LOOP(n,m) loop##n: if (n < h_ctxt->havege_cdidx) { \
                              switch(havege_cp(h_ctxt,i,n,LOOP_PT(n))) { \
                                 case LOOP_NEXT:   goto loop##m; \
                                 case LOOP_ENTER:  goto loop_enter; \
                                 case LOOP_EXIT:   goto loop_exit; \
                                 } \
                              }
/**
 * These macros below bind the code contained in oneiteration.h to the H_COLLECT
 * instance defined above
 */
#define ANDPT   (h_ctxt->havege_andpt)
#define PTTEST  (h_ctxt->havege_PTtest)
#define PT      (h_ctxt->havege_PT)
#define PT1     (h_ctxt->havege_pt2)
#define PT2     (h_ctxt->havege_PT2)
#define PWALK   (h_ctxt->havege_pwalk)
#define RESULT  (h_ctxt->havege_bigarray)
/**
 * Previous diagnostic support has been replaced. The new implementation provides
 * simultaneous access to both the noise source (i.e. the timer tics) and the
 * output. The tics buffer is also used by the injection diagnostic if enabled
 */
#ifdef DIAGNOSTICS_ENABLE
#define HTICK1          (h_ctxt->havege_tics[i>>3])
#define HTICK2          (h_ctxt->havege_tics[i>>3])
#define SZ_TICK         ((h_ptr->i_collectSz)>>3)
#else
#define HTICK1          (h_ctxt->havege_tic)
#define HTICK2          (h_ctxt->havege_tic)
#define SZ_TICK         0
#endif

/**
 * If the injection diagnostic is enabled, use a wrapper for the timer source
 */
#ifdef   RAW_IN_ENABLE
static H_UINT havege_inject(H_COLLECT *h_ctxt, H_UINT x);

#define  HARDCLOCKR(x)  x=havege_inject(h_ctxt, x)
#else
#define  HARDCLOCKR(x)  HARDCLOCK(x)
#endif
/**
 * inline optimization - left conditional for legacy systems
 */
#if 0
#define ROR32(value,shift)   ((value >> (shift)) | (value << (32-shift)))
#else
inline static H_UINT ror32(const H_UINT value, const H_UINT shift) {
   return (value >> shift) | (value << (32 - shift));
}
#define ROR32(value,shift) ror32(value, shift)
#endif
/**
 * Local prototypes
 */
static LOOP_BRANCH havege_cp(H_COLLECT *h_ctxt, H_UINT i, H_UINT n, char *p);
/**
 * Protect the collection mechanism against ever-increasing gcc optimization
 */
#if defined (GCC_VERSION) && GCC_VERSION >= 40400
static int havege_gather(H_COLLECT * h_ctxt) __attribute__((optimize(1)));
#else
static int  havege_gather(H_COLLECT * h_ctxt);
#endif
static void havege_ndinit(H_PTR h_ptr, struct h_collect *h_ctxt);

/**
 * Create a collector
 */
H_COLLECT *havege_ndcreate(/* RETURN: NULL on failure          */
   H_PTR h_ptr,            /* IN-OUT: application instance     */
   H_UINT nCollector)      /* IN: The collector instance       */
{
   H_UINT      i,offs,*p,d_cache;
   H_UINT      szBuffer;
   H_COLLECT   *h_ctxt;

   szBuffer = h_ptr->i_collectSz;
   d_cache  = ((CACHE_INST *)(h_ptr->dataCache))->size;
   h_ctxt   = (H_COLLECT *) calloc(SZH_COLLECT(szBuffer + SZ_TICK),1);
   if (NULL != h_ctxt) {
      h_ctxt->havege_app        = h_ptr;
      h_ctxt->havege_idx        = nCollector;
      h_ctxt->havege_raw        = h_ptr->havege_opts & 0xff00;
      h_ctxt->havege_rawInput   = h_ptr->inject;
      h_ctxt->havege_szCollect  = szBuffer;
      h_ctxt->havege_szFill     = szBuffer>>3;
      h_ctxt->havege_cdidx      = h_ptr->i_idx;
      p                         = (H_UINT *) RESULT;
      h_ctxt->havege_err        = H_NOERR;
      h_ctxt->havege_tests      = 0;
      h_ctxt->havege_extra      = 0;
      h_ctxt->havege_tics       = p+szBuffer;
      
      /** An intermediate walk table twice the size of the L1 cache is allocated
       ** for use in permuting time stamp readings. The is meant to exercise
       ** processor TLBs.
       */
      ANDPT = ((2*d_cache*1024)/sizeof(H_UINT))-1;
      p     = (H_UINT *) calloc((ANDPT + 4097)*sizeof(H_UINT),1);
      if (NULL != p) {
         h_ctxt->havege_extra = p;
         offs = (H_UINT)((((unsigned long)&p[4096])&0xfff)/sizeof(H_UINT));
         PWALK = &p[4096-offs];
         /**
          * Warm up the generator, running the startup tests
          */
#if defined(RAW_IN_ENABLE)
      if (0 == (h_ctxt->havege_raw & H_DEBUG_TEST_IN))
#endif
         {
            H_UINT t0=0;
            
            (void)havege_gather(h_ctxt);           /* first sample   */
            t0 = h_ctxt->havege_tic;
            for(i=1;i<MININITRAND;i++)
               (void)havege_gather(h_ctxt);        /* warmup rng     */
            if (h_ctxt->havege_tic==t0) {          /* timer stuck?   */
               h_ptr->error = H_NOTIMER;
               havege_nddestroy(h_ctxt);
               return NULL;
               }
         }
#ifdef ONLINE_TESTS_ENABLE
         {
            procShared *ps = (procShared *)(h_ptr->testData);
            while(0!=ps->run(h_ctxt, 0)) {      /* run tot tests  */
               (void)havege_gather(h_ctxt);
               }
         }
         if (H_NOERR != (h_ptr->error = h_ctxt->havege_err)) {
            havege_nddestroy(h_ctxt);
            return NULL;
            }
#endif
         h_ctxt->havege_nptr = szBuffer;
         if (0 == (h_ctxt->havege_raw & H_DEBUG_RAW_OUT))
            h_ctxt->havege_szFill = szBuffer;
         }
      else {
         havege_nddestroy(h_ctxt);
         h_ptr->error = H_NOWALK;
         return NULL;
         }
      }
   else h_ptr->error = H_NOCOLLECT;
   return h_ctxt;
}
/**
 * Destruct a collector
 */
void havege_nddestroy(        /* RETURN: none           */
   H_COLLECT *h_ctxt)         /* IN: collector context  */
{
   if (0 != h_ctxt) {
      if (h_ctxt->havege_extra!=0) {
         free(h_ctxt->havege_extra);
         h_ctxt->havege_extra = 0;
         }
      if (h_ctxt->havege_tests!=0) {
         free(h_ctxt->havege_tests);
         h_ctxt->havege_tests = 0;
         }
      free((void *)h_ctxt);
   }
}
/**
 * Read from the collector.
 */
H_UINT havege_ndread(         /* RETURN: data value     */
   H_COLLECT *h_ctxt)         /* IN: collector context  */
{
   if (h_ctxt->havege_nptr >= h_ctxt->havege_szFill) {
      H_PTR h_ptr = (H_PTR)(h_ctxt->havege_app);
      pMeter pm;

      if (0 != (pm = h_ptr->metering))
         (*pm)(h_ctxt->havege_idx, 0);
#ifdef ONLINE_TESTS_ENABLE
      {
         procShared *ps = (procShared *)(h_ptr->testData);
         do {
            (void) havege_gather(h_ctxt);
            (void) ps->run(h_ctxt, 1);
            } while(ps->discard(h_ctxt)>0);
      }
#else
      (void) havege_gather(h_ctxt);
#endif
      h_ptr->n_fills += 1;
      if (0 != pm)
         (*pm)(h_ctxt->havege_idx, 1);
      h_ctxt->havege_nptr = 0;
      }
#ifdef   RAW_OUT_ENABLE
   if (0!=(h_ctxt->havege_raw & H_DEBUG_RAW_OUT))
      return h_ctxt->havege_tics[h_ctxt->havege_nptr++];
#endif
   return RESULT[h_ctxt->havege_nptr++];
}
/**
 * Setup haveged
 */
void havege_ndsetup(       /* RETURN: None                     */
   H_PTR h_ptr)            /* IN-OUT: application instance     */
{
   char  wkspc[SZH_INIT];
   
   memset(wkspc, 0, SZH_INIT);
   havege_ndinit(h_ptr, (struct h_collect *) wkspc);
}
/**
 * This method is called only for control points NOT part of a normal collection:
 *
 *   a) For a collection loop after all iterations are performed, this function
 *      determines if the collection buffer is full.
 *   b) For initialization, this method saves the address of the collection point
 *      for analysis at the end of the loop.
 */
static LOOP_BRANCH havege_cp( /* RETURN: branch to take */
   H_COLLECT *h_ctxt,         /* IN: collection context */
   H_UINT i,                  /* IN: collection offset  */
   H_UINT n,                  /* IN: iteration index    */
   char *p)                   /* IN: code pointer       */
{

   if (h_ctxt->havege_cdidx <= LOOP_CT)
      return i < h_ctxt->havege_szCollect? LOOP_ENTER : LOOP_EXIT;
   ((char **)RESULT)[n] = CODE_PT(p);
   if (n==0) h_ctxt->havege_cdidx = 0;
   return LOOP_NEXT;
}

/**
 * The collection loop is constructed by repetitions of oneinteration.h interleaved
 * with control points generated by the LOOP macro.
 */
static int havege_gather(     /* RETURN: 1 if initialized    */
   H_COLLECT * h_ctxt)        /* IN:     collector context  */
{
   H_UINT   i=0,pt=0,inter=0;
   H_UINT  *Pt0, *Pt1, *Pt2, *Pt3, *Ptinter;

#if defined(RAW_IN_ENABLE)
if (0 != (h_ctxt->havege_raw & H_DEBUG_RAW_IN)) {
   (*h_ctxt->havege_rawInput)(h_ctxt->havege_tics, h_ctxt->havege_szCollect>>3);
   h_ctxt->havege_tic = h_ctxt->havege_tics[0];
   }
else if (0 != (h_ctxt->havege_raw & H_DEBUG_TEST_IN)) {
   (*h_ctxt->havege_rawInput)(RESULT, h_ctxt->havege_szCollect);
   return 1;
   }
#endif
loop_enter:
LOOP(40,39)
   #include "oneiteration.h"
LOOP(39,38)
   #include "oneiteration.h"
LOOP(38,37)
   #include "oneiteration.h"
LOOP(37,36)
   #include "oneiteration.h"
LOOP(36,35)
   #include "oneiteration.h"
LOOP(35,34)
   #include "oneiteration.h"
LOOP(34,33)
   #include "oneiteration.h"
LOOP(33,32)
   #include "oneiteration.h"
LOOP(32,31)
   #include "oneiteration.h"
LOOP(31,30)
   #include "oneiteration.h"
LOOP(30,29)
   #include "oneiteration.h"
LOOP(29,28)
   #include "oneiteration.h"
LOOP(28,27)
   #include "oneiteration.h"
LOOP(27,26)
   #include "oneiteration.h"
LOOP(26,25)
   #include "oneiteration.h"
LOOP(25,24)
   #include "oneiteration.h"
LOOP(24,23)
   #include "oneiteration.h"
LOOP(23,22)
   #include "oneiteration.h"
LOOP(22,21)
   #include "oneiteration.h"
LOOP(21,20)
   #include "oneiteration.h"
LOOP(20,19)
   #include "oneiteration.h"
LOOP(19,18)
   #include "oneiteration.h"
LOOP(18,17)
   #include "oneiteration.h"
LOOP(17,16)
   #include "oneiteration.h"
LOOP(16,15)
   #include "oneiteration.h"
LOOP(15,14)
   #include "oneiteration.h"
LOOP(14,13)
   #include "oneiteration.h"
LOOP(13,12)
   #include "oneiteration.h"
LOOP(12,11)
   #include "oneiteration.h"
LOOP(11,10)
   #include "oneiteration.h"
LOOP(10,9)
   #include "oneiteration.h"
LOOP(9,8)
   #include "oneiteration.h"
LOOP(8,7)
   #include "oneiteration.h"
LOOP(7,6)
   #include "oneiteration.h"
LOOP(6,5)
   #include "oneiteration.h"
LOOP(5,4)
   #include "oneiteration.h"
LOOP(4,3)
   #include "oneiteration.h"
LOOP(3,2)
   #include "oneiteration.h"
LOOP(2,1)
   #include "oneiteration.h"
LOOP(1,0)
   #include "oneiteration.h"
LOOP(0,0)
   (void)havege_cp(h_ctxt, i,0,LOOP_PT(0));
loop_exit:
   return ANDPT==0? 0 : 1;
}
#ifdef   RAW_IN_ENABLE
/**
 * Wrapper for noise injector. When input is injected, the hardclock
 * call is not made and the contents of the tic buffer are used
 * unchanged from when the inject call was made at the top of the
 * loop.
 */
static H_UINT havege_inject(     /* RETURN: clock value           */
   H_COLLECT *h_ctxt,            /* IN: workspace                 */
   H_UINT x)                     /* IN: injected value            */
{
   if (0==(h_ctxt->havege_raw & H_DEBUG_RAW_IN)) {
      HARDCLOCK(x);
      }
   return x;
}
#endif
/**
 * Initialize the collection loop
 */
#if defined (GCC_VERSION) && GCC_VERSION >= 40600
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

static void havege_ndinit(       /* RETURN: None                     */
   H_PTR h_ptr,                  /* IN-OUT: application instance     */
   struct h_collect *h_ctxt)     /* IN: workspace                    */
{
   char  **addr = (char **)(&RESULT[0]);
   H_UINT sz;
   int   i;

   h_ctxt->havege_cdidx = LOOP_CT + 1;
   (void)havege_gather(h_ctxt);
   for (i=0;i<=LOOP_CT;i++) {
      if (0 != (h_ptr->havege_opts & H_DEBUG_COMPILE)) {
         h_ptr->print_msg("Address %u=%p\n", i, addr[i]);
         }
      RESULT[i] = abs(addr[i] - addr[LOOP_CT]);
      if (i > 0 && 0 != (h_ptr->havege_opts & H_DEBUG_LOOP)) {
         h_ptr->print_msg("Loop %u: offset=%u, delta=%u\n", i,RESULT[i],RESULT[i-1]-RESULT[i]);
         }
      }
   h_ptr->i_maxidx = LOOP_CT;
   h_ptr->i_maxsz  = RESULT[1];
   sz = ((CACHE_INST *)(h_ptr->instCache))->size * 1024;
   for(i=LOOP_CT;i>0;i--)
      if (RESULT[i]>sz)
         break;
   h_ptr->i_idx = ++i;
   h_ptr->i_sz  = RESULT[i];
}
