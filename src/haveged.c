/**
 ** Simple entropy harvester based upon the havege RNG
 **
 ** Copyright 2009-2012 Gary Wuertz gary@issiweb.com
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
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>

#ifndef NO_DAEMON
#include <syslog.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/random.h>
#endif

#include <errno.h>
#include "haveged.h"
#include "havege.h"
/**
 * Parameters
 */
static struct pparams defaults = {
  .daemon         = PACKAGE,
  .setup          = 0,
  .ncores         = 1,
  .buffersz       = NDSIZECOLLECT,
  .detached       = 0,
  .foreground     = 0,
  .d_cache        = 0,
  .i_cache        = 0,
  .run_level      = 0,
  .low_water      = 0,
  .os_rel         = "/proc/sys/kernel/osrelease",
  .pid_file       = "/var/run/haveged.pid",
  .poolsize       = "/proc/sys/kernel/random/poolsize",
  .random_device  = "/dev/random",
  .sample_in      = "data",
  .sample_out     = "sample",
  .verbose        = 0,
  .version        = PACKAGE_VERSION,
  .watermark      = "/proc/sys/kernel/random/write_wakeup_threshold"
  };
struct pparams *params = &defaults;

/**
 * Local prototypes
 */
#ifndef NO_DAEMON
static void daemonize(void);
static int  get_poolsize(void);
static void run_daemon(H_PTR handle);
static void set_watermark(int level);
#endif

static void error_exit(const char *format, ...);
static void get_info(H_PTR handle);
static int  get_runsize(unsigned int *bufct, unsigned int *bufrem, char *bp);
static char *ppSize(char *buffer, double sz);

static void run_app(H_PTR handle, U_INT bufct, U_INT bufres);
static void show_meterInfo(U_INT id, U_INT event);
static void tidy_exit(int signum);
static void usage(int nopts, struct option *long_options, const char **cmds);

#ifndef NO_DAEMON
/**
 * The usual daemon setup
 */
static void daemonize(void)
{
   FILE *fh;
   openlog(params->daemon, LOG_CONS, LOG_DAEMON);
   syslog(LOG_NOTICE, "%s starting up", params->daemon);
   if (daemon(0, 0) == -1)
      error_exit("Cannot fork into the background");
   fh = fopen(params->pid_file, "w");
   if (!fh)
      error_exit("Couldn't open PID file \"%s\" for writing: %m.", params->pid_file);
   fprintf(fh, "%i", getpid());
   fclose(fh);
   params->detached = 1;
}
/**
 * Run as a daemon writing to random device entropy pool
 */
static void run_daemon(H_PTR h)
{
   int                     random_fd = -1;
   struct rand_pool_info   *output;

   if (0 != params->run_level) {
      get_info(h);
      return;
      }
   if (params->foreground==0)
     daemonize();
   else printf ("%s starting up\n", params->daemon);
   if (0 != havege_create(h))
      error_exit("Couldn't initialize HAVEGE rng %d", h->error);
   if (params->verbose & VERBOSE)
     get_info(h);
   if (params->low_water>0)
      set_watermark(params->low_water);
   random_fd = open(params->random_device, O_RDWR);
   if (random_fd == -1)
     error_exit("Couldn't open random device: %m");

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
            error_exit("Select error: %m");
         }
      if (ioctl(random_fd, RNDGETENTCNT, &current) == -1)
         error_exit("Couldn't query entropy-level from kernel");
      /* get number of bytes needed to fill pool */
      nbytes = (h->params->poolsize  - current)/8;
      if(nbytes<1)   continue;
      /* get that many random bytes */
      r = (nbytes+sizeof(U_INT)-1)/sizeof(U_INT);
      if (havege_rng(h, (U_INT *)output->buf, r)<1)
         error_exit("havege_rng failed! %d", h->error);
      output->buf_size = nbytes;
      /* entropy is 8 bits per byte */
      output->entropy_count = nbytes * 8;
      if (ioctl(random_fd, RNDADDENTROPY, output) == -1)
         error_exit("RNDADDENTROPY failed!");
      }
}
/**
 * Get configured poolsize in bits.
 */
static int get_poolsize(void)
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
         }
      if (major==2 && minor==4) max_bits *= 8;
      }
   else max_bits = -1;
   if (max_bits < 1)
      error_exit("Couldn't get poolsize");
   return max_bits;
}
/**
 * Set random write threshold
 */
static void set_watermark(int level)
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
 * Bail....
 */
static void error_exit(const char *format, ...)
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
   exit(1);
}
/**
 * Dump static information about the HAVEGE rng
 */
static void get_info(H_PTR handle)
{
char buf[2048];

havege_status(handle, buf, sizeof(buf));
#ifndef NO_DAEMON
if (params->detached != 0)
   syslog(LOG_INFO, "%s\n", buf);
else
#endif
   fprintf(stderr, "%s\n", buf);
}
/**
 * Implement fixed point shorthand for run sizes
 */
