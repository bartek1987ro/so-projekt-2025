#!/bin/bash

#  Czyszczenie zasobów IPC po nieudanym uruchomieniu


echo "Czyszczenie zasobow IPC..."

ME=`whoami`

IPCS_S=`ipcs -s | egrep "0x[0-9a-f]+ [0-9]+" | grep $ME | cut -f2 -d" "`
IPCS_M=`ipcs -m | egrep "0x[0-9a-f]+ [0-9]+" | grep $ME | cut -f2 -d" "`
IPCS_Q=`ipcs -q | egrep "0x[0-9a-f]+ [0-9]+" | grep $ME | cut -f2 -d" "`

for id in $IPCS_M; do
    echo "  Usuwam shm: $id"
    ipcrm -m $id 2>/dev/null
done

for id in $IPCS_S; do
    echo "  Usuwam sem: $id"
    ipcrm -s $id 2>/dev/null
done

for id in $IPCS_Q; do
    echo "  Usuwam msg: $id"
    ipcrm -q $id 2>/dev/null
done

# Zabij ewentualne wiszące procesy
killall -u $ME dispatcher.out worker.out truck.out p4_express.out 2>/dev/null

echo "Gotowe."
