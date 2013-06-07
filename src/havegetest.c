/**
 ** Simple entropy harvester based upon the havege RNG
 **
 ** Copyright 2013 Gary Wuertz gary@issiweb.com
 ** Copyright 2012 BenEleventh Consulting manolson@beneleventh.com
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
 * This compile unit implements online tests for haveged through the public functions tests*.
 * Online tests are run directly against the contents of the collection buffer immediately after
 * a buffer fill. Because collection buffer size does not have any direct relationship with
 * the data requirements of the individual tests, all tests implement a state machine to
 * handle segmented input.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "havegetest.h"

#ifdef ONLINE_TESTS_ENABLE
/**
 * The tests and supporting methods
 */
static H_UINT aisBitTest(procB *context, H_UINT n);
static void   aisFail(H_COLLECT * h_ctxt, H_UINT prod);
static H_UINT aisProcedureA(procShared *tps, procA *context, H_UINT *buffer, H_UINT sz);
static H_UINT aisProcedureB(procShared *tps, procB *context, H_UINT *buffer, H_UINT sz);
static void   aisReportA(H_PTR h, procA *context);
static void   aisReportB(H_PTR h, procB *context);
static H_UINT aisSeq(procB *p, H_UINT offs, H_UINT id);
static H_UINT aisTest(H_COLLECT * h_ctxt, H_UINT prod, H_UINT *buffer, H_UINT sz);
static H_UINT fips140(procShared *tps, procA *p, H_UINT offs, H_UINT id);
static H_UINT test0(procA *p, H_UINT offs, H_UINT id);
static int    test0cmp(const void *aa, const void *bb);
static H_UINT test5(procA *p, H_UINT offs, H_UINT id);
static H_UINT test5XOR(H_UINT8 *src, H_UINT shift);
static H_UINT test6a(procB *p, H_UINT offs, H_UINT id);
static H_UINT test8(procShared *tps, procB *p, H_UINT offs, H_UINT id);

static int   testsConfigure(H_UINT *tot, H_UINT *run, char *options);
static int   testsDiscard(H_COLLECT *rdr);
static void  testsStatus(H_PTR h, char *tot, char *prod);
static int   testsRun(H_COLLECT *rdr, H_UINT prod);
/**
 * Data shared by all test instances
 */
static const H_UINT seq_bitx[4] = {1,3,15,255};

/**
 * Setup shared resources for online tests by sorting the test options into "tot"
 * and production groupings and allocating any resources used by the tests.
 */
int   havege_test(         /* RETURN: nz on failure   */
  H_PTR h,                 /* IN-OUT: app state       */
  H_PARAMS *params)        /* IN: app parameters      */
{
   procShared  *tps = (procShared *)h->testData;
   H_UINT tests[4] = {B_RUN, B_OPTIONS, A_RUN, A_OPTIONS};
   H_UINT tot=0;           /* OUT: tot test options      */
   H_UINT run=0;           /* OUT: run test options      */
   H_UINT i, j;
   
   if (testsConfigure(&tot, &run, params->testSpec)) {
      h->error = H_NOTESTSPEC;
      return 1;
      }
   tps->discard   = testsDiscard;
   tps->run       = testsRun;
   tps->options   = params->options;
   
   for(i=j=0;i<2;i++)
      if (0!=(tot & tests[i*2])) {
         tps->testsUsed |= tests[i*2];
         tps->totTests[j].action  = tests[i*2];
         tps->totTests[j++].options = tests[1+(i*2)] & tot;
         }
   for(i=j=0;i<2;i++)
      if (0!=(run & tests[i*2])) {
         tps->testsUsed |= tests[i*2];
         tps->runTests[j].action  = tests[i*2];
         tps->runTests[j++].options = tests[1+(i*2)] & run;
         }
   testsStatus(h, tps->totText, tps->prodText);
   if (0!=(tps->testsUsed & A_RUN)) {
      H_UINT low[6]  = {FIPS_RUNS_LOW};   
      H_UINT high[6] = {FIPS_RUNS_HIGH};

      tps->procReps  = 1 + (5 * AIS_A_REPS);
      for (i=0;i<6;i++) {
         tps->fips_low[i]  = low[i];
         tps->fips_high[i] = high[i];
         }
      }
   if (0!=(tps->testsUsed & B_RUN)) {
      tps->G = (double *) malloc((Q+K+1)*sizeof(double));
      if (0 == tps->G) {
         h->error = H_NOTESTMEM;
         return 1;
         }
      tps->G[1] = 0.0;
      for(i=1; i<=(K+Q-1); i++)
         tps->G[i+1]=tps->G[i]+1.0/i;
      for(i=1; i<=(K+Q); i++)
         tps->G[i] /= LN2;
      }
   return 0;
}
/**
 * Interpret options string as settings. The option string consists of terms
 * like "[t|c][a[1-8][w]|b[w]]". This function is called from main() to
 * handle a test specification invocation argument.
 */
