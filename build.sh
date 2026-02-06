#!/bin/bash
# ============================================================
#  Magazyn firmy spedycyjnej – kompilacja i czyszczenie IPC
# ============================================================

echo "=== Kompilacja ==="

gcc src/dispatcher.c   -o dispatcher.out   -Wall -Wextra
gcc src/worker.c       -o worker.out       -Wall -Wextra
gcc src/truck.c        -o truck.out        -Wall -Wextra
gcc src/p4_express.c   -o p4_express.out   -Wall -Wextra

if [ $? -eq 0 ]; then
    echo "Kompilacja zakonczona pomyslnie."
else
    echo "BLAD kompilacji!"
    exit 1
fi

# ============================================================
#  Czyszczenie starych zasobów IPC (semafory, shm, kolejki)
# ============================================================
echo ""
echo "=== Czyszczenie starych zasobow IPC ==="

ME=`whoami`

IPCS_S=`ipcs -s | egrep "0x[0-9a-f]+ [0-9]+" | grep $ME | cut -f2 -d" "`
IPCS_M=`ipcs -m | egrep "0x[0-9a-f]+ [0-9]+" | grep $ME | cut -f2 -d" "`
IPCS_Q=`ipcs -q | egrep "0x[0-9a-f]+ [0-9]+" | grep $ME | cut -f2 -d" "`

for id in $IPCS_M; do
    ipcrm -m $id 2>/dev/null
done

for id in $IPCS_S; do
    ipcrm -s $id 2>/dev/null
done

for id in $IPCS_Q; do
    ipcrm -q $id 2>/dev/null
done

echo "IPC wyczyszczone."
echo ""
echo "=== Gotowe! ==="
echo "Uruchomienie:  ./dispatcher.out"
echo "Lub z limitem: ./dispatcher.out 50"
