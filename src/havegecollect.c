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
 */
/**
 * This compile unit isolates the operation of the HAVEGE algorithm to better
 * deal with compiler issues. Portability of the algorithm is improved by
 * isolating enviromental dependencies in the HARDCLOCK and CSZ macros.
 */
#include <stdio.h>

#include <stdlib.h>
#include "havege.h"
#include "havegecollect.h"
/**
 * This type is used by havege_df() as an affectation to allow static variables
 * escape the optimizers data flow analysis
 */
typedef void volatile * VVAR;
/**
 * N.B. The walk mask has been relocated for better debugging diagnosis.
 */
#define ANDPT     havege_andpt
/**
 * The LOOP macro is designed to both size and control the collection loop through use
 * of the loop index. Calculation nodes are numbered from LOOP_CT down to 0 and
 * for those nodes numbered greater than the loop index, the collection iteration following
 * is executed at the cost of introducing an extra conditional branch.
 *
 * For those nodes numbered less than the loop index, havege_sp() is called to determine
 * the action. If the loop index has not been initialized, the collection iteration is
 * skipped and execution continues at the next loop index. If the loop index has been
 * initialized, control transfers to either the start or end of the collection routine
 * depending upon whether the collection buffer has been filled.
 */
#define LOOP(n,m) loop##n: if (n < loop_idx) { \
                              switch(havege_sp(i,n,LOOP_PT(n))) { \
                                 case 0:   goto loop##m; \
                                 case 1:   goto loop40; \
                                 default:  goto loop_exit; \
                                 } \
                              }
#if 0
#define ROR32(value,shift)   ((value >> (shift)) | (value << (32-shift)))
#else
inline U_INT ror32(const U_INT value, const U_INT shift) {
   return (value >> shift) | (value << (32 - shift));
}
#define ROR32(value,shift) ror32(value, shift)
#endif

/**
 * Significant variables used in the calculation. The data is declared local to this
 * compile unit but referenced by havege_df() to ensure that a clever optimizer does
 * not decide to ignore the volatile because a variable only has local access.
 */
static volatile U_INT  havege_bigarray [NDSIZECOLLECT + 16384];
static volatile U_INT  havege_andpt;
static volatile U_INT  havege_hardtick;
static volatile U_INT  loop_idx = LOOP_CT+1;
static volatile U_INT  *havege_pwalk;
static volatile char *havege_pts[LOOP_CT+1];
static volatile U_INT  *Pt0;
static volatile U_INT  *Pt1;
static volatile U_INT  *Pt2;
static volatile U_INT  *Pt3;
static volatile U_INT   PT;
static volatile U_INT   PT2;
static volatile U_INT   pt2;
static volatile U_INT   PTtest;
/**
 * Local prototypes
 */
// int havege_collect(volatile H_PTR hptr) __attribute__((optimize(1)));

static U_INT           havege_sp(U_INT i, U_INT n, char *p);
static volatile U_INT *havege_tune(H_PTR h);
/**
 * The collection loop is constructed by repetitions of oneinteration.h with the
 * number of repetitions tailored to the size of the instruction cache. The use
 * of volatile variables forces the compiler to produce the correct sequence of
 * operations for an iteration but DOES NOT prevent compiler optimization of a
 * sequence of interations.
 */
U_INT havege_collect(volatile H_PTR hptr)
{
   volatile U_INT * RESULT = havege_bigarray;
   U_INT i=0,pt=0,inter=0;

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
   havege_sp(i,0,LOOP_PT(0));
   havege_pwalk = havege_tune(hptr);
loop_exit:
   return ANDPT==0? 0 : 1;
}
/**
 * This function provides additional optimizer insurance. It should never be called.
 * But because it CAN export the addresses of the calculation static variables to an
 * outside caller, this must further limit any compiler optimizations.
 */
VVAR *havege_df()
{
   static VVAR escape[16] = {
      havege_bigarray,  // 0
      &havege_andpt,    // 1
      &havege_hardtick, // 2
      &havege_pwalk,    // 3
      havege_pts,       // 4
      &Pt0,             // 5
      &Pt1,             // 6
      &Pt2,             // 7
      &Pt3,             // 8
      &PT,              // 9
      &PT2,             // 10
      &pt2,             // 11
      &PTtest,          // 12
      0,                // 13
      0,                // 14
      0,                // 15
      };
   return escape;
}
/**
 * Create a sequence point that is executed for points NOT in the collection
 * sequence. This happens for all points on the collection pass and only for
 * the terminating point thereafter.
 */
static U_INT havege_sp(U_INT i, U_INT n,char *p)
{
   if (loop_idx < LOOP_CT)
      return i < NDSIZECOLLECT? 1 : 2;
   havege_pts[n] = CODE_PT(p);
   if (n==0) loop_idx = 0;
   return 0;
}
/**
 * Initialization routine called from havege_collect() to size the collection loop
 * based on the instruction cache and allocate the walk array based on the size
 * of the data cache.
 */
static volatile U_INT *havege_tune(H_PTR hptr)
{
   U_INT offsets[LOOP_CT+1];
   U_INT i,offs,*p,sz;

   hptr->havege_buf = (U_INT *)havege_bigarray;
   for (i=0;i<=LOOP_CT;i++)
      offsets[i] = abs(havege_pts[i]-havege_pts[LOOP_CT]);
   havege_debug(hptr, (char **)havege_pts, offsets);
   hptr->loop_idxmax = LOOP_CT;
   hptr->loop_szmax  = offsets[1];
   if (hptr->i_cache<1 || hptr->d_cache<1)
      return 0;
   sz = hptr->i_cache * 1024;
   for(i=LOOP_CT;i>0;i--)
      if (offsets[i]>sz)
         break;
   hptr->loop_idx = loop_idx = ++i;
   hptr->loop_sz  = offsets[i];
   ANDPT = ((2*hptr->d_cache*1024)/sizeof(int))-1;
   p    = (U_INT *) malloc((ANDPT + 4097)*sizeof(int));
   offs = (U_INT)((((unsigned long)&p[4096])&0xfff)/sizeof(int));
   return &p[4096-offs];
}
