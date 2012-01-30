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
#ifndef HAVEGECOLLECT_H
#define HAVEGECOLLECT_H
/**
 ** These definitions define the environment needed to build havege
 ** using the gcc compiler.
 */
#include "havege.h"
#include <sys/time.h>
/**
 ** Hardware constraints
 */
#define CRYPTOSIZECOLLECT 0x040000 /* 256k (1MB int)   */
#define NDSIZECOLLECT     0x100000 /* 1M   (4MB int)   */
#define NDSIZECOLLECTx2   0x200000 /* 2x NDSIZECOLLECT */
#define MININITRAND       32
/**
 ** Microsecond resolution times use gettimeofday
 */
#define MSC_DATA          static struct timeval et0,et1;
#define MSC_ELAPSED()     (et1.tv_sec - et0.tv_sec)*1000000 + et1.tv_usec - et0.tv_usec
#define MSC_START()       gettimeofday(&et0,NULL)
#define MSC_STOP()        gettimeofday(&et1,NULL)
/**
 ** Compiler intrinsics are used to make the build more portable and stable
 ** with fallbacks provided where the intrisics cannot be used. 
 */
#ifdef __GNUC__
/* ################################################################################# */

/**
 ** For the GNU compiler, the use of a cpuid intrinsic is somewhat garbled by the
 ** fact that some distributions (Centos 5.x) carry an empty cpuid.h (in order
 ** to back patch glicb?). AFAIK cpuid did not appear in gcc until version 4.3
 ** although it was in existance before. If we do not have a valid cpuid.h,
 ** we provide our own copy of the file (from gcc 4.3)
 **
 ** Also, gcc 4.4 and later provide an optimize attribute which remedies the
 ** effect ever increasing optimization on the collection loop
 */
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100  +  __GNUC_PATCHLEVEL__)

#define ASM __asm__ volatile
/**
 ** The collection mechanism cannot withstand agressive optimization
 */
#if GCC_VERSION<40400
unsigned int havege_collect(volatile H_PTR hptr);
#else
unsigned int havege_collect(volatile H_PTR hptr) __attribute__((optimize(1)));
#endif
/**
 ** For the intel world...
 */
#ifdef HAVE_ISA_X86
#define ARCH "x86"

#if GCC_VERSION<40300
#undef HAVE_CPUID_H
#endif
#ifdef HAVE_CPUID_H
#include <cpuid.h>
#else
#include "cpuid-43.h"
#endif
/**
 ** Compatability wrappers
 */
#define CPUID(level,p)\
  {\
  __cpuid_count (level,p[3],p[0],p[1],p[2],p[3]);\
  }
#define HASCPUID(p) __get_cpuid_max(0, p)
/**
 ** The rdtsc intrinsic is called in by x86intrin.h - also a recent gcc innovation
 ** There have been some discussions of the code in 4.5 and 4.6, so you may opt
 ** to use the inline alternative based on GCC_VERSION
 */
#ifdef HAVE_X86INTRIN_H
#include <x86intrin.h>
#define HARDCLOCK(x) x=__rdtsc()
#else
#define HARDCLOCK(x) ASM("rdtsc;movl %%eax,%0":"=m"(x)::"ax","dx")
#endif
#else
/**
 * No cpuid support outside of the x86 family
 */
#define CPUID(level,p) 0
#define HASCPUID(p)    0

#ifdef HAVE_ISA_SPARC
#define ARCH "sparc"
#define HARDCLOCK(x) ASM("rd %%tick, %0":"=r"(x):"r"(x))
#endif

#ifdef HAVE_ISA_SPARCLITE
#define ARCH "sparclite"
#define HARDCLOCK(x) ASM(".byte 0x83, 0x41, 0x00, 0x00");\
  ASM("mov   %%g1, %0" : "=r"(x))
#endif

#ifdef HAVE_ISA_PPC
#define ARCH "ppc"
#define HARDCLOCK(x) ASM("mftb %0":"=r"(x)) /* eq. to mftb %0, 268 */
#endif

#ifdef HAVE_ISA_IA64
#define ARCH "ia64"
#define CPUID(op,reg) ASM("mov %0=cpuid[%1]"\
   : "=r" (value)\
   : "r" (reg))
#define HARDCLOCK(x) ASM("mov %0=ar.itc" : "=r"(x))
#define HASCPUID(x) x=1
#endif
#endif
/**
 *  Use the "&&" extension to calculate the LOOP_PT
 */
#define CODE_PT(a)   a
#define LOOP_PT(a)   &&loop##a
/* ################################################################################# */
#endif
/**
 * For the MSVC world
 */
#if _MSVC_VERS
/* ################################################################################# */
#define ARCH "x86"
/**
 ** The collection mechanism
 */
unsigned int havege_collect(volatile H_PTR hptr);
/**
 * For the MSVC compilers V8 and above
 */
#include <intrin.h>
/**
 * Read the processor timestamp counter
 */
#define HARDCLOCK(x) x=__rdtsc()
/**
 * Normalize to the gcc interface
 */
#define CPUID(level,p) return __cpuidx(p, p[3], level)
#define HASCPUID(p) \
  {
  CPUID(0,a,b,c,d)
  }  
/**
 * Use the __ReturnAddress intrisic to calculate the LOOP_PT
 */
#define CODE_PT(a) __ReturnAddress()
#define LOOP_PT(a) 0
#endif
/* ################################################################################# */

#endif
