/**
 ** Simple entropy harvester based upon the havege RNG
 **
 ** Copyright 2009-2013 Gary Wuertz gary@issiweb.com
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
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>

#ifndef NO_DAEMON
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/random.h>
#endif

#include <errno.h>
#include "haveged.h"
/**
 * Parameters
 */
static struct pparams defaults = {
  .daemon         = PACKAGE,
  .setup          = 0,
  .ncores         = 0,
  .buffersz       = 0,
  .detached       = 0,
  .foreground     = 0,
  .d_cache        = 0,
  .i_cache        = 0,
  .run_level      = 0,
  .low_water      = 0,
  .tests_config   = 0,
  .os_rel         = "/proc/sys/kernel/osrelease",
  .pid_file       = "/var/run/haveged.pid",
  .poolsize       = "/proc/sys/kernel/random/poolsize",
  .random_device  = "/dev/random",
  .sample_in      = "data",
  .sample_out     = "sample",
  .verbose        = 0,
  .watermark      = "/proc/sys/kernel/random/write_wakeup_threshold"
  };
struct pparams *params = &defaults;
#ifdef  RAW_IN_ENABLE
FILE *fd_in;
/**
 * The injection diagnostic
 */
static int injectFile(volatile H_UINT *pData, H_UINT szData);
#endif
/**
 * havege instance used by application
 */
static H_PTR handle = NULL;
/**
 * Local prototypes
 */
#ifndef NO_DAEMON
static H_UINT poolSize = 0;

static void daemonize(void);
static int  get_poolsize(void);
static void run_daemon(H_PTR handle);
static void set_watermark(int level);
#endif

static void anchor_info(H_PTR h);
static void error_exit(const char *format, ...);
static int  get_runsize(unsigned int *bufct, unsigned int *bufrem, char *bp);
static char *ppSize(char *buffer, double sz);
static void print_msg(const char *format, ...);

static void run_app(H_PTR handle, H_UINT bufct, H_UINT bufres);
static void show_meterInfo(H_UINT id, H_UINT event);
static void tidy_exit(int signum);
static void usage(int nopts, struct option *long_options, const char **cmds);

#define  ATOU(a)     (unsigned int)atoi(a)
/**
 * Entry point
 */
