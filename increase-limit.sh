#!/bin/bash -x

echo 1000000 > /proc/sys/kernel/threads-max
ulimit -n 1048576
