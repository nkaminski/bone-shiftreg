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

#Clocks to zero
echo "0" > gpio111/value
echo "0" > gpio112/value

#pulse clear
echo "0" > gpio113/value
echo "1" > gpio113/value

#wait a sec
sleep 1

while [ 1 -eq 1 ]; do

#shift and latch a one
#set SER
echo "1" > gpio110/value

#pulse clocks
echo "1" > gpio111/value
echo "0" > gpio111/value
echo "1" > gpio112/value
echo "0" > gpio112/value

#wait one second
sleep 1

#shift and latch a zero
#set SER
echo "0" > gpio110/value

#pulse clocks
echo "1" > gpio111/value
echo "0" > gpio111/value
echo "1" > gpio112/value
echo "0" > gpio112/value

#wait one second
sleep 1

done
