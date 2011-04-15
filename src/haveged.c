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
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <asm/types.h>
#include <linux/random.h>
#include <errno.h>

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
  .version        = "1.01",
  .watermark      = "/proc/sys/kernel/random/write_wakeup_threshold"
  };
struct pparams *params = &defaults;
/**
 * Prototypes
 */
void daemonize(struct hperf *perf);
void error_exit(const char *format, ...);
void get_info(struct hperf *perf);
int  get_poolsize(void);
void run(int poolsize,struct rand_pool_info *output, int *buffer, struct hperf *perf);
void set_watermark(int level);
void tidy_exit(int signum);
/**
 * The usual daemon setup
 */
void daemonize(struct hperf *perf)
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
 * Bail....
 */
void error_exit(const char *format, ...)
{
   char buffer[4096];
   va_list ap;
   va_start(ap, format);
   vsnprintf(buffer, sizeof(buffer), format, ap);
   va_end(ap);
   if (params->detached!=0) {
      unlink(params->pid_file);
      syslog(LOG_INFO, "%s %s", params->daemon, buffer);
      }
   else fprintf(stderr, "%s %s\n", params->daemon, buffer);
   exit(1);
}
/**
 * Dump static information about the HAVEGE rng
 */
void get_info(struct hperf *perf)
{
const char *fmt =
   "%s Configuration\n"
   "arch:     %s\n"
   "vendor:   %s %s\n"
   "i_cache:  %d kb\n"
   "d_cache:  %d kb\n"
   "loop_idx: %d\n"
   "loop_sz:  %d bytes\n";
const char *fmt2 =
   "1st fill: %d us\n";
struct hinfo h;
char * tune;

ndinfo(&h);
tune = h.generic? "USING GENERIC TUNING":"";
if (params->detached!=0) {
   syslog(LOG_INFO, fmt,params->daemon,h.arch,h.vendor,tune,h.i_cache,h.d_cache,h.loop_idx,h.loop_sz);
   if (perf!=NULL) {
      perf->fill = 0;
      syslog(LOG_INFO, fmt2 ,perf->etime);
      }
   }
else {
  printf(fmt,params->daemon,h.arch,h.vendor,tune,h.i_cache,h.d_cache,h.loop_idx,h.loop_sz);
  if (perf!=NULL) {
    perf->fill = 0;
    printf(fmt2 , perf->etime);
    }
  }
}
/**
 * Get configured poolsize in bits.
 */
int get_poolsize(void)
{
   FILE *poolsize_fh,*osrel_fh;
   int max_bits,major,minor;

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
   return max_bits;
}
/**
 * Entry point
 */
int main(int argc, char **argv)
{
   static const char* cmds[] = {
      "d", "data",      "1", "Data cache size [KB]",
      "i", "inst",      "1", "Instruction cache size [KB]",
      "f", "file",      "1", "Sample output file - default: 'sample'",
      "F", "Foreground","1", "0=background daemon,!=0 remain attached",
      "r", "run",       "1", "0=daemon,1=config info,>1=Write <r>KB sample file",
      "v", "verbose",   "1", "Output level 0=minimal,1=config/fill items",
      "w", "write",     "1", "Set write_wakeup_threshold [BITS]",
      "h", "help",      "0", "This help"
      };
   static int nopts = sizeof(cmds)/(4*sizeof(char *));
   struct option long_options[nopts];
   char short_options[1+nopts*2];
   int c,i,j,poolsize;

   signal(SIGHUP, tidy_exit);
   signal(SIGINT, tidy_exit);
   signal(SIGTERM, tidy_exit);
   strcpy(short_options,"");
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
         case 'F':
            params->foreground = atoi(optarg);
            break;
         case 'd':
            params->d_cache = atoi(optarg);
            break;
         case 'i':
            params->i_cache = atoi(optarg);
            break;
         case 'f':
            params->sample_out = optarg;
         case 'r':
            params->run_level  = atoi(optarg);
            if (params->run_level>1)
               params->sample_size = params->run_level;
            break;
         case 'v':
            params->verbose  = atoi(optarg);
            break;
         case 'w':
            params->low_water = atoi(optarg);
            break;
         case '?':
         case 'h':
            fprintf(stderr, "\nUsage: %s [options]\n\n", params->daemon);
            fprintf(stderr, "Collect entropy and feed into random pool.\n");
            fprintf(stderr, "  Options:\n");
            for(i=0;i<nopts;i++)
               fprintf(stderr,"     --%-10s, -%c %s %s\n",
                  long_options[i].name, long_options[i].val,
                  long_options[i].has_arg? "[]":"  ",cmds[i*4+3]);
            fprintf(stderr, "\n");
            exit(1);
         case -1:
            break;
         }
   } while (c!=-1);
   poolsize = get_poolsize();
   if (poolsize>0) {
      struct hperf hpf, *hp = NULL;
      int nbytes = poolsize/8;
      if (params->verbose>0) {
         hpf.fill = 0;
         hp = &hpf;
         }
      int  buf[(nbytes+sizeof(int)-1)/sizeof(int)];
      char rb[sizeof(struct rand_pool_info)+nbytes+2];
      run(poolsize,(struct rand_pool_info *)rb,buf,hp);
      }
   else error_exit("Couldn't get poolsize");
   exit(0);
}
/**
 * The meat
 */
