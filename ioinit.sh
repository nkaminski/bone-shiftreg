#!/bin/sh

cd /sys/class/gpio

#export and set as outputs
echo "110" > export
echo "out" > gpio110/direction
echo "111" > export
echo "out" > gpio111/direction
echo "112" > export
echo "out" > gpio112/direction
echo "113" > export
echo "out" > gpio113/direction
