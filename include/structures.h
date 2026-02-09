#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <sys/types.h>
#include "config.h"


// Typy przesyłek
#define PKG_TYPE_A  0
#define PKG_TYPE_B  1
#define PKG_TYPE_C  2


// Przesyłka
typedef struct {
    int    type;
    float  weight;
    float  volume;
    pid_t  worker_pid;    // PID pracownika
    int    express;       // 1 = ekspresowa
    int    id;            // Numer kolejny
} package_t;


// Taśma transportowa (pamięć dzielona)
typedef struct {
    package_t   packages[BELT_CAPACITY_K];
    int         head;
    int         tail;
    int         count;
    float       current_weight;
    int         next_id;
    int         shutdown;
    /* Wymiana paczek ekspresowych P4 <-> ciężarówka */
    package_t   express_pkg;       // paczka ekspresowa do załadunku
    int         express_waiting;   // 1 = paczka gotowa do załadunku
    int         express_remaining; // ile jeszcze paczek P4 chce dostarczyć
} belt_t;


//Stan ciężarówki (pamięć dzielona)
#define TRUCK_FREE       0
#define TRUCK_AT_RAMP    1
#define TRUCK_ON_ROUTE   2

typedef struct {
    pid_t   pid;
    int     status;
    float   current_weight;
    float   current_volume;
    int     package_count;
    int     courses;
    int     total_packages;
} truck_state_t;


//Komunikat w kolejce komunikatów

typedef struct {
    long    mtype;
    pid_t   sender_pid;
    int     data;
} msg_t;

#endif /* STRUCTURES_H */