int main(int argc, char **argv)
{
   static const char* cmds[] = {
      "b", "buffer",      "1", "Buffer size [KW], default: 128",
      "d", "data",        "1", "Data cache size [KB]",
      "i", "inst",        "1", "Instruction cache size [KB]",
      "f", "file",        "1", "Sample output file,  default: 'sample', '-' for stdout",
      "F", "Foreground",  "0", "Run daemon in foreground",
      "r", "run",         "1", "0=daemon, 1=config info, >1=<r>KB sample",
      "n", "number",      "1", "Output size in [k|m|g|t] bytes, 0 = unlimited (if stdout)",
      "o", "onlinetest",  "1", "[t<x>][c<x>] x=[a[n][w]][b[w]] 't'ot, 'c'ontinuous, default: ta8b",
      "p", "pidfile",     "1", "daemon pidfile, default: /var/run/haveged.pid",
      "s", "source",      "1", "Injection soure file",
      "t", "threads",     "1", "Number of threads",
      "v", "verbose",     "1", "Output level 0=minimal,1=config/fill info",
      "w", "write",       "1", "Set write_wakeup_threshold [bits]",
      "h", "help",        "0", "This help"
      };
   static int nopts = sizeof(cmds)/(4*sizeof(char *));
   struct option long_options[nopts+1];
   char short_options[1+nopts*2];
   int c,i,j;
   H_UINT bufct, bufrem, ierr;
   H_PARAMS cmd;

#if NO_DAEMON==1
   params->setup |= RUN_AS_APP;
#endif
#ifdef  RAW_IN_ENABLE
   params->setup |= INJECT | RUN_AS_APP;
#endif
#ifdef  RAW_OUT_ENABLE
   params->setup |= CAPTURE | RUN_AS_APP;
#endif
#if NUMBER_CORES>1
   params->setup |= MULTI_CORE;
#endif
   params->tests_config = (0 != (params->setup & RUN_AS_APP))? TESTS_DEFAULT_APP : TESTS_DEFAULT_RUN;
#ifdef SIGHUP
   signal(SIGHUP, tidy_exit);
#endif
   signal(SIGINT, tidy_exit);
   signal(SIGTERM, tidy_exit);
   strcpy(short_options,"");
   bufct  = bufrem = 0;

   for(i=j=0;j<(nopts*4);j+=4) {
      switch(cmds[j][0]) {
         case 'o':
#ifdef  ONLINE_TESTS_ENABLE
            break;
#else
            continue;
#endif
         case 'r':
            if (0!=(params->setup & (INJECT|CAPTURE))) {
               params->daemon = "havege_diagnostic";
               cmds[j+3] = "0=usage, 1=anchor_info, 2=capture, 4=inject, 6=inject test";
               }
            else if (0!=(params->setup & RUN_AS_APP))
               continue;
            break;
         case 's':
            if (0 == (params->setup & INJECT))
               continue;
            break;
         case 't':
            if (0 == (params->setup & MULTI_CORE))
               continue;
            break;
         case 'p':   case 'w':  case 'F':
            if (0 !=(params->setup & RUN_AS_APP))
               continue;
            break;
         }
      long_options[i].name      = cmds[j+1];
      long_options[i].has_arg   = atoi(cmds[j+2]);
      long_options[i].flag      = NULL;
      long_options[i].val       = cmds[j][0];
      strcat(short_options,cmds[j]);
      if (long_options[i].has_arg!=0) strcat(short_options,":");
      i += 1;
      }
   memset(&long_options[i], 0, sizeof(struct option));

   do {
      c = getopt_long (argc, argv, short_options, long_options, NULL);
      switch(c) {
         case 'F':
            params->setup |= RUN_IN_FG;
            params->foreground = 1;
            break;
         case 'b':
            params->buffersz = ATOU(optarg) * 1024;
            if (params->buffersz<4)
               error_exit("invalid size %s", optarg);
            break;
         case 'd':
            params->d_cache = ATOU(optarg);
            break;
         case 'i':
            params->i_cache = ATOU(optarg);
            break;
         case 'f':
            params->sample_out = optarg;
            if (strcmp(optarg,"-") == 0 )
               params->setup |= USE_STDOUT;
            break;
         case 'n':
            if (get_runsize(&bufct, &bufrem, optarg))
               error_exit("invalid count: %s", optarg);
            params->setup |= RUN_AS_APP | RANGE_SPEC;
            if (bufct==0 && bufrem==0)
               params->setup |= USE_STDOUT;
            break;
         case 'o':
            params->tests_config = optarg;
            break;
         case 'p':
            params->pid_file = optarg;
            break;
         case 'r':
            params->run_level  = ATOU(optarg);
            if (params->run_level != 0)
               params->setup |= RUN_AS_APP;
            break;
         case 's':
            params->sample_in = optarg;
            break;
         case 't':
            params->ncores = ATOU(optarg);
            if (params->ncores > NUMBER_CORES)
               error_exit("invalid thread count: %s", optarg);
            break;
         case 'v':
            params->verbose  = ATOU(optarg);
            break;
         case 'w':
            params->setup |= SET_LWM;
            params->low_water = ATOU(optarg);
            break;
         case '?':
         case 'h':
            usage(nopts, long_options, cmds);
         case -1:
            break;
         }
      } while (c!=-1);
   memset(&cmd, 0, sizeof(H_PARAMS));
   cmd.collectSize = params->buffersz;
   cmd.icacheSize  = params->i_cache;
   cmd.dcacheSize  = params->d_cache;
   cmd.options     = params->verbose & 0xff;
   cmd.nCores      = params->ncores;
   cmd.testSpec    = params->tests_config;
   cmd.msg_out     = print_msg;
   if (0 != (params->setup & RUN_AS_APP))
      cmd.ioSz = APP_BUFF_SIZE * sizeof(H_UINT);
#ifndef NO_DAEMON
   else  {
      poolSize = get_poolsize();
      i = (poolSize + 7)/8 * sizeof(H_UINT);
      cmd.ioSz = sizeof(struct rand_pool_info) + i *sizeof(H_UINT);
      }
#endif
   if (0 != (params->verbose & H_DEBUG_TIME))
      cmd.metering = show_meterInfo;

   if (0 !=(params->setup & CAPTURE) && 0 != (params->run_level == 2))
      cmd.options |= H_DEBUG_RAW_OUT;
#ifdef  RAW_IN_ENABLE
  if (0 !=(params->setup & INJECT) && 0 != (params->run_level & 4)) {
      fd_in = fopen(params->sample_in, "rb");
      if (NULL == fd_in)
         error_exit("Unable to open: %s", params->sample_in);
      cmd.injection = injectFile;
      if (params->run_level==4)
         cmd.options |= H_DEBUG_RAW_IN;
      else cmd.options |= H_DEBUG_TEST_IN;
      }
#endif
   handle = havege_create(&cmd);
   ierr = handle==NULL? H_NOHANDLE : handle->error;
   switch(ierr) {
      case H_NOERR:
         break;
      case H_NOTESTSPEC:
         error_exit("unrecognized test setup: %s", cmd.testSpec);
         break;
      default:
         error_exit("Couldn't initialize haveged (%d)", ierr);
   }
   if (0 != (params->setup & RUN_AS_APP)) {
      if (0 == (params->setup & RANGE_SPEC)) {
         if (params->run_level > 1)
            if (0!=(params->setup & (INJECT|CAPTURE)))
              usage(nopts, long_options, cmds);
            else run_app(handle,
               params->run_level/sizeof(H_UINT),
              (params->run_level%sizeof(H_UINT))*1024
              );
         else if (params->run_level==1 || 0 != (params->verbose & 1))
            anchor_info(handle);
         else usage(nopts, long_options, cmds);
         }
      else run_app(handle, bufct, bufrem);
      }
#ifndef NO_DAEMON
   else run_daemon(handle);
#endif
   havege_destroy(handle);
   exit(0);
}
#ifndef NO_DAEMON
/**
 * The usual daemon setup
 */
