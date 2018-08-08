#!/bin/sh
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#


. /etc/include.properties   
. /etc/device.properties
. /etc/env_setup.sh

. $RDK_PATH/commonUtils.sh
. $RDK_PATH/interfaceCalls.sh

Timestamp()
{
    date +"%Y-%m-%d %T"
}

check=0
loop=1
cnt=0
sleep 10

startmonitor=0

while [ $startmonitor -eq 0 ]
do
    id=`pidof mpeos-main|wc -w`
    if [ $id -eq 0 ]; then
         sleep 5
    else
         startmonitor=1
         echo "Started mpeos, starting monitoring..!"
    fi
done

while [ $loop -eq 1 ]
do

# Check the MPEOS MAIN process
ret=`processCheck mpeos-main`
if [ "$ret" == "1" ]; then
    echo "mpeos process is killed.."
      check=`expr $check + 1`
      if [ $check -gt 6 ]; then
	  if [ -f /opt/.reboot ]; then
	      r=`cat /opt/.reboot`
	  else
	      r=0
	  fi
          r=`expr $r + 1`
          if [ $r -le 10 ]; then
               echo " mpeos-main is not alive.. rebooting the box"
               echo $r > /opt/.reboot
               echo `Timestamp` 'Rebooting the box'>> $LOG_PATH/ocapri_log.txt
               echo 0 > /opt/.rebootFlag
               echo `/bin/timestamp` ------------ Rebooting due to mpeos crash ----------------- >> $LOG_PATH/ocapri_log.txt
               if [ "$HDD_ENABLED" = "false" ]; then
                    if [[ `haveCoreToUpload` == 'true' ]] ; then
                             waitForDumpCompletion 300
                             TS=`date +%Y-%m-%d-%H-%M-%S`
                             sh $RDK_PATH/uploadDumps.sh $TS 1
                    fi
                    fileName=`ls $CORE_PATH/*core.prog_mpeos-main.signal_*.gz`
	                if [ -f $fileName ] ; then
                         echo `ls $CORE_PATH/*mpeos-main* -tc1 | head -1` > /opt/corefiles_back/.mpeos_crashed
                    fi
               else
                    cp $LOG_PATH/receiver.log $LOG_PATH/receiver.log_mpeos-main
                    cp $LOG_PATH/ocapri_log.txt $LOG_PATH/ocapri_log.txt_mpeos-main
                    cp $LOG_PATH/messages.txt $LOG_PATH/messages.txt_mpeos-main
                    cp $LOG_PATH/app_status.log $LOG_PATH/app_status_backup.log_mpeos-main
               fi
               /rebootNow.sh
               exit 0                                                                
          else                                                                      
               echo  `/bin/timestamp` -----Box has rebooted 10 times.. no more reboot---- >> /opt/logs/ocapri_log.txt
	       exit 1
          fi
     fi
 fi

 sleep 15
 cnt=`expr $cnt + 1`
 if [ $cnt -eq 80 ]; then
    echo 0 > /opt/.reboot
 fi
done
