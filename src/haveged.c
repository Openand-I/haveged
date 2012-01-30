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
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
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
  .daemon         = "haveged",
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
  .sample_out     = "sample",
  .sample_size    = 1024,
  .verbose        = 0,
  .version        = "1.3",
  .watermark      = "/proc/sys/kernel/random/write_wakeup_threshold"
  };
struct pparams *params = &defaults;
/**
 * Local Prototypes
 */
#ifndef NO_DAEMON
static void daemonize();
static void exec_daemon(U_INT poolsize,struct rand_pool_info *output, U_INT *buffer);
static int  get_poolsize(void);
static void run_daemon(void);
static void set_watermark(int level);
#endif

static void error_exit(const char *format, ...);
static void get_info();
static int  get_runsize(unsigned int *bufct, unsigned int *bufrem, char *bp);
static char *ppSize(char *buffer, double sz);

static void run_app(U_INT bufct, U_INT bufres);
static void tidy_exit(int signum);
static void usage(int nopts, struct option *long_options, const char **cmds);

#ifndef NO_DAEMON
/**
 * The usual daemon setup
 */
static void daemonize()
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
static void exec_daemon(U_INT poolsize, struct rand_pool_info *output, U_INT *buffer)
{
   H_RDR h       = havege_state();
   int random_fd = -1;
   int fills     = 0;

   if (params->foreground==0)
     daemonize();
   else printf ("%s starting up\n", params->daemon);
   if (params->verbose & VERBOSE)
     get_info();
   if (params->low_water>0)
      set_watermark(params->low_water);
   random_fd = open(params->random_device, O_RDWR);
   if (random_fd == -1)
     error_exit("Couldn't open random device: %m");

   for(;;) {
      int current,i,nbytes,r;

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
      // get number of bytes needed to fill pool
      nbytes = (poolsize  - current)/8;
      if(nbytes<1)   continue;
      // get that many random bytes
      r = (nbytes+sizeof(U_INT)-1)/sizeof(U_INT);
      for(i=0;i<r;i++)
         buffer[i] = ndrand();
      output->buf_size = nbytes;
      // entropy is 8 bits per byte
      output->entropy_count = nbytes * 8;
      memcpy(output->buf, buffer, output->buf_size);
      if (ioctl(random_fd, RNDADDENTROPY, output) == -1)
         error_exit("RNDADDENTROPY failed!");
      if ((params->verbose & VERBOSE)!=0 && fills != h->havege_fills) {
         fills = h->havege_fills;
         if (params->detached!=0)
            syslog(LOG_INFO, "%s fill %d us", params->daemon, h->etime);
         else printf("%s fill %d us\n", params->daemon, h->etime);
         }
      }
}
/**
 * Get configured poolsize in bits.
 */
static int get_poolsize(void)
{
   FILE *poolsize_fh,*osrel_fh;
   unsigned int max_bits,major,minor;

   if (params->run_level==0) {
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
      }
   else max_bits = 1024 * 8;
   if (max_bits < 1)
      error_exit("Couldn't get poolsize");
   return max_bits;
}
/**
 * Final daemon setup
 */