static void daemonize(     /* RETURN: nothing   */
   void)                   /* IN: nothing       */
{
   FILE *fh;
   openlog(params->daemon, LOG_CONS, LOG_DAEMON);
   syslog(LOG_NOTICE, "%s starting up", params->daemon);
   if (daemon(0, 0) == -1)
      error_exit("Cannot fork into the background");
   fh = fopen(params->pid_file, "w");
   if (!fh)
      error_exit("Couldn't open PID file \"%s\" for writing: %s.", params->pid_file, strerror(errno));
   fprintf(fh, "%i", getpid());
   fclose(fh);
   params->detached = 1;
}
/**
 * Get configured poolsize
 */
static int get_poolsize(   /* RETURN: number of bits  */
   void)                   /* IN: nothing             */
{
   FILE *poolsize_fh,*osrel_fh;
   unsigned int max_bits,major,minor;

   poolsize_fh = fopen(params->poolsize, "rb");
   if (poolsize_fh) {
      if (fscanf(poolsize_fh, "%d", &max_bits)!=1)
         max_bits = -1;
      fclose(poolsize_fh);
      osrel_fh = fopen(params->os_rel, "rb");
      if (osrel_fh) {
         if (fscanf(osrel_fh,"%d.%d", &major, &minor)<2)
           major = minor = 0;
         fclose(osrel_fh);
         if (major==2 && minor==4) max_bits *= 8;
         }
      }
   else max_bits = -1;
   if (max_bits < 1)
      error_exit("Couldn't get poolsize");
   return max_bits;
}
/**
 * Run as a daemon writing to random device entropy pool
 */
static void run_daemon(    /* RETURN: nothing   */
   H_PTR h)                /* IN: app instance  */
{
   int                     random_fd = -1;
   struct rand_pool_info   *output;

   if (0 != params->run_level) {
      anchor_info(h);
      return;
      }
   if (params->foreground==0)
     daemonize();
   else printf ("%s starting up\n", params->daemon);
   if (0 != havege_run(h))
      error_exit("Couldn't initialize HAVEGE rng %d", h->error);
   if (params->verbose & H_VERBOSE)
     anchor_info(h);
   if (params->low_water>0)
      set_watermark(params->low_water);
   random_fd = open(params->random_device, O_RDWR);
   if (random_fd == -1)
     error_exit("Couldn't open random device: %s", strerror(errno));

   output = (struct rand_pool_info *) h->io_buf;
   for(;;) {
      int current,nbytes,r;

      fd_set write_fd;
      FD_ZERO(&write_fd);
      FD_SET(random_fd, &write_fd);
      for(;;)  {
         int rc = select(random_fd+1, NULL, &write_fd, NULL, NULL);
         if (rc >= 0) break;
         if (errno != EINTR)
            error_exit("Select error: %s", strerror(errno));
         }
      if (ioctl(random_fd, RNDGETENTCNT, &current) == -1)
         error_exit("Couldn't query entropy-level from kernel");
      /* get number of bytes needed to fill pool */
      nbytes = (poolSize  - current)/8;
      if(nbytes<1)   continue;
      /* get that many random bytes */
      r = (nbytes+sizeof(H_UINT)-1)/sizeof(H_UINT);
      if (havege_rng(h, (H_UINT *)output->buf, r)<1)
         error_exit("RNG failed! %d", h->error);
      output->buf_size = nbytes;
      /* entropy is 8 bits per byte */
      output->entropy_count = nbytes * 8;
      if (ioctl(random_fd, RNDADDENTROPY, output) == -1)
         error_exit("RNDADDENTROPY failed!");
      }
}
/**
 * Set random write threshold
 */
