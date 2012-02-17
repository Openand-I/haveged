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
 **
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "havege.h"
#include "havegecollect.h"

#if  NUMBER_CORES>1
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sched.h>
#include <signal.h>
/**
 * Collection thread directory
 */
typedef struct {
   U_INT    count;      /* associated count        */
   U_INT    last;       /* last output             */
   U_INT    *out;       /* buffer pointer          */
   U_INT    fatal;      /* fatal error in last     */
   sem_t    flags[1];   /* thread signals          */
} H_THREAD;
/**
 * Cleanup hooks
 */
static H_THREAD *exit_hook = NULL;
static U_INT    exit_ct    = 0;
/**
 * Local prototypes
 */
static void havege_ipc(H_PTR h_ptr, U_INT n, U_INT sz);
static int  havege_rngChild(H_PTR h_ptr, U_INT cNumber);
static void havege_signal(int signum);
#endif

/**
 * State
 */
static struct     hinfo info;

/**
 * Initialize the entropy collector.
 */
int havege_create(         /* RETURN: NZ on failure   */
  H_PTR h)                 /* IN-OUT: app anchor      */
{
   int        i = 0;

#if NUMBER_CORES>1
   for(i = 0; i < info.n_cores;i++)
      if (havege_rngChild(h, i)<0)
         return 1;
#else
   if (NULL==(h->collector = havege_ndcreate(h, i)))
      return 1;
#endif
   return 0;
}
/**
 * Cleanup collector(s). In a multi-thread environment, need to kill
 * children to avoid zombies.
 */
void havege_exit(          /* RETURN: None         */
  int level)               /* IN: NZ if thread     */
{
#if NUMBER_CORES>1
   H_THREAD *t = exit_hook;
   U_INT i;
   
   if (NULL != t) {
      t->fatal = H_EXIT;
      for(i=0;i<exit_ct;i++)
         (void)sem_post(&t->flags[i]);
      }
   if (level==0)
      wait((int *)&i);
#endif
}
/**
 * Initialize the environment based upon the tuning survey. This includes,
 * allocation the output buffer (in shared memory if mult-threaded) and
 * fitting the collection code to the tuning inputs.
 */
H_PTR havege_init(         /* RETURN: app state    */
  CMD_PARAMS *params)      /* IN: input params     */
{
   HOST_CFG   *env  = havege_tune(params);
   U_INT      n = params->nCores;

   if (0 == n)
      n = 1;
   info.error = H_NOBUF;
#if NUMBER_CORES>1
   havege_ipc(&info, n, params->ioSz);
#else
   info.io_buf  = malloc(params->ioSz);
   info.threads = NULL;
#endif
   if (NULL==info.io_buf)
      return &info;
   info.error           = H_NOERR;
   info.arch            = ARCH;
   info.params          = params;
   info.n_cores         = n;
   info.havege_opts     = params->options;
   info.i_collectSz     = params->collectSize;
   info.cpu             = &env->cpus[env->a_cpu];
   info.instCache       = &env->caches[env->i_tune];
   info.dataCache       = &env->caches[env->d_tune];
   havege_ndsetup(&info);
   return &info;
}
/**
 * Read random words. In the single-thread case, input is read by the calling the
 * collection method directly. In the multi-thread case, the request info is
 * signalled to the thread last read and this thread waits for a completion signal.
 */
int havege_rng(            /* RETURN: number words read     */
  H_PTR h,                 /* IN-OUT: app state             */
  U_INT *buffer,           /* OUT: read buffer              */
  U_INT sz)                /* IN: number words to read      */
{
#if NUMBER_CORES>1
   H_THREAD    *t = (H_THREAD *) h->threads;
   
   t->count = sz;
   t->out   = buffer;
   if (0!=sem_post(&t->flags[t->last]))
      h->error = H_NORQST;
   else if (0!=sem_wait(&t->flags[h->n_cores]))
      h->error = H_NOCOMP;
   else if (H_NOERR != t->fatal)
      h->error = t->fatal;
   else return sz;
   return -1;
#else
   U_INT i;

   for(i=0;i<sz;i++)
      buffer[i] = havege_ndread(h->collector);
   return i;
#endif
}
/**
 * Report setup
 */
