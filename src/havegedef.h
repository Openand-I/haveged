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
#ifndef HAVEGEDEF_H
#define HAVEGEDEF_H
/**
 * Hardware constraints
 */
#define CRYPTOSIZECOLLECT 0x040000 /* 256k (1MB int)   */
#define NDSIZECOLLECT     0x100000 /* 1M   (4MB int)   */
#define NDSIZECOLLECTx2   0x200000 /* 2x NDSIZECOLLECT */
#define MININITRAND       32
#define NBSTEPHIDING      32
/**
 * Map original compile time constant to local static
 */
#define ANDPT andpt
/*
 * Only GNU Compilers need apply
 */
#define GNUCC
#ifdef GNUCC
#define ASM __asm__ volatile
#endif
/**
 * For intel iron, configuration uses the cpuid instruction. This app assumes it
 * is present if this is a 64 bit architecture because the 32 bit test uses the
 * stack and won't fly in 64 bit land.
 *
 * N.B. The definition of HARDCLOCK AS 'ASM("rdtsc":"=A"(x))' is incorrect for x86_64
 * and even documented as such in the gnu assembler doc under machine constraints.
 */
#ifdef HAVE_ISA_X86
#define ARCH "x86"
#define CPUID(op,regs) ASM("xchgl  %%ebx,%0\n\tcpuid  \n\txchgl  %%ebx,%0\n\t"\
  : "+r" (regs[1]), "=a" (regs[0]), "=c" (regs[3]), "=d" (regs[2])\
  : "1" (op), "2" (regs[3]) :  "%ebx")
#define HARDCLOCK(x) ASM("rdtsc;movl %%eax,%0":"=m"(x)::"ax","dx")
#ifdef HAVE_64
#define HASCPUID(x)  x=1
#else
#define HASCPUID(x)  ASM ("pushfl;popl %%eax;movl %%eax,%%ecx;xorl $0x00200000,%%eax;"\
  "pushl %%eax;popfl;pushfl;popl %%eax;xorl %%ecx,%%eax":"=a" (x))
#endif
#endif

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
