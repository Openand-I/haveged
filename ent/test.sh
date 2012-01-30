#!/bin/sh
cd ent
./entest -t
../src/haveged -n 16384k -v 1 $*
./entest -vf sample