static int testsConfigure( /* RETURN: non-zero on error  */
   H_UINT *tot,            /* OUT: tot test options      */
   H_UINT *run,            /* OUT: run test options      */
   char *options)          /* IN: option string          */
{
   H_UINT section=0;
   int   c;

   if (options==0)
      options = DEFAULT_TEST_OPTIONS;
   while(0 != (c = *options++)) {
      switch(c) {
         case 'T': case 't':        /* tot test          */
            section = 't';
            *tot = 0;
            break;
         case 'C': case 'c':        /* production test   */
            section = 'c';
            *run = 0;
            break;
         case 'A': case 'a':
            if (!section) return 1;
            c  = atoi(options);
            if (c >= 1 && c < 9) {
               c = 1<<c;
               options +=1;
               }
            else c = 0;
            c |= A_RUN;
            if (*options=='W' || *options=='w') {
               c |= A_WARN;
               options++;
               }
            if (section=='t')
               *tot |= c;
            else *run |= c;
            break;
         case 'B': case 'b':
            if (!section) return 1;
            c = B_RUN;
            if (*options=='W' || *options=='w') {
               c |= B_WARN;
               options++;
               }
            if (section=='t')
               *tot |= c;
            else *run |= c;
            break;
         default:
            return 1;
         }
   }
   return 0;
}
/**
 * Check if the current buffer should be released if continuous testing is
 * being performed. The buffer must be discarded if it contains an
 * uncompleted retry or an uncompleted procedure with a failed test
 * or a failed procedure.
 */
static int testsDiscard(   /* RETURN: non-zero to discard   */
   H_COLLECT * h_ctxt)     /* IN-OUT: collector context     */
{
   onlineTests *context = (onlineTests *) h_ctxt->havege_tests;
   procShared  *tps = TESTS_SHARED(h_ctxt);
   procInst    *p;
   H_UINT      i;

   if (0==tps->testsUsed)
      return 0;
   if (context->result!=0)
      return -1;
   p = tps->runTests + context->runIdx;
   switch(p->action) {
      case A_RUN:
         if (0 != context->pA->procRetry)
            return 1;
         for (i = 0;i<context->pA->testRun;i++)
            if (0 !=(context->pA->results[i].testResult & 1))
               return 1;
         break;
      case B_RUN:
         if (0 != context->pB->procRetry)
            return 1;
         for (i=0;i<context->pB->testNbr;i++)
            if (0!=(context->pB->results[i].testResult & 0xff))
               return 1;
         break;
      }
   return 0;
}
/**
 * The public wrapper that runs the tests. On the first call, the necessary machinery is built.
 * The calls to aisTest() actually run the tests. The test shared structure is read only in this
 * case, since testsRun could be called in a multi-threaded environment where an onlineTests
 * structure is associated with each collection thread.
 */
static int testsRun(       /* RETURN: nz if input needed    */
   H_COLLECT * h_ctxt,     /* IN-OUT: collector context     */
   H_UINT prod)            /* IN: nz if run, else tot       */
{
   procShared  *tps = TESTS_SHARED(h_ctxt);
   onlineTests *context;

   if (0 ==(tps->testsUsed))
      return 0;
   if (0 == h_ctxt->havege_tests) {
      char  *bp;
      H_UINT sz = sizeof(onlineTests);

      if (0!=(tps->testsUsed & A_RUN))
         sz += sizeof(procA);
      if (0!=(tps->testsUsed & B_RUN))
         sz += sizeof(procB);
      context = (onlineTests *) (bp = (char *) malloc(sz));
      if (NULL==bp) {
         h_ctxt->havege_err = H_NOTESTMEM;
         return 1;
         }
      memset(bp, 0, sizeof(onlineTests));
      bp += sizeof(onlineTests);
      if (0!=(tps->testsUsed & A_RUN)) {
         context->pA = (procA *) bp;
         context->pA->procState = TEST_INIT;
         bp += sizeof(procA);
         }
      if (0!=(tps->testsUsed & B_RUN)) {
         context->pB = (procB *) bp;
         context->pB->procState = TEST_INIT;
         bp += sizeof(procB);
         }
      h_ctxt->havege_tests = context;
      if (0 != (h_ctxt->havege_raw & H_DEBUG_TEST_IN)) return 1;
      }
   return aisTest(h_ctxt, prod, (H_UINT *)h_ctxt->havege_bigarray, h_ctxt->havege_szFill);
}
/**
 * Show test setup. Output strings are [A[N]][B]..
 */
