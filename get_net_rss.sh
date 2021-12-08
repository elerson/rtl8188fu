#!/bin/bash
mac_addresses=(1c:bf:ce:44:2f:bb 1c:bf:ce:27:3b:62 1c:bf:ce:26:ee:f2 1c:bf:ce:52:1b:ba 1c:bf:ce:27:65:fe 1c:bf:ce:27:2b:d1 1c:bf:ce:27:2a:0e 1c:bf:ce:27:d1:7b)

for mac in ${mac_addresses[@]}; do
  iw wlan1 station get $mac | grep -E 'Station|signal|inactive'
done
