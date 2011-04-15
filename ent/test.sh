#!/bin/sh
cd ent
./entest -t
../src/haveged -r 16384 $*
./entest -vf sample
