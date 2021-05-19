#!/bin/bash

# run script with sudo

# make executable
#chmod +x setup_record.sh 

echo 'Setting net.core.rmem_max and net.core.wmem_max...'

sysctl -w net.core.rmem_max=128000000
sysctl -w net.core.wmem_max=128000000

sysctl -w net.core.rmem_default=128000000
sysctl -w net.core.rmem_default=128000000

echo 'Done!'

# On success, the program returns a zero which is TRUE.
# The ! turns TRUE into a FALSE, so executions stops.
# Therefore, this loop means: Restart if program crashes, end execution if program shutdowns gracefully.
while ! ../build/iqrecorder --args 'type=n3xx,mgmt_addr=192.168.1.156,addr=192.168.10.2,second_addr=192.168.20.2,time_source=internal,clock_source=internal,master_clock_rate=200e6' --rx_channels '0,1' --rx_rate 100e6 --rx_subdev 'A:0 B:0' --rx_delay 0.1
do
    echo "Execution failed. Trying to restart in 10 seconds."
    sleep 10
done

echo 'Done execution of bash script!'