static void testsStatus(    /* RETURN: test config     */
   H_PTR h,                 /* IN: app state           */
   char *tot,               /* OUT: tot tests          */
   char *prod)              /* OUT: production tests   */
{
   procShared  *tps = (procShared *)h->testData;
   procInst    *p;
   char        *dst = tot;
   H_UINT      i, j, k, m;
   
   *dst = *tot = 0;
   p = tps->totTests;
   for(i=0;i<2;i++,p = tps->runTests, dst = prod) {
      for(j=0;j<2;j++,p++) {
         switch(p->action) {
            case A_RUN:
               *dst++ = 'A';
               if (0!=(m = p->options & A_CYCLE)) {
                  for(k=0;m>>=1 != 0;k++);
                  *dst++ = '0' + k;
                  }
               break;
            case B_RUN:
               *dst++ = 'B';
               break;
            }
         *dst = 0;
         }
      }
}
/**
 * Bit testing for segmented bit arrays. N.B. n modulo 8 is either 0, 2, 4
 * so endian not a issue in bit twiddling
 */
static H_UINT aisBitTest(   /* RETURN: 0, 1        */
  procB *context,          /* IN: test context    */
  H_UINT n)                /* IN: bit to test     */
{
   H_UINT mask,word;
 
   mask = 1 << (n % BITS_PER_H_UINT);
   word = n / BITS_PER_H_UINT;

   if (word < context->range && 0 != (context->noise[word] & mask))
      return 1;
   return 0;
}
/**
 * Handle a test failure
 */
static void aisFail(       /* RETURN: nothing               */
   H_COLLECT * h_ctxt,     /* IN-OUT: collector context     */
   H_UINT prod)            /* IN: nz if run, else tot       */
{
   H_PTR       h_ptr = (H_PTR)(h_ctxt->havege_app);
   procShared  *tps = TESTS_SHARED(h_ctxt);
   onlineTests *context = (onlineTests *) h_ctxt->havege_tests;
   procInst    *p;

   if (prod==0)
      p = tps->totTests + context->totIdx;
   else p = tps->runTests + context->runIdx;
   h_ptr->print_msg("AIS-31 procedure %s failed during %s\n", p->action==A_RUN? "A":"B", prod==0? "tot" : "production");
   switch(p->action) {
      case A_RUN:
         tps->meters[prod? H_OLT_PROD_A_F : H_OLT_TOT_A_F] += 1;
         aisReportA(h_ptr,context->pA);
         if (0!=(p->options & A_WARN))
            return;
         break;
      case B_RUN:
         tps->meters[prod? H_OLT_PROD_B_F : H_OLT_TOT_B_F] += 1;
         aisReportB(h_ptr,context->pB);
         if (0!=(p->options & B_WARN))
            return;
         break;
   }
   context->result = p->action;
   h_ctxt->havege_err = prod? H_NOTESTRUN : H_NOTESTTOT;
}
/*
 * AIS-31 test procedure A. The test is initiated by setting procState to TEST_INIT and
 * the test is performed by calling the procedure adding input until completion is indicated
 * by a proc state of TEST_DONE. The first test requires 3145728 bits (393,216 bytes) and
 * the remaining 5 tests are repeated on sucessive 2500 byte samples for 257 times.
 * 
 * An ideal RNG will fail this test with a probablilty of 0.9987. If there is a single failed
 * test, the test will be repeated. An ideal RNG should almost never fail the retry.
 */