static void set_watermark( /* RETURN: nothing   */
   int level)              /* IN: threshold     */
{
   FILE *wm_fh;

   wm_fh = fopen(params->watermark, "w");
   if (wm_fh) {
      fprintf(wm_fh, "%d\n", level);
      fclose(wm_fh);
      }
   else error_exit("Fail:set_watermark()!");
}
#endif
/**
 * Display handle information
 */
static void anchor_info(H_PTR h)
{
   char       buf[120];
   H_SD_TOPIC topics[4] = {H_SD_TOPIC_BUILD, H_SD_TOPIC_TUNE, H_SD_TOPIC_TEST, H_SD_TOPIC_SUM};
   int        i;
   
   for(i=0;i<4;i++)
      if (havege_status_dump(h, topics[i], buf, 120)>0)
         print_msg("%s\n", buf);
}
/**
 * Bail....
 */
static void error_exit(    /* RETURN: nothing   */
   const char *format,     /* IN: msg format    */
   ...)                    /* IN: varadic args  */
{
   char buffer[4096];

   va_list ap;
   va_start(ap, format);
   vsnprintf(buffer, sizeof(buffer), format, ap);
   va_end(ap);
#ifndef NO_DAEMON
   if (params->detached!=0) {
      unlink(params->pid_file);
      syslog(LOG_INFO, "%s %s", params->daemon, buffer);
      }
   else
#endif
      fprintf(stderr, "%s %s\n", params->daemon, buffer);
   havege_destroy(handle);
   exit(1);
}
/**
 * Implement fixed point shorthand for run sizes
 */
static int get_runsize(    /* RETURN: the size        */
   H_UINT *bufct,          /* OUT: nbr app buffers    */
   H_UINT *bufrem,         /* OUT: residue            */
   char *bp)               /* IN: the specification   */
{
   char        *suffix;
   double      f;
   int         p2 = 0;
   int         p10 = APP_BUFF_SIZE * sizeof(H_UINT);
   long long   ct;
   

   f = strtod(bp, &suffix);
   if (f < 0 || strlen(suffix)>1)
      return 1;
   switch(*suffix) {
      case 't': case 'T':
         p2 += 1;
      case 'g': case 'G':
         p2 += 1;
      case 'm': case 'M':
         p2 += 1;
      case 'k': case 'K':
         p2 += 1;
      case 0:
         break;
      default:
         return 2;
      }
   while(p2-- > 0)
      f *= 1024;
   ct = f;
   if (f != 0 && ct==0)
      return 3;
   if ((double) (ct+1) < f)
      return 3;
   *bufrem = (H_UINT)(ct%p10);
   *bufct  = (H_UINT)(ct/p10);
   if (*bufct == (ct/p10))
      return 0;
   /* hack to allow 16t */
   ct -= 1;
   *bufrem = (H_UINT)(ct%p10);
   *bufct  = (H_UINT)(ct/p10);
   return (*bufct == (ct/p10))? 0 : 4;
}
#ifdef  RAW_IN_ENABLE
/**
 * The injection diagnostic
 */
static int injectFile(     /* RETURN: not used  */
   volatile H_UINT *pData, /* OUT: data buffer  */
   H_UINT szData)          /* IN: H_UINT needed  */
{
   int r;

   if ((r=fread((void *)pData, sizeof(H_UINT), szData, fd_in)) != szData)
      error_exit("Cannot read data in file: %d!=%d", r, szData);
   return 0;
}
#endif
/**
 * Pretty print the collection size
 */
static char *ppSize(       /* RETURN: the formated size  */
   char *buffer,           /* IN: work space             */
   double sz)              /* IN: the size               */
{
   char   units[] = {'T', 'G', 'M', 'K', 0};
   double factor  = 1024.0 * 1024.0 * 1024.0 * 1024.0;
   int i;
   
   for (i=0;0 != units[i];i++) {
      if (sz >= factor)
         break;
      factor /= 1024.0;
      }
   sprintf(buffer, "%.4g %c byte", sz / factor, units[i]);
   return buffer;
}
/**
 * Execution notices - to stderr or syslog
 */