static int get_runsize(U_INT *bufct, U_INT *bufrem, char *bp)
{
   char        *suffix;
   double      f;
   int         p2 = 0;
   int         p10 = APP_BUFF_SIZE * sizeof(U_INT);
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
   *bufrem = (U_INT)(ct%p10);
   *bufct  = (U_INT)(ct/p10);
   if (*bufct == (ct/p10))
      return 0;
   /* hack to allow 16t */
   ct -= 1;
   *bufrem = (U_INT)(ct%p10);
   *bufct  = (U_INT)(ct/p10);
   return (*bufct == (ct/p10))? 0 : 4;
}
#ifdef  RAW_IN_ENABLE

FILE *fd_in;
/**
 * The injection diagnostic
 */
static int injectFile(volatile U_INT *pData, U_INT szData)
{
   if (fread((void *)pData, sizeof(U_INT), szData, fd_in) != szData)
      error_exit("Cannot read data in file: %m");
   return 0;
}
#endif

#define  ATOU(a)     (unsigned int)atoi(a)
/**
 * Entry point
 */
int main(int argc, char **argv)
{
   static const char* cmds[] = {
      "b", "buffer",    "1", "Buffer size [KB], default: 128",
      "d", "data",      "1", "Data cache size [KB]",
      "i", "inst",      "1", "Instruction cache size [KB]",
      "f", "file",      "1", "Sample output file,  default: 'sample', '-' for stdout",
      "F", "Foreground","1", "0=background daemon,!=0 remain attached",
      "r", "run",       "1", "0=daemon, 1=config info, >1=<r>KB sample",
      "n", "number",    "1", "Output size in [k|m|g|t] bytes, 0 = unlimited (if stdout)",
      "s", "source",    "1", "Injection soure file",
      "t", "threads",   "1", "Number of threads",
      "v", "verbose",   "1", "Output level 0=minimal,1=config/fill info",
      "w", "write",     "1", "Set write_wakeup_threshold [bits]",
      "h", "help",      "0", "This help"
      };
   static int nopts = sizeof(cmds)/(4*sizeof(char *));
   struct option long_options[nopts+1];
   char short_options[1+nopts*2];
   int c,i,j;
   U_INT bufct, bufrem;
   H_PTR handle;
   CMD_PARAMS cmd;

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

#ifdef SIGHUP
   signal(SIGHUP, tidy_exit);
#endif
   signal(SIGINT, tidy_exit);
   signal(SIGTERM, tidy_exit);
   strcpy(short_options,"");
   bufct  = bufrem = 0;

   for(i=j=0;j<(nopts*4);j+=4) {
      switch(cmds[j][0]) {
         case 'r':
            if (0!=(params->setup & (INJECT|CAPTURE))) {
               params->daemon = "havege_diagnostic";
               cmds[j+3] = "0=usage, 1=config_info, 2=capture, 4=inject";
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
         case 'w':   case 'F':
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
            params->foreground = ATOU(optarg);
            break;
         case 'b':
            params->buffersz = ATOU(optarg) * 1024;
            break;
         case 'd':
            params->d_cache = ATOU(optarg);
            break;
         case 'i':
            params->i_cache = ATOU(optarg);
            break;
         case 'f':
            if (strcmp(optarg,"-") == 0 )
              params->setup |= USE_STDOUT;
            else
              params->sample_out = optarg;
            break;
         case 'n':
            if (get_runsize(&bufct, &bufrem, optarg))
               error_exit("invalid count: %s", optarg);
            params->setup |= RUN_AS_APP | RANGE_SPEC;
            if (bufct==0 && bufrem==0)
               params->setup |= USE_STDOUT;
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
   memset(&cmd, 0, sizeof(CMD_PARAMS));
   cmd.collectSize = params->buffersz;
   cmd.icacheSize  = params->i_cache;
   cmd.dcacheSize  = params->d_cache;
   cmd.options     = params->verbose & 0xff;
   cmd.nCores      = params->ncores;
   cmd.version     = params->version;
   if (0 != (params->setup & RUN_AS_APP))
      cmd.ioSz = APP_BUFF_SIZE * sizeof(U_INT);
#ifndef NO_DAEMON
   else  {
      cmd.poolsize = get_poolsize();
      i = (cmd.poolsize + 7)/8 * sizeof(U_INT);
      cmd.ioSz = sizeof(struct rand_pool_info) + i *sizeof(U_INT);
      }
#endif
   if (0 != (params->verbose & DEBUG_TIME))
      cmd.metering = show_meterInfo;
   if (0 !=(params->setup & CAPTURE) && 0 != (params->run_level & 2))
      cmd.options |= DEBUG_RAW_OUT;
#ifdef  RAW_IN_ENABLE
  if (0 !=(params->setup & INJECT) && 0 != (params->run_level & 4)) {
      fd_in = fopen(params->sample_in, "rb");
      if (NULL == fd_in)
         error_exit("Unable to open: %s", params->sample_in);
      cmd.options |= DEBUG_RAW_IN;
      cmd.injection = injectFile;
      }
#endif
   handle = havege_init(&cmd);
   if (H_NOERR != handle->error)
      error_exit("Couldn't initialize haveged (%d)", handle->error);
   if (0 != (params->setup & RUN_AS_APP)) {
      if (0 == (params->setup & RANGE_SPEC)) {
         if (params->run_level > 1)
            if (0!=(params->setup & (INJECT|CAPTURE)))
              usage(nopts, long_options, cmds);
            else run_app(handle,
               params->run_level/sizeof(U_INT),
              (params->run_level%sizeof(U_INT))*1024
              );
         else if (params->run_level==1 || 0 != (params->verbose & 1))
            get_info(handle);
         else usage(nopts, long_options, cmds);
         }
      else run_app(handle, bufct, bufrem);
      }
#ifndef NO_DAEMON
   else run_daemon(handle);
#endif
   havege_exit(0);
   exit(0);
}
/**
 * Pretty print the collection size
 */
static char *ppSize(char *buffer, double sz)
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
* Run as application writing to a file
*/
static void run_app(H_PTR h, U_INT bufct, U_INT bufres)
{
   U_INT    *buffer;
   FILE     *fout = NULL;
   U_INT     ct=0;
   int       limits = bufct;

   if (0 != havege_create(h))
      error_exit("Couldn't initialize HAVEGE rng %d", h->error);
   if (0 != (params->setup & USE_STDOUT)) {
      params->sample_out = "stdout";
      fout = stdout;
      }
   else if (!(fout = fopen (params->sample_out, "wb")))
      error_exit("Cannot open file <%s> for writing.\n", params->sample_out);
   limits = bufct!=0? 1 : bufres != 0;
   buffer = (U_INT *)h->io_buf;
   if (limits)
      fprintf(stderr, "Writing %s output to %s\n",
         ppSize((char *)buffer, (1.0 * bufct) * APP_BUFF_SIZE * sizeof(U_INT) + bufres), params->sample_out);
   else fprintf(stderr, "Writing unlimited bytes to stdout\n");
   if (params->verbose & VERBOSE)
      get_info(h);
   while(!limits || ct++ < bufct) {
      if (havege_rng(h, buffer, APP_BUFF_SIZE)<1)
         error_exit("havege_rng failed %d!", h->error);
      if (fwrite (buffer, 1, APP_BUFF_SIZE * sizeof(U_INT), fout) == 0)
         error_exit("Cannot write data in file: %m");
   }
   ct = (bufres + sizeof(U_INT) - 1)/sizeof(U_INT);
   if (ct) {
      if (havege_rng(h, buffer, ct)<1)
         error_exit("havege_rng failed %d!", h->error);
      if (fwrite (buffer, 1, bufres, fout) == 0)
         error_exit("Cannot write data in file: %m");
      }
   fclose(fout);
}
/**
 * Show collection info.
 */
static void show_meterInfo(U_INT id, U_INT event)
{
   struct timeval tm;
   /* N.B. if multiple thread, each child gets its own copy of this */
   static H_STATUS status;

   gettimeofday(&tm, NULL);
   if (event == 0) {
      status.tmp_sec = tm.tv_sec;
      status.tmp_usec = tm.tv_usec;
      }
   else {
      status.etime =  (tm.tv_sec - status.tmp_sec)*1000000 +
                              tm.tv_usec - status.tmp_usec;
      status.n_fill += 1;
#ifndef NO_DAEMON
      if (params->detached!=0)
         syslog(LOG_INFO, "%s:%d fill %d us\n", params->daemon, id, status.etime);
      else
#endif
      if (params->run_level > 0)
         fprintf(stderr, "%s:%d fill %d us\n", params->daemon, id, status.etime);
      else printf("%s:%d fill %d us\n", params->daemon, id, status.etime);
      }
}
/**
 * Signal handler
 */
static void tidy_exit(int signum)
{
   havege_exit(0);
#ifndef NO_DAEMON
   if (params->detached!=0) {
      unlink(params->pid_file);
      syslog(LOG_NOTICE, "%s stopping due to signal %d\n", params->daemon, signum);
      }
   else
#endif
   fprintf(stderr, "%s stopping due to signal %d\n", params->daemon, signum);
   exit(0);
}
/**
 * usage
 */
static void usage(int nopts, struct option *long_options, const char **cmds)
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
