/**
 ** Simple entropy harvester based upon the havege RNG
 **
 ** Copyright 2009 Gary Wuertz gary@issiweb.com
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
#include "haveged.h"
/**
 * Configuration information
 */
#define GENERIC_DCACHE 16
#define GENERIC_ICACHE 16

struct hinfo {
   char  *arch;      // machine architecture ("x86","sparc","ppc","ia64")
   char  *vendor;    // for x86 architecture only
   int   generic;    // idication for generic fallback
   int   i_cache;    // size of instruction cache in kb
   int   d_cache;    // size of data cache in kb
   int   loop_idx;   // loop index (1-32)
   int   loop_sz;    // size of collection loop (bytes)
   int   etime;      // number of milleseconds required by last collection
   int   dbcpuid;    // non-zero for cpuid debugging
};
/**
 * Performance monitor
 */
struct hperf {
   int   fill;       // Set when filled
   int   etime;      // number of milleseconds required by last collection
};
/**
 * Public prototypes
 */
int   ndinit(struct pparams *params, struct hperf *perf);
void  ndinfo(struct hinfo *info);
int   ndrand (struct hperf *perf);
#endif