static H_UINT aisProcedureA(  /* RETURN: nz for fail     */
   procShared  *tps,          /* IN: shared data         */
   procA *p,                  /* IN: the context         */
   H_UINT *buffer,            /* IN: the input           */
   H_UINT sz)                 /* IN: the input range     */
{
   H_UINT ct = 0, i;

   switch(p->procState) {
      case TEST_INIT:
         p->bytesUsed = 0;
         p->procRetry = 0;
      case TEST_RETRY:
         p->procState = TEST_INPUT;
         p->testState = TEST_INIT;
         p->testId = p->testRun   = 0;
      case TEST_INPUT:
         p->data  = (char *)buffer;
         p->range = sz * sizeof(H_UINT);
         while(p->testRun < tps->procReps) {
            p->testId  = p->testRun<6? p->testRun : (1+(p->testRun-6) % 5);
            switch(p->testId) {
               case 0:
                  ct = test0(p, ct, p->testRun);
                  break;
               case 1:  case 2:  case 3: case 4:
                  ct = fips140(tps, p, ct, p->testRun);
                  break;
               case 5:
                  ct = test5(p, ct, p->testRun);
                  break;
               }
            if (p->testState == TEST_DONE)
               p->testState = TEST_INPUT;
            else if (p->testState == TEST_INPUT)
               return 0;
            }
         p->procState = TEST_EVAL;
      case TEST_EVAL:
         for (ct = i = 0;i<p->testRun;i++)
            ct += p->results[i].testResult & 1;
         if (ct == 1 && p->procRetry ==0) {
            p->procRetry = ct;
            p->procState = TEST_RETRY;
            break;
            }
         p->procState = TEST_DONE;
         break;
      }
   return ct;
}
/*
 * AIS-31 test procedure B. The test is initiated by setting procState to TEST_INIT and
 * the test is performed by calling the procedure adding input until completion is indicated
 * by a proc state of TEST_DONE. Unlike procedure A, the number of input bits is not fixed
 * but depends on the input. The probability that an ideal RNG will fail this test is 0.9998.
 * If a single test fails, the test is repeated. An ideal RNG should almost never fail the
 * retry.
 */
static H_UINT aisProcedureB(  /* RETURN: nz for fail     */
   procShared  *tps,          /* IN: shared data         */
   procB *p,                  /* IN: the context         */
   H_UINT *buffer,            /* IN: the input           */
   H_UINT sz)                 /* IN: the input range     */
{
   H_UINT ct = 0, i;
   
   switch(p->procState) {
      case TEST_INIT:
         p->bitsUsed = 0;
         p->procRetry = 0;
      case TEST_RETRY:
         p->testId = p->testNbr = 0;
         p->procState = TEST_INPUT;
         p->testState = TEST_INIT;
      case TEST_INPUT:
         p->noise = buffer;
         p->range = sz;
         i = p->testId;
         while(p->testState != TEST_DONE && i < 5) {
            p->seq   = 1<<i;
            if (i==4)
               ct = test8(tps,p,ct,i);
            else ct = i==0? test6a(p,ct,i) : aisSeq(p,ct,i);
            if (p->testState == TEST_INPUT)
               return 0;
            p->testId = ++i;
            p->testState = TEST_INIT;
            }
         p->procState = TEST_EVAL;
      case TEST_EVAL:
         for (i=ct=0;i<p->testNbr;i++)
            ct += p->results[i].testResult & 0xff;
         if (ct == 1 && p->procRetry ==0) {
            p->procRetry = ct;
            p->procState = TEST_RETRY;
            break;
            }
         p->procState = TEST_DONE;
         break;
      }
   return ct;
}
/**
 * Reporting unit
 */
static void aisReportA(       /* RETURN: nothing               */
   H_PTR h_ptr,               /* IN-OUT: application instance  */
   procA *p)                  /* IN: proc instance             */
{
   H_UINT ran[6],sum[6];
   H_UINT ct, i, j;
         
   for (i=0;i<6;i++)
      ran[i] = sum[i] = 0;
   for(i=0;i<p->testRun;i++){
      ct = p->results[i].testResult;
      j = ct>>8;
      if (j>5) continue;
      ran[j] += 1;
      if ((ct & 0xff)!=0)
         sum[j] += 1;
      }
   h_ptr->print_msg("procedure A: test0:%d/%d, test1:%d/%d, test2:%d/%d, test3:%d/%d, test4:%d/%d, test5:%d/%d\n",
      ran[0]-sum[0], ran[0],
      ran[1]-sum[1], ran[1],
      ran[2]-sum[2], ran[2],
      ran[3]-sum[3], ran[3],
      ran[4]-sum[4], ran[4],
      ran[5]-sum[5], ran[5]
      );
}
/**
 * Reporting unit
 */
static void aisReportB(       /* RETURN: nothing               */
   H_PTR h_ptr,               /* IN-OUT: application instance  */
   procB *p)                  /* IN: proc instance             */
{
   H_UINT ct,i,sum[5];

   for (i=0;i<5;i++) sum[i] = 1;
   for(i=0;i<p->testNbr;i++){
      ct = p->results[i].testResult * 1;
      sum[ct>>8] -= ct & 1;
      }
   h_ptr->print_msg("procedure B: test6a:%d/1, test6b:%d/1, test7a:%d/1, test7b:%d/1, test8: %d/1\n",
      sum[0], sum[1], sum[2], sum[3], sum[4]       
      );
}
/**
 * Driver for disjoint sequence tests - steps 2,3,4 of AIS-31 procedure B (aka test6b, test7a, and test7b).
 * The 'seq' member of procB is the width of the bit tuple to be analyzed: test6b==2; test7a==4;test7b==8;
 * For a tuple of width n, transition probabilities are calculated for log2(n) transitions for the first
 * 100000 sequences. The deadman timer prevents total runaways with pathalogical input - if we process
 * an entire sequence of bits without a transition then something is wrong.
 */