static void print_msg(     /* RETURN: nothing   */
   const char *format,     /* IN: format string */
   ...)                    /* IN: args          */
{
   char buffer[128];
   
   va_list ap;
   va_start(ap, format);
   snprintf(buffer, sizeof(buffer), "%s: %s", params->daemon, format);
#ifndef NO_DAEMON
   if (params->detached != 0)
      vsyslog(LOG_INFO, buffer, ap);
   else
#endif
   vfprintf(stderr, buffer, ap);
   va_end(ap);
}
/**
* Run as application writing to a file
*/
static void run_app(       /* RETURN: nothing         */
   H_PTR h,                /* IN: app instance        */
   H_UINT bufct,           /* IN: # buffers to fill   */
   H_UINT bufres)          /* IN: # bytes extra       */
{
   H_UINT   *buffer;
   FILE     *fout = NULL;
   H_UINT    ct=0;
   int       limits = bufct;

   if (0 != havege_run(h))
      error_exit("Couldn't initialize HAVEGE rng %d", h->error);
   if (0 != (params->setup & USE_STDOUT)) {
      params->sample_out = "stdout";
      fout = stdout;
      }
   else if (!(fout = fopen (params->sample_out, "wb")))
      error_exit("Cannot open file <%s> for writing.\n", params->sample_out);
   limits = bufct!=0? 1 : bufres != 0;
   buffer = (H_UINT *)h->io_buf;
   if (limits)
      fprintf(stderr, "Writing %s output to %s\n",
         ppSize((char *)buffer, (1.0 * bufct) * APP_BUFF_SIZE * sizeof(H_UINT) + bufres), params->sample_out);
   else fprintf(stderr, "Writing unlimited bytes to stdout\n");
   while(!limits || ct++ < bufct) {
      if (havege_rng(h, buffer, APP_BUFF_SIZE)<1)
         error_exit("RNG failed %d!", h->error);
      if (fwrite (buffer, 1, APP_BUFF_SIZE * sizeof(H_UINT), fout) == 0)
         error_exit("Cannot write data in file: %s", strerror(errno));
   }
   ct = (bufres + sizeof(H_UINT) - 1)/sizeof(H_UINT);
   if (ct) {
      if (havege_rng(h, buffer, ct)<1)
         error_exit("RNG failed %d!", h->error);
      if (fwrite (buffer, 1, bufres, fout) == 0)
         error_exit("Cannot write data in file: %s", strerror(errno));
      }
   fclose(fout);
   if (params->verbose & H_VERBOSE)
      anchor_info(h);
}
/**
 * Show collection info.
 */
static void show_meterInfo(      /* RETURN: nothing   */
   H_UINT id,                    /* IN: identifier    */
   H_UINT event)                 /* IN: start/stop    */
{
   struct timeval tm;
   /* N.B. if multiple thread, each child gets its own copy of this */
   static H_METER status;

   gettimeofday(&tm, NULL);
   if (event == 0)
      status.estart = ((double)tm.tv_sec*1000.0 + (double)tm.tv_usec/1000.0);
   else {
      status.etime  = ((double)tm.tv_sec*1000.0 + (double)tm.tv_usec/1000.0);
      if ((status.etime -= status.estart)<0.0)
         status.etime=0.0;
      status.n_fill += 1;
      print_msg("%d fill %g ms\n", id, status.etime);
      }
}
/**
 * Signal handler
 */
static void tidy_exit(           /* OUT: nothing      */
   int signum)                   /* IN: signal number */
{
   error_exit("Stopping due to signal %d\n", signum);
}
/**
 * send usage display to stderr
 */
static void usage(               /* OUT: nothing            */
   int nopts,                    /* IN: number of options   */
   struct option *long_options,  /* IN: long options        */
   const char **cmds)            /* IN: associated text     */
{
   int i, j;
   
   fprintf(stderr, "\nUsage: %s [options]\n\n", params->daemon);
#ifndef NO_DAEMON
   fprintf(stderr, "Collect entropy and feed into random pool or write to file.\n");
#else
   fprintf(stderr, "Collect entropy and write to file.\n");
#endif
   fprintf(stderr, "  Options:\n");
   for(i=j=0;long_options[i].val != 0;i++,j+=4) {
      while(cmds[j][0] != long_options[i].val && (j+4) < (nopts * 4))
         j += 4;
      fprintf(stderr,"     --%-10s, -%c %s %s\n",
         long_options[i].name, long_options[i].val,
         long_options[i].has_arg? "[]":"  ",cmds[j+3]);
      }
   fprintf(stderr, "\n");
   exit(1);
}
