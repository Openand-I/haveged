#!/bin/sh
cd nist
../src/haveged -n 16M $*
./nist sample