#define  DEADMAN  p->lastpos[0]

static H_UINT aisSeq(      /* RETURN: last bit index  */
   procB *p,               /* IN: the context         */
   H_UINT offs,            /* IN: starting offset     */
   H_UINT tid)             /* IN: test id             */
{
   H_UINT   r, i=0, j, hilf;
   H_UINT   uln2, umsk2;

   switch(p->testState) {
      case TEST_INIT:
         DEADMAN = 0;
         p->bridge = p->full = 0;
         for(i=0;i<p->seq;i++)
            p->counter[i] = p->einsen[i] = 0;
         p->testState = TEST_INPUT;
      case TEST_INPUT:
         for(uln2=0,i=p->seq;i>>=1;uln2++)   ;
         umsk2 = seq_bitx[uln2];
         r = (p->range * BITS_PER_H_UINT) - offs;
         DEADMAN += r;
         for (i=0;i < r && p->testState==TEST_INPUT; i += p->seq) {
            for (hilf=p->bridge&15,j=p->bridge>>4;j<uln2 && (i+j)<r;j++)
               hilf += hilf + aisBitTest(p, i+offs+j);
            if (j<uln2) {
               p->bridge = hilf | (j<<4);
               break;                           /* need a few more bits */
               }
            if (0==(p->full & (1<<hilf))) {
               if ((i+j)>=r) {
                  p->bridge = hilf | (j<<4);
                  break;                        /* need 1 more bit      */
                  }
               p->counter[hilf] += 1;
               if (p->counter[hilf]==AIS_LENGTH) {
                  p->full |= 1<<hilf;
                  if (p->full == umsk2)
                     p->testState = TEST_EVAL;
                  }
               DEADMAN = 0;
               p->einsen[hilf] += aisBitTest(p, i+offs+j);
               }
            p->bridge = 0;
            }
         p->bitsUsed += i;
         if (p->testState != TEST_EVAL && DEADMAN<AIS_LENGTH) break;
      case TEST_EVAL:
         if (p->seq==2) {                       /* special evaluation for step 2 */
            double q[2];

            for(j=0;j<2;j++)
               q[j] = (double) p->einsen[j] / (double) AIS_LENGTH;
            p->results[p->testNbr].finalValue = q[0] - q[1];
            hilf = tid << 8;
            if (p->results[p->testNbr].finalValue <= -0.02 || p->results[p->testNbr].finalValue >= 0.02)
               hilf |= 1;
            p->results[p->testNbr++].testResult = hilf;
            p->testState = TEST_DONE;
            break;
            }
         for(j=0; j<p->seq; j+=2) {             /* the full package           */
            double qn = (double)((int)p->einsen[j] - (int)p->einsen[j+1]); 
            double qd = (double)(p->einsen[j] + p->einsen[j+1]);
            double pn = (double)((int)p->einsen[j+1] - (int)p->einsen[j]);
            double pd = AIS_LENGTH * 2.0 - qd;
            p->results[p->testNbr].finalValue = ((pn * pn) / pd) + ((qn * qn) / qd);
            hilf = tid << 8;
            if (p->results[p->testNbr].finalValue > 15.13)
               hilf |= 1;
            p->results[p->testNbr++].testResult = hilf;
            }
         p->testState = TEST_DONE;
     }
   return i+offs;
}
/**
 * Run the configured test procedures. This function cycles the procedure calls
 * setup by the configuration using tail recursion to sequence multiple tests.
 */
