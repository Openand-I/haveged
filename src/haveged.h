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
 * Settings - default values declared in haveged.c
 */
struct pparams  {
  char  *daemon;          /* Daemon name - default is "haveged"           */
  int   detached;         /* non-zero of daemonized                       */
  int   foreground;       /* non-zero if running in foreground            */
  int   run_level;        /* type of run 0=daemon,1=setup,2=pip,sample kb */
  int   d_cache;          /* size of data cache (kb)                      */
  int   i_cache;          /* size of instruction cache (kb)               */
  int   low_water;        /* write threshold to set - 0 for none          */
  char  *os_rel;          /* path to operating sytem release              */
  char  *pid_file;        /* name of pid file                             */
  char  *poolsize;        /* path to poolsize                             */
  char  *random_device;   /* path to random device                        */
  char  *sample_out;      /* path to sample file                          */
  int   sample_size;      /* size of sample (kb)                          */
  int   verbose;          /* Output level for log or stdout               */
  char  *version;         /* Our version                                  */
  char  *watermark;       /* path to write_wakeup_threshold               */
  int   write_to_stdout;  /* write random bytes directly to stdout        */   
  };
/**
 * Buffer size used when not running as daemon
 */
#define   APP_BUFF_SIZE 1024
