#!/bin/bash

VALUE="1048576"

#########################
# Changing values
#########################

# maximum system threads
echo "$VALUE" > /proc/sys/kernel/threads-max

# maximum number of contrack connexions
echo "$VALUE" > /proc/sys/net/ipv4/ip_conntrack_max

# maximum open files
ulimit -n $VALUE

# maximum user processes
ulimit -u $VALUE


#########################
# Verifications
#########################

echo "maximum system threads"
echo -n "value:    "
cat /proc/sys/kernel/threads-max
echo "expected: $VALUE"
echo

echo "maximum number of contrack connexions"
echo -n "value:    "
cat /proc/sys/net/ipv4/ip_conntrack_max
echo "expected: $VALUE"
echo

echo "maximum open files"
echo -n "value:    "
ulimit -n
echo "expected: $VALUE"
echo

echo "maximum user processes"
echo -n "value:    "
ulimit -u
echo "expected: $VALUE"
echo