static void run_daemon(void)
{
   int poolsize;

   poolsize = get_poolsize();
   if (0 == params->run_level) {
      unsigned int  nbytes = (unsigned)poolsize/8;
      unsigned int  buf[(nbytes+sizeof(int)-1)/sizeof(int)];
      char rb[sizeof(struct rand_pool_info)+nbytes+2];
      
      exec_daemon(poolsize,(struct rand_pool_info *)rb,buf);
      }
   else get_info();
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
static void get_info()
{
char buf[2048];

havege_status(buf);
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
   // hack to allow 16t
   ct -= 1;
   *bufrem = (U_INT)(ct%p10);
   *bufct  = (U_INT)(ct/p10);
   return (*bufct == (ct/p10))? 0 : 4;
}
/**
 * Entry point
 */
int main(int argc, char **argv)
{
   static const char* cmds[] = {
      "d", "data",      "1", "Data cache size [KB]",
      "i", "inst",      "1", "Instruction cache size [KB]",
      "f", "file",      "1", "Sample output file,  default: 'sample', '-' for stdout",
#ifndef NO_DAEMON
      "F", "Foreground","1", "0=background daemon,!=0 remain attached",
      "r", "run",       "1", "0=daemon, 1=config info, >1=<r>KB sample",
      "n", "number",    "1", "Same as r=2 with size in [k|m|g|t] bytes, 0 unlimited (to stdout)",
#else
      "n", "number",    "1", "Output size in [k|m|g|t] bytes, 0 = unlimited (implies stdout)",
#endif
      "v", "verbose",   "1", "Output level 0=minimal,1=config/fill info",
#ifndef NO_DAEMON
      "w", "write",     "1", "Set write_wakeup_threshold [bits]",
#endif
      "h", "help",      "0", "This help"
      };
   static int nopts = sizeof(cmds)/(4*sizeof(char *));
   struct option long_options[nopts];
   char short_options[1+nopts*2];
   int c,i,j;
   U_INT bufct, bufrem, bufarg;

#ifdef SIGHUP
   signal(SIGHUP, tidy_exit);
#endif
   signal(SIGINT, tidy_exit);
   signal(SIGTERM, tidy_exit);
   strcpy(short_options,"");
   bufct  = bufrem = bufarg = 0;
   for(i=j=0;i<nopts;i++,j+=4) {
      long_options[i].name      = cmds[j+1];
      long_options[i].has_arg   = atoi(cmds[j+2]);
      long_options[i].flag      = NULL;
      long_options[i].val       = cmds[j][0];
      strcat(short_options,cmds[j]);
      if (long_options[i].has_arg!=0) strcat(short_options,":");
      }
   do {
      c = getopt_long (argc, argv, short_options, long_options, NULL);
      switch(c) {
#ifndef NO_DAEMON
         case 'F':
            params->foreground = atoi(optarg);
            break;
#endif
         case 'd':
            params->d_cache = atoi(optarg);
            break;
         case 'i':
            params->i_cache = atoi(optarg);
            break;
         case 'f':
            if (strcmp(optarg,"-") == 0 )
              params->write_to_stdout = 1;
            else
              params->sample_out = optarg;
            break;
         case 'n':
            if (get_runsize(&bufct, &bufrem, optarg))
               error_exit("invalid count: %s", optarg);
            params->run_level = 2;
            bufarg = 1;
            if (0 == bufct && 0 == bufrem)
              params->write_to_stdout = 1;
            break;
#ifndef NO_DAEMON
         case 'r':
            if (bufarg != 0)
               break;
            params->run_level  = atoi(optarg);
            if (params->run_level>1)
               params->sample_size = params->run_level * 1024;
            break;
#endif
         case 'v':
            params->verbose  = atoi(optarg);
            break;
         case 'w':
            params->low_water = atoi(optarg);
            break;
         case '?':
         case 'h':
            usage(nopts, long_options, cmds);
         case -1:
            break;
         }
   } while (c!=-1);

   if (!havege_init(params->i_cache, params->d_cache, params->verbose))
      error_exit("Couldn't initialize HAVEGE rng");
   switch(params->run_level) {
#ifndef NO_DAEMON
      case 0:  case 1:
         run_daemon();
         break;
#endif
      default:
         if (bufarg==0)
            usage(nopts, long_options, cmds);
         run_app(bufct, bufrem);
         break;
      }
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
static void run_app(U_INT bufct, U_INT bufres)
{
   U_INT     buffer[APP_BUFF_SIZE];
   FILE     *fout = NULL;
   H_RDR     h = havege_state();
   U_INT     ct=0;
   int       fills=0;
   int       limits = bufct;
   U_INT     i;

   if (params->write_to_stdout)
     fout = stdout;
   else if (!(fout = fopen (params->sample_out, "wb")))
      error_exit("Cannot open file <%s> for writing.\n", params->sample_out);
   limits = bufct!=0? 1 : bufres != 0;
   if (limits)
      fprintf(stderr, "Writing %s output to %s\n",
         ppSize((char *)buffer, (1.0 * bufct) * APP_BUFF_SIZE * sizeof(U_INT) + bufres), params->sample_out);
   else fprintf(stderr, "Writing unlimited bytes to stdout\n");
   if (params->verbose & VERBOSE)
      get_info();
   fills = h->havege_fills;
   while(!limits || ct++ < bufct) {
      for(i=0;i<APP_BUFF_SIZE;i++)
         buffer[i] = ndrand();
      if (fwrite (buffer, 1, APP_BUFF_SIZE * sizeof(U_INT), fout) == 0)
         error_exit("Cannot write data in file: %m");
      if ((params->verbose & VERBOSE)!=0 && fills != h->havege_fills) {
         fills = h->havege_fills;
         fprintf(stderr, "%s fill %d us\n", params->daemon, h->etime);
         }
   }
   // number of values to get residue
   ct = (bufres + sizeof(U_INT) - 1)/sizeof(U_INT);
   if (ct) {
      for(i=0;i<ct;i++)
         buffer[i] = ndrand();
      if (fwrite (buffer, 1, bufres, fout) == 0)
         error_exit("Cannot write data in file: %m");
      }
   fclose(fout);
}
/**
 * Signal handler
 */
static void tidy_exit(int signum)
{
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
   int i;
   
   fprintf(stderr, "\nUsage: %s [options]\n\n", params->daemon);
#ifndef NO_DAEMON
   fprintf(stderr, "Collect entropy and feed into random pool or write to file.\n");
#else
   fprintf(stderr, "Collect entropy and write to file.\n");
#endif
   fprintf(stderr, "  Options:\n");
   for(i=0;i<nopts;i++)
      fprintf(stderr,"     --%-10s, -%c %s %s\n",
         long_options[i].name, long_options[i].val,
         long_options[i].has_arg? "[]":"  ",cmds[i*4+3]);
   fprintf(stderr, "\n");
   exit(1);
}
