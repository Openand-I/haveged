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
 **
 ** This source is an adaptation of work released as
 **
 ** Copyright (C) 2006 - André Seznec - Olivier Rochecouste
 **
 ** under version 2.1 of the GNU Lesser General Public License
 **
 ** Minor changes from HAVENGE original: Removed conditinal wrapper, change to
 ** autoincrement for RESULT updates.
 */

/* ------------------------------------------------------------------------
 * On average, one iteration accesses two 8-word blocks in the havege_walk
 * table, and generates 16 words in the RESULT array.
 *
 * The data read in the Walk table are updated and permuted after each use.
 * The result of the hardware clock counter read is used for this update.
 *
 * 21 conditional tests are present. The conditional tests are grouped in
 * two nested  groups of 10 conditional tests and 1 test that controls the
 * permutation.
 *
 * In average, there should be 4 tests executed and, in average, 2 of them
 * should be mispredicted.
 * ------------------------------------------------------------------------
 */

  PTtest = PT >> 20;

  if (PTtest & 1) {
    PTtest ^= 3; PTtest = PTtest >> 1;
    if (PTtest & 1) {
      PTtest ^= 3; PTtest = PTtest >> 1;
      if (PTtest & 1) {
        PTtest ^= 3; PTtest = PTtest >> 1;
        if (PTtest & 1) {
          PTtest ^= 3; PTtest = PTtest >> 1;
          if (PTtest & 1) {
            PTtest ^= 3; PTtest = PTtest >> 1;
            if (PTtest & 1) {
              PTtest ^= 3; PTtest = PTtest >> 1;
              if (PTtest & 1) {
                PTtest ^= 3; PTtest = PTtest >> 1;
                if (PTtest & 1) {
                  PTtest ^= 3; PTtest = PTtest >> 1;
                  if (PTtest & 1) {
                    PTtest ^= 3; PTtest = PTtest >> 1;
                    if (PTtest & 1) {
                      PTtest ^= 3; PTtest = PTtest >> 1;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  };

  PTtest = PTtest >> 1;
  pt = (PT >> 18) & 7;

  PT = PT & ANDPT;

  HARDCLOCK(havege_hardtick);

  Pt0 = &havege_pwalk[PT];
  Pt1 = &havege_pwalk[PT2];
  Pt2 = &havege_pwalk[PT  ^ 1];
  Pt3 = &havege_pwalk[PT2 ^ 4];

  RESULT[i++] ^= *Pt0;
  RESULT[i++] ^= *Pt1;
  RESULT[i++] ^= *Pt2;
  RESULT[i++] ^= *Pt3;

  inter = (*Pt0 >> (1)) ^ (*Pt0 << (31)) ^ havege_hardtick;
  *Pt0  = (*Pt1 >> (2)) ^ (*Pt1 << (30)) ^ havege_hardtick;
  *Pt1  = inter;
  *Pt2  = (*Pt2 >> (3)) ^ (*Pt2 << (29)) ^ havege_hardtick;
  *Pt3  = (*Pt3 >> (4)) ^ (*Pt3 << (28)) ^ havege_hardtick;

  Pt0 = &havege_pwalk[PT  ^ 2];
  Pt1 = &havege_pwalk[PT2 ^ 2];
  Pt2 = &havege_pwalk[PT  ^ 3];
  Pt3 = &havege_pwalk[PT2 ^ 6];

  RESULT[i++] ^= *Pt0;
  RESULT[i++] ^= *Pt1;
  RESULT[i++] ^= *Pt2;
  RESULT[i++] ^= *Pt3;

  if (PTtest & 1) {
    volatile int *Ptinter;
    Ptinter = Pt0;
    Pt2 = Pt0;
    Pt0 = Ptinter;
  }

  PTtest = (PT2 >> 18);
  inter  = (*Pt0 >> (5)) ^ (*Pt0 << (27)) ^ havege_hardtick;
  *Pt0   = (*Pt1 >> (6)) ^ (*Pt1 << (26)) ^ havege_hardtick;
  *Pt1   = inter;

  HARDCLOCK(havege_hardtick);

  *Pt2 = (*Pt2 >> (7)) ^ (*Pt2 << (25)) ^ havege_hardtick;
  *Pt3 = (*Pt3 >> (8)) ^ (*Pt3 << (24)) ^ havege_hardtick;

  Pt0 = &havege_pwalk[PT  ^ 4];
  Pt1 = &havege_pwalk[PT2 ^ 1];

  PT2 = (RESULT[(i - 8) ^ pt2] ^ havege_pwalk[PT2 ^ pt2 ^ 7]);
  PT2 = ((PT2 & ANDPT) & (0xfffffff7)) ^ ((PT ^ 8) & 0x8);

  /* avoid PT and PT2 to point on the same cache block */
  pt2 = ((PT2 >> 28) & 7);

  if (PTtest & 1) {
    PTtest ^= 3; PTtest = PTtest >> 1;
    if (PTtest & 1) {
      PTtest ^= 3; PTtest = PTtest >> 1;
      if (PTtest & 1) {
        PTtest ^= 3; PTtest = PTtest >> 1;
        if (PTtest & 1) {
          PTtest ^= 3; PTtest = PTtest >> 1;
          if (PTtest & 1) {
            PTtest ^= 3; PTtest = PTtest >> 1;
            if (PTtest & 1) {
              PTtest ^= 3; PTtest = PTtest >> 1;
              if (PTtest & 1) {
                PTtest ^= 3; PTtest = PTtest >> 1;
                if (PTtest & 1) {
                  PTtest ^= 3; PTtest = PTtest >> 1;
                  if (PTtest & 1) {
                    PTtest ^= 3; PTtest = PTtest >> 1;
                    if (PTtest & 1) {
                      PTtest ^= 3; PTtest = PTtest >> 1;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  };

  Pt2 = &havege_pwalk[PT  ^ 5];
  Pt3 = &havege_pwalk[PT2 ^ 5];

  RESULT[i++] ^= *Pt0;
  RESULT[i++] ^= *Pt1;
  RESULT[i++] ^= *Pt2;
  RESULT[i++] ^= *Pt3;

  inter = (*Pt0 >> (9))  ^ (*Pt0 << (23)) ^ havege_hardtick;
  *Pt0  = (*Pt1 >> (10)) ^ (*Pt1 << (22)) ^ havege_hardtick;
  *Pt1  = inter;
  *Pt2  = (*Pt2 >> (11)) ^ (*Pt2 << (21)) ^ havege_hardtick;
  *Pt3  = (*Pt3 >> (12)) ^ (*Pt3 << (20)) ^ havege_hardtick;

  Pt0 = &havege_pwalk[PT  ^ 6];
  Pt1 = &havege_pwalk[PT2 ^ 3];
  Pt2 = &havege_pwalk[PT  ^ 7];
  Pt3 = &havege_pwalk[PT2 ^ 7];

  RESULT[i++] ^= *Pt0;
  RESULT[i++] ^= *Pt1;
  RESULT[i++] ^= *Pt2;
  RESULT[i++] ^= *Pt3;

  inter = (*Pt0 >> (13)) ^ (*Pt0 << (19)) ^ havege_hardtick;
  *Pt0  = (*Pt1 >> (14)) ^ (*Pt1 << (18)) ^ havege_hardtick;
  *Pt1  = inter;
  *Pt2  = (*Pt2 >> (15)) ^ (*Pt2 << (17)) ^ havege_hardtick;
  *Pt3  = (*Pt3 >> (16)) ^ (*Pt3 << (16)) ^ havege_hardtick;

  /* avoid PT and PT2 to point on the same cache block */
  PT = (((RESULT[(i - 8) ^ pt] ^ havege_pwalk[PT ^ pt ^ 7])) &
        (0xffffffef)) ^ ((PT2 ^ 0x10) & 0x10);
