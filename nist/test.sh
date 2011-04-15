#!/bin/sh
cd nist
../src/haveged -r16384 $*
./nist sample