void run(int poolsize, struct rand_pool_info *output, int *buffer, struct hperf *perf)
{
   FILE *fout = NULL;
   int random_fd = -1;
   int ct=0,ft=1;

   if (!ndinit(params,perf)) {
      params->run_level = 1;
      error_exit("Couldn't initialize HAVEGE rng");
      }
   switch(params->run_level) {
      case 0:
         if (params->foreground==0)
            daemonize(perf);
         else printf ("%s starting up\n", params->daemon);
         if (params->verbose>0)
         get_info(perf);
         if (params->low_water>0)
            set_watermark(params->low_water);
         random_fd = open(params->random_device, O_RDWR);
         if (random_fd == -1)
            error_exit("Couldn't open random device: %m");
         break;
      case 1:
         get_info(perf);
         return;
      default:
         ct = params->sample_size*1024;
         if (!(fout = fopen (params->sample_out, "wb")))
            error_exit("Cannot open file <%s> for writing.\n", params->sample_out);
         fprintf(stderr, "Writing %d byte sample\n",ct);
      }
   for(;;) {
      int current,i,nbytes,r;
      if (params->run_level==0) {
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
         nbytes = (poolsize  - current)/8;
         }
      else {
         if (ft) {
            get_info(perf);
            ft=0;
            }
         if (ct<=0) {
            fclose(fout);
            break;
            }
         nbytes = poolsize/8;
         ct -= nbytes;
         }
      if(nbytes<1)   continue;
      r = (nbytes+sizeof(int)-1)/sizeof(int);
      for(i=0;i<r;i++)
         buffer[i] = ndrand(perf);
      output->buf_size = nbytes;
      output->entropy_count = nbytes * 8;
      memcpy(output->buf, buffer, output->buf_size);
      if (params->run_level==0) {
         if (ioctl(random_fd, RNDADDENTROPY, output) == -1)
            error_exit("RNDADDENTROPY failed!");
         }
      else {
         if (fwrite (output->buf, 1, output->buf_size, fout) == 0)
            error_exit("Cannot write data in file: %m");
         }
      if (perf!=NULL && perf->fill!=0) {
         if (params->detached!=0)
            syslog(LOG_INFO, "%s fill %d us", params->daemon, perf->etime);
         else printf("%s fill %d us\n", params->daemon, perf->etime);
         perf->fill = 0;
         }
      }
}
/**
 * Set random write threshold
 */
void set_watermark(int level)
{
   FILE *wm_fh;

   wm_fh = fopen(params->watermark, "w");
   if (wm_fh) {
      fprintf(wm_fh, "%d\n", level);
      fclose(wm_fh);
      }
   else error_exit("Fail:set_watermark()!");
}
/**
 * Signal handler
 */
void tidy_exit(int signum)
{
   if (params->detached!=0) {
      unlink(params->pid_file);
      syslog(LOG_NOTICE, "%s stopping due to signal %d\n", params->daemon, signum);
      }
   else fprintf(stderr, "%s stopping due to signal %d\n", params->daemon, signum);
   exit(0);
}
