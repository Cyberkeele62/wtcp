#! /bin/bash
# setup packet forwarder

# 00:22:68:1c:a6:22   my Thinkpad
# 54:42:49:07:f3:39   my laptop
# 44:37:e6:a4:2c:71   Skype-Beta
# 44:37:e6:a4:16:fe   Skype-Alpha
if [ $# -lt 6 ]; then
   echo "Usage : ./setup.sh ingress egress client_mac uplink_trace downlink_trace expt_name"
   exit
fi ;

ingress=$1
egress=$2
client_mac=$3
uplink_trace=$4
downlink_trace=$5
expt=$6
# put both interfaces in promisc mode. 
# Otherwise, the ping requests and responses won't come back correctly. 
set -v
set -x
sudo ifconfig $ingress up promisc
sudo ifconfig $egress up promisc


# Disable segmentation offloading to NIC. 

sudo ethtool --offload  $ingress gso off  tso off gro off  # lro off ufo off   # apparently lro and ufo are not on the cards. 
sudo ethtool --offload  $egress gso off  tso off gro off  # lro off ufo off

# now start the pkt forwarding 

sudo ./traffic-shaping --ingress $ingress --egress $egress --client-mac $client_mac --uplink-trace $uplink_trace --downlink-trace $downlink_trace > $expt.log & 
