#!/bin/bash

# run script with sudo

# make executable
#chmod +x setup_dpdk.sh 

echo 'Disabling interfaces, setting up dpdk...'

ifconfig enp104s0f0 down
ifconfig enp104s0f1 down
ifconfig enp103s0f0 down
ifconfig enp103s0f1 down

modprobe vfio-pci

/usr/share/dpdk/usertools/dpdk-devbind.py --bind=vfio-pci enp104s0f0
/usr/share/dpdk/usertools/dpdk-devbind.py --bind=vfio-pci enp104s0f1
/usr/share/dpdk/usertools/dpdk-devbind.py --bind=vfio-pci enp103s0f0
/usr/share/dpdk/usertools/dpdk-devbind.py --bind=vfio-pci enp103s0f1

echo 'Done!'