static H_UINT aisTest(     /* RETURN: nz if input needed    */
   H_COLLECT * h_ctxt,     /* IN-OUT: collector context     */
   H_UINT prod,            /* IN: production indicator      */
   H_UINT *buffer,         /* IN: data to test              */
   H_UINT sz)              /* IN: size of data in H_UINT    */
{
   H_PTR       h_ptr = (H_PTR)(h_ctxt->havege_app);
   procShared  *tps = TESTS_SHARED(h_ctxt);
   onlineTests *context = (onlineTests *) h_ctxt->havege_tests;
   procInst   *p;
   H_UINT ct, tot=0;

   if (context->result!=0)
      return 0;
   if (prod==0)
      p = tps->totTests + context->totIdx;
   else p = tps->runTests + context->runIdx;

   switch(p->action) {
      case A_RUN:
         if (context->pA->procState==TEST_INIT)
            context->pA->options = p->options;
         ct  = aisProcedureA(tps, context->pA, buffer, sz);
         if (context->pA->procState != TEST_DONE)
            return 1;
         if (ct>0)
            aisFail(h_ctxt, prod);
         tot = context->pA->bytesUsed;
         tps->meters[prod? H_OLT_PROD_A_P : H_OLT_TOT_A_P] += 1;
         if (tps->options & H_DEBUG_OLT) {
            aisReportA(h_ptr,context->pA);
            h_ptr->print_msg("Completed %s procedure A after %d bytes\n", prod? "continuous":"tot", tot);
            }
         break;
      case B_RUN:
         ct = aisProcedureB(tps, context->pB, buffer, sz);
         if (context->pB->procState != TEST_DONE)
            return 1;
         if (ct>0)
            aisFail(h_ctxt, prod);
         tot = context->pB->bitsUsed/8;
         tps->meters[prod? H_OLT_PROD_B_P : H_OLT_TOT_B_P] += 1;
         if (tps->options & H_DEBUG_OLT) {
            aisReportB(h_ptr,context->pB);
            h_ptr->print_msg("Completed %s procedure B after %d bytes\n", prod? "continuous":"tot", tot);
            }
         break;
      default:                                     /* empty test slot */
         break;
      }
   context->szTotal += tot;
   if (prod==0) {
      if (context->totIdx>=1)                      /* check for end of tot          */
         return 0;
      context->totIdx += 1;
      p = tps->totTests + context->totIdx;
      }
   else {
      if (0==tps->runTests[0].action)       /* check for no cont tests       */
         return 0;
      else if (0!=tps->runTests[1].action)  /* check for only 1 cont test    */
         context->runIdx = context->runIdx? 0 : 1;
      p = tps->runTests + context->runIdx;
      }
   switch(p->action) {
      case A_RUN:
         context->pA->procState=TEST_INIT;
         break;
      case B_RUN:
         context->pB->procState=TEST_INIT;
         break;
      }
   tot /= sizeof(H_UINT);
   return tot<sz? aisTest(h_ctxt, prod, buffer+tot, sz - tot) : 1;
}
/**
 * Procedure A tests 1 through 4 correspond to the fips140-1 tests. These tests
 * are conducted on the same input stream, so the calculations can be
 * done in parallel.
 */
#define  FIPS_ADD()  {\
                     if (runLength < 5)\
                        runs[runLength + (6*current)]++;\
                     else runs[5 + (6*current)]++;\
                     }

static H_UINT fips140(     /* RETURN: byte offset     */
   procShared  *tps,       /* IN: shared data         */
   procA *p,               /* IN: the context         */
   H_UINT offs,            /* IN: starting offset     */
   H_UINT tid)             /* IN: test id             */
{
   H_UINT    poker[16];    /* counters for poker test    */
   H_UINT    ones;         /* counter for monbit test    */
   H_UINT    runs[12];     /* counters for runs tests    */
   H_UINT    runLength;    /* current run length         */
   H_UINT    maxRun;       /* largest run encountered    */
   H_UINT    current;      /* current bit index          */
   H_UINT    last;         /* last bit index             */
   H_UINT8  *bp = (H_UINT8 *)p->data + offs;
   H_UINT8  *dp = (H_UINT8 *)p->aux;
   H_UINT    r = p->range - offs;
   H_UINT    c, i=0, j, k;
   
   switch(p->testState) {
      case TEST_INIT:
         p->bridge = 0;
         p->testState = TEST_INPUT;
      /**
       * Use the data in place if possible else use the aux buffer as a bridge
       */
      case TEST_INPUT:
         if ((r + p->bridge) < FIPS_LENGTH) {
            memcpy(dp + p->bridge, bp, r);
            p->bridge += r;
            return offs + r;
            }
         if (p->bridge != 0) {
            memcpy(dp + p->bridge, bp, i = FIPS_LENGTH - p->bridge);
            offs += i;
            p->bridge = 0;
            }
         else {
            dp = bp;
            offs += FIPS_LENGTH;
            }
         p->bytesUsed += FIPS_LENGTH;
      /**
       * Run the tests on contiguous data, set up the chain pointer so that test5 can be run
       * on the same data
       */
      case TEST_EVAL:
         last = 256;
         maxRun = ones = runLength = 0;
         memset(poker, 0, 16*sizeof(H_UINT));
         memset(runs, 0, 12*sizeof(H_UINT));
         p->chain5 = dp;
         for (j=0;j<FIPS_LENGTH;j++) {
            c = dp[j];   
            poker[c>>4] += 1;
            poker[c&15] += 1;
            for(k=0;k<8;k++) {
               current = (c >> (7-k)) & 1;
               ones += current;
               if (current != last) {
                  FIPS_ADD();
                  runLength = 0;
                  last = current;
                  }
               else runLength++;
               }
            }
         FIPS_ADD();
         /* 1 = monobit test  */
         k = (ones >= FIPS_ONES_HIGH || ones <= FIPS_ONES_LOW)? 1 : 0;
         p->results[tid].testResult = k | (1<<8);
         p->results[tid++].finalValue = ones;
         /* 2 = poker test    */
         for(j=k=0;j<16;j++)  k += poker[j]*poker[j];
         j = (k <= FIPS_POKER_LOW || k >= FIPS_POKER_HIGH)? 1 : 0;
         p->results[tid].testResult = j | (2<<8);
         p->results[tid++].finalValue = k;
         /* 3 = runs test     */
         k = 3<<8;
         for(j=0;j<12;j++)
            if (runs[j] < tps->fips_low[j%6] || runs[j] > tps->fips_high[j%6]) {
               k |= 1;
            }
         p->results[tid].testResult = k;
         p->results[tid++].finalValue = 0;
         /* 4 = max run length */
         k = maxRun>=FIPS_MAX_RUN? 1 : 0;
         p->results[tid].testResult = k | (4<<8);
         p->results[tid++].finalValue = maxRun;
         p->testRun = tid;
         p->testState = TEST_DONE;
      }
   return offs;
}
/**
 * Procedure A disjointness test on 48 bit strings.
 */
static H_UINT test0(       /* RETURN: byte offset     */
   procA *p,               /* IN: the context         */
   H_UINT offs,            /* IN: starting offset     */
   H_UINT tid)             /* IN: test id             */
{
   H_UINT i, j, k;

   switch(p->testState) {
      case TEST_INIT:
         p->testState = TEST_INPUT;
         p->bridge = 0;
      /** 
       * this test always uses the aux buffer to collect the test data.
       */
      case TEST_INPUT:
         i = TEST0_USED - p->bridge;      // what's needed
         j = p->range - offs;             // what's available
         k = j<i? j : i;
         memcpy(p->aux+p->bridge, p->data+offs, k);
         offs += k;
         p->bridge += k;
         if (p->bridge<TEST0_USED)
            break;
         p->bytesUsed += TEST0_USED;
         p->bridge = 0;
      case TEST_EVAL:
         qsort(p->aux, TEST0_LENGTH, 6, test0cmp);
         for (i=1,j=0;i<TEST0_LENGTH;i++)
            if (!memcmp(p->aux+(i-1)*6, p->aux+i*6, 6))
               j++;
         p->results[tid].testResult = j;
         p->results[tid++].finalValue = i;
         p->testRun = tid;
         p->testState = TEST_DONE;
      }
   return offs;
}
/**
 * Comparison method for the test0 sort
 */
static int test0cmp(const void *aa, const void *bb)
{
   return memcmp(aa,bb,6);
}
/**
 * Procedure A autocorrelation test. Brutal bit twiddling.
 */
static H_UINT test5(       /* RETURN: byte offset     */
   procA *p,               /* IN: the context         */
   H_UINT offs,            /* IN: starting offset     */
   H_UINT tid)             /* IN: test id             */
{
   H_UINT8 * dp;
   H_UINT j, k, max, tau, Z_tau;

   /**
    * Because this test is so slow it can be skipped on one or more repetitions
    */
   if (0 != (p->options & A_CYCLE)) {
      j = p->options & A_CYCLE;
      if (j==0 || ((tid-1)/5 % j)!=0) {
         p->results[tid++].testResult = 0xff00;
         p->testRun = tid;
         p->testState = TEST_DONE;
         return offs;
         }
      }
   /**
    * This test always uses the same data as test1 through test4
    */
   dp = (H_UINT8 *) p->chain5;
   for (max = k = 0,tau=1;tau<=TEST5_LENGTH;tau++){
      Z_tau = abs(test5XOR(dp, tau) - 2500);
      if (Z_tau > max) {
         max = Z_tau;
         k = tau - 1;
         }
      }
   dp += TEST5_LENGTH/8;
   Z_tau = test5XOR(dp, k + 1);
   j = 5<<8; 
   if (( Z_tau <= 2326) || ( Z_tau >= 2674))
      j  |= 1;
      
   p->results[tid].testResult = j;
   p->results[tid++].finalValue = Z_tau;
   p->testRun = tid;
   p->testState = TEST_DONE;
   return offs;
}
/**
 * The test5 reference implementation looks something like this:
 *
 *   for(i=0,j=shift;i<TEST5_LENGTH;i++,j++)
 *      rv += 1 & (((src[i>>3]>>(i & 7))) ^ ((src[j>>3]>>(j & 7))));
 *   return rv;
 *
 * A high performance optimization using multi-byte casts is 3x as fast as the above but blows up
 * because of alignment issues (leftovers from the test0 implementation)
 * The optimized single byte optimization is 2x as fast as the above but uses no alignment games
 */
