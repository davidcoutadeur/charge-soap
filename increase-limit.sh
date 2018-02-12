#!/bin/bash -x

# maximum system threads
echo 1000000 > /proc/sys/kernel/threads-max

# maximum open files
ulimit -n 1048576

# maximum user processes
ulimit -u 1048576