void havege_status(
  H_PTR h_ptr,             /* IN-OUT: app state    */
  char *buf,               /* OUT: text buffer     */
  int sz)                  /* IN: buffer size      */
{
   const char *fmt =
      "version:     %s\n"
      "arch:        %s\n"
      "vendor:      %s\n"
      "cores:       %d/%d\n"
      "buffer:      %dK\n"
      "i_cache:     %dK (%06x)\n"
      "i_index:     %d/%d\n"
      "i_size:      %d/%d\n"
      "d_cache:     %dK (%06x)\n";
   (void) snprintf(buf, sz, fmt,
      h_ptr->params->version,
      h_ptr->arch,
      h_ptr->cpu->vendor,
      h_ptr->n_cores,
      NUMBER_CORES,
      h_ptr->i_collectSz/1024,
      h_ptr->instCache->size,
      h_ptr->instCache->cpuMap.source,
      h_ptr->i_idx, h_ptr->i_maxidx,
      h_ptr->i_sz,  h_ptr->i_maxsz,
      h_ptr->dataCache->size,
      h_ptr->dataCache->cpuMap.source
      );
}
#if NUMBER_CORES > 1
/**
 * Initialize IPC mechanism. This consists of setting up a shared memory area
 * containing the output buffer and the collection thread directory.
 */
static void havege_ipc(H_PTR h, U_INT n, U_INT sz)
{
   void     *m;
   H_THREAD *t;
   int      i;

   h->io_buf = h->threads = NULL;
   m = mmap(NULL,
            sz + sizeof(H_THREAD) + n * sizeof(sem_t),
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANONYMOUS,
            0,
            0);
   if (m != MAP_FAILED) {
      t = (H_THREAD *)((char *) m + sz);
      memset(t, 0, sizeof(H_THREAD));
      for(i=0;i<=n;i++)
         if(sem_init(&t->flags[i],1,0) < 0) {
            h->error = H_NOINIT;
            return;
            }
      h->io_buf  = m;
      h->threads = exit_hook = t;
      exit_ct = n;
      }
}
/**
 * Child harvester task. If task fails to start H_PTR::error will be set to reason.
 * If task fails after start, H_THREAD::fatal will be set to the reason and a completion
 * will be posted to prevent the main thread from hanging waiting for a response.
 */
static int havege_rngChild(
  H_PTR h_ptr,             /* IN: app state        */
  U_INT cNumber)           /* IN: collector index  */
{
   H_COLLECT   *h_ctxt;
   H_THREAD    *thds = (H_THREAD *) h_ptr->threads;
   U_INT       cNext, i, r;
   int         pid;

   switch(pid=fork()) {
      case 0:

#ifdef SIGHUP
         signal(SIGHUP, havege_signal);
#endif
         signal(SIGINT, havege_signal);
         signal(SIGTERM, havege_signal);

         h_ctxt = havege_ndcreate(h_ptr, cNumber);
         if (NULL != h_ctxt) {
            cNext = (cNumber + 1) % h_ptr->n_cores;
            while(1) {
               if (0!=sem_wait(&thds->flags[cNumber])) {
                  thds->fatal = H_NOWAIT;
                  break;
                  }
               if (H_NOERR != thds->fatal)
                  exit(0);
               thds->last = cNumber;
               r = h_ctxt->havege_szFill - h_ctxt->havege_nptr;
               if (thds->count < r)
                  r = thds->count; 
               for(i=0;i<r;i++)
                  thds->out[i] = havege_ndread(h_ctxt);
               if (0==(thds->count -= i)) {
                  if (0!=sem_post(&thds->flags[h_ptr->n_cores])) {
                     thds->fatal = H_NODONE;
                     break;
                     }
                  continue;
                  }
               thds->out += i;
               if (0!=sem_post(&thds->flags[cNext])) {
                  thds->fatal = H_NOPOST;
                  break;
                  }
#ifdef HAVE_SCHED_YIELD
               (void)sched_yield();
#endif
               (void)havege_ndread(h_ctxt);
               h_ctxt->havege_nptr = 0;
               }
            }
         else thds->fatal = h_ptr->error;                /* h_ptr is a copy!!    */
         (void)sem_post(&thds->flags[h_ptr->n_cores]);   /* announce death!      */
         break;
      case -1:
         h_ptr->error = H_NOTASK;
      }
   return pid;
}
/**
 * Local signal handling
 */
static void havege_signal(int signum)
{
   havege_exit(1);
}

#endif