static H_UINT test5XOR(H_UINT8 *src, H_UINT shift)
{
   H_UINT8  *src1;
   H_UINT   i,rest, rv;
   
   src1 = src + (shift>>3);
   shift &= 7;
   rest = 8 - shift;
   for(i=rv=0;i<(TEST5_LENGTH>>3);i++) {
      H_UINT8 lw   = *src++;
      H_UINT8 rw   = *src1++;
      H_UINT8 w;

      for (w = (lw & (0xff>>shift)) ^ (rw>>shift);w!=0;w>>=1)
         rv += w & 1;
      for (w = (lw>>rest) ^ (*src1 & (0xff>>rest));w!=0;w>>=1)
         rv += w & 1;
      }
   return rv;
}
/**
 * Procedure B uniform distribution test for parameter set (1,100000,0.25). Very simple, just count bits.
 */
static H_UINT test6a(      /* RETURN: bit offset      */
   procB *p,               /* IN: the context         */
   H_UINT offs,            /* IN: starting offset     */
   H_UINT tid)             /* IN: test id             */
{
   H_UINT r = p->range * BITS_PER_H_UINT - offs;
   H_UINT i=0;

   switch(p->testState) {
      case TEST_INIT:
         p->bridge = p->counter[0] = 0;
         p->testState = TEST_INPUT;
      case TEST_INPUT:
         for(i=0;p->bridge<AIS_LENGTH && i < r;i++,p->bridge++)
            p->counter[0] += aisBitTest(p, i+offs);
         p->bitsUsed += i;
         if (p->bridge<AIS_LENGTH) break;
      case TEST_EVAL:
         p->results[p->testNbr].finalValue = (double) p->counter[0] / (double) AIS_LENGTH;
         r = tid << 8;
         if (p->results[p->testNbr].finalValue <= 0.25 || p->results[p->testNbr].finalValue >= 0.75)
            r |= 1;
         p->results[p->testNbr++].testResult = r;
         p->testState = TEST_DONE;
      }
   return i+offs;
}

#define  TG p->results[p->testNbr].finalValue
/**
 * Procedure B entropy estimator (Coron). Find the distribution of the
 * distance between bytes and their predecessors
 */
static H_UINT test8(       /* RETURN: offset          */
   procShared  *tps,       /* IN: shared data         */
   procB *p,               /* IN: the context         */
   H_UINT offs,            /* IN: starting offset     */
   H_UINT tid)             /* IN: test id             */
{
   H_UINT8  *bp = (H_UINT8 *)p->noise;
   int      r,d,i=0;
   
   switch(p->testState) {
      case TEST_INIT:
         memset(p->lastpos, 0, 256*sizeof(H_UINT));
         TG = 0.0;
         p->bridge = 0;
         p->testState  = TEST_INPUT;
      case TEST_INPUT:
         offs >>= 3;
         bp += offs;
         d = (int) p->bridge;
         r = (int)(p->range*sizeof(H_UINT) - offs + d);
         for(i=d;i<Q && i < r;i++)
            p->lastpos[*bp++] = i;
         for(;i<(K+Q) && i < r;i++) {
            TG += tps->G[i - p->lastpos[*bp]];
            p->lastpos[*bp++] = i;
            }
         p->bitsUsed +=i;
         if (i < (K+Q)) {
            p->bridge = i;
            break;
            }
         p->testState = TEST_EVAL;
      case TEST_EVAL:
         tps->lastCoron = p->results[p->testNbr].finalValue = TG/(double)K;
         r = tid<<8;
         if (p->results[p->testNbr].finalValue <= 7.967)
            r |= 1;
         p->results[p->testNbr++].testResult = r;
         p->testState = TEST_DONE;
      }
   return i<<3;
}
#endif
