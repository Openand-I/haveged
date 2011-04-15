#!/bin/sh
cd ent
./entest -t
../src/haveged -r 16384 -v 1 $*
./entest -vf sample
