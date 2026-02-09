#ifndef CONFIG_H
#define CONFIG_H

// Magazyn firmy spedycyjnej – parametry konfiguracyjne

// Tasma transportowa
#define BELT_CAPACITY_K     10      // Maks. przesylek na taśmie
#define BELT_MAX_WEIGHT_M   120.0   // Maks. udzwig tasmy [kg]

// Ciezarowki
#define TRUCK_COUNT_N       3       // Liczba ciezarowek
#define TRUCK_LOAD_W        150.0   // Ladownsc [kg]
#define TRUCK_VOLUME_V      1.0     // Objetość ładunkowa [m3]
#define TRUCK_MIN_COURSE    1       // Min. czas kursu [s]
#define TRUCK_MAX_COURSE    5      // Maks. czas kursu [s]

// Gabaryt A: 64×38×8
#define VOLUME_A            0.019456
#define WEIGHT_MIN_A        0.1
#define WEIGHT_MAX_A        8.0

// Gabaryt B: 64×38×19 cm
#define VOLUME_B            0.046208
#define WEIGHT_MIN_B        0.1
#define WEIGHT_MAX_B        16.0

// Gabaryt C: 64×38×41 cm
#define VOLUME_C            0.099712
#define WEIGHT_MIN_C        0.1
#define WEIGHT_MAX_C        25.0

// Przesyłki ekspresowe
#define EXPRESS_MIN_COUNT   1
#define EXPRESS_MAX_COUNT   3

// Tryb pracy
#define MAX_PACKAGES        100     // Paczek na pracownika (0 = ∞)


// Indeksy semaforów w zbiorze
#define SEM_BELT_SLOTS      0   /* Wolne miejsca na taśmie (init=K)       */
#define SEM_BELT_ITEMS      1   /* Paczki czekające na taśmie (init=0)    */
#define SEM_BELT_MUTEX      2   /* Mutex na bufor taśmy                   */
#define SEM_WEIGHT_MUTEX    3   /* Mutex na wagi                          */
#define SEM_WEIGHT_FREED    4   /* Sygnalizacja zwolnienia wagi (init=0)  */
#define SEM_RAMP_MUTEX      5   /* Mutex: 1 ciężarówka przy rampie        */
#define SEM_LOG_MUTEX       6   /* Mutex na plik logów                    */
#define SEM_TRUCK_MUTEX     7   /* Mutex na stan ciężarówek               */
#define SEM_EXPRESS_READY   8   /* P4→ciężarówka: ekspres gotowy          */
#define SEM_EXPRESS_DONE    9   /* Ciężarówka→P4: załadowano              */
#define SEM_RAMP_READY      10  /* Dyspozytor→ciężarówka: podjedź         */
#define SEM_COUNT           11

// Klucze IPC (znaki dla ftok)
#define FTOK_SEM            'S'
#define FTOK_SHM_BELT       'B'
#define FTOK_SHM_TRUCKS     'T'
#define FTOK_MSGQ           'M'

// Typy komunikatów w kolejce
#define MSG_TRUCK_FULL       1
#define MSG_TRUCK_RETURNED   2
#define MSG_EXPRESS_DELIVERY 3
#define MSG_SHUTDOWN         4

#endif /* CONFIG_H */
