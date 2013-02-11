/*
Simple test of build - writes 1k of RNG runs to stdout

gcc -o havege_sample -DUSE_SOURCE -I../../src -Wall havege_sample.c ../../src/.libs/libhavege.a
./havege_sample > /dev/null

After package is installed into test RAM disk /dev/shm/1
gcc -Wall -o havege_sample -I/dev/shm/1/include -L/dev/shm/1/lib havege_sample.c -lhavege
LD_LIBRARY_PATH=/dev/shm/1/lib ./havege_sample > /dev/shm/havege_random



*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#ifdef USE_SOURCE
#include "havege.h"
#else
#include <haveged/havege.h>
#endif

int my_status_dump ( H_PTR h, char *buf, size_t buflen) {
  H_SD_TOPIC topics[4] = {H_SD_TOPIC_BUILD, H_SD_TOPIC_TUNE, H_SD_TOPIC_TEST, H_SD_TOPIC_SUM};
  int ret=0, i;
   
  for(i=0;i<4 && ret<(buflen-2);i++) {
    ret += havege_status_dump(h, topics[i], buf + ret, buflen-ret);
    if (ret>(buflen-2))
      break;
    buf[ret++] = '\n';
    }
  return ret;
}


int main(void) {
  int rc;
  H_UINT* buf;
  H_PARAMS havege_parameters;
  H_PTR havege_state = NULL;
  const int status_buf_size = 8192;
  char status_buf[status_buf_size];
  int i, size;

  memset(&havege_parameters, 0, sizeof(H_PARAMS));
  //havege_parameters.msg_out     = print_msg;
  havege_state = havege_create(&havege_parameters);
  buf = havege_state->io_buf;
  size = havege_state->i_readSz /sizeof(H_UINT);
  rc = havege_state==NULL? H_NOHANDLE : havege_state->error;
  switch(rc) {
    case H_NOERR:
      fprintf(stderr, "havege_create: buffer size is %d\n", havege_state->i_readSz);
      break;
    case H_NOTESTSPEC:
      fprintf(stderr, "ERROR: havege_create: unrecognized test setup: %s", havege_parameters.testSpec);
      break;
    default:
      fprintf(stderr, "ERROR: havege_create has returned %d\n",rc);
      return 1;
  }

  rc = havege_run(havege_state);
  if ( rc ) {
    fprintf(stderr, "ERROR: havege_create has returned %d\n", havege_state->error);
    return 1;
  }

  if ( my_status_dump(havege_state, status_buf, status_buf_size) > 0 )
    fprintf(stderr,"%s\n", status_buf);
  for (i=0;i<1024;i++) {

    rc = havege_rng(havege_state, buf, size);
    if ( rc != (int) size ) {
      fprintf(stderr, "ERROR: havege_rng has returned %d\n", havege_state->error);
      return 1;
    }

    rc = fwrite(buf, 1, size, stdout);
    if ( rc < size ) {
      fprintf(stderr, "ERROR: fwrite\n");
      return 1;
    }
  }

  if ( my_status_dump(havege_state, status_buf, status_buf_size) > 0 )
      fprintf(stderr,"%s\n", status_buf);
  havege_destroy(havege_state);
  return 0;
}


