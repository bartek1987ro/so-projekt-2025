/*
Dyspozytor – proces główny magazynu
 Użycie:
./dispatcher.out [max_paczek_na_pracownika] */


#include "../include/utils.h"


//Zmienne globalne dyspozytora
static pid_t worker_pids[3] = {0};
static pid_t p4_pid = 0;
static pid_t truck_pids[TRUCK_COUNT_N] = {0};

static int sem_id  = -1;
static int shm_belt_id = -1;
static int shm_trucks_id = -1;
static int msgq_id = -1;

static belt_t *belt = NULL;
static truck_state_t *trucks = NULL;

static volatile sig_atomic_t g_signal1 = 0;  /* SIGUSR1: ciężarówka odjeżdża */
static volatile sig_atomic_t g_signal2 = 0;  /* SIGUSR2: P4 ekspres          */
static volatile sig_atomic_t g_signal3 = 0;  /* SIGINT:  koniec pracy        */

// Kolejka wolnych ciężarówek
static int free_trucks[TRUCK_COUNT_N];
static int free_truck_count = 0;


//Obsługa sygnałów dyspozytora

static void dispatcher_sig_handler(int signum) {
    if (signum == SIGUSR1)      g_signal1 = 1;
    else if (signum == SIGUSR2) g_signal2 = 1;
    else if (signum == SIGINT)  g_signal3 = 1;
}


// Czyszczenie zasobów IPC

static void cleanup(void) {
    if (belt)   shmdt(belt);
    if (trucks) shmdt(trucks);
    if (shm_belt_id >= 0)   shmctl(shm_belt_id, IPC_RMID, NULL);
    if (shm_trucks_id >= 0) shmctl(shm_trucks_id, IPC_RMID, NULL);
    if (msgq_id >= 0)       msgctl(msgq_id, IPC_RMID, NULL);
    if (sem_id >= 0)        semctl(sem_id, 0, IPC_RMID);
}


//  Raport końcowy

static void generate_report(time_t start_time) {
    time_t end_time = time(NULL);
    log_msg("DYSPOZYTOR", "=== RAPORT KONCOWY ===");
    log_msg("DYSPOZYTOR", "Czas trwania: %ld sekund", (long)(end_time - start_time));
    log_msg("DYSPOZYTOR", "--- Ciezarowki ---");
    for (int i = 0; i < TRUCK_COUNT_N; i++) {
        log_msg("DYSPOZYTOR", "  Ciezarowka %d (PID %d): kursow=%d, paczek=%d",
                i, truck_pids[i], trucks[i].courses, trucks[i].total_packages);
    }
    log_msg("DYSPOZYTOR", "Paczek przetworzonych lacznie: ~%d", belt->next_id);
    log_msg("DYSPOZYTOR", "=== KONIEC RAPORTU ===");
}

// Kolejka wolnych ciężarówek
static void enqueue_free_truck(int index) {
    if (free_truck_count < TRUCK_COUNT_N)
        free_trucks[free_truck_count++] = index;
}

static int dequeue_free_truck(void) {
    if (free_truck_count == 0) return -1;
    int idx = free_trucks[0];
    for (int i = 1; i < free_truck_count; i++)
        free_trucks[i - 1] = free_trucks[i];
    free_truck_count--;
    return idx;
}

// Pomocnik: tworzenie shm (czyści stary jeśli istnieje)
static int create_shm(key_t key, size_t size) {
    int id = shmget(key, size, IPC_CREAT | IPC_EXCL | 0600);
    if (id < 0) {
        int old = shmget(key, size, 0600);
        if (old >= 0) shmctl(old, IPC_RMID, NULL);
        id = shmget(key, size, IPC_CREAT | 0600);
    }
    return id;
}

int main(int argc, char *argv[]) {
    time_t start_time = time(NULL);
    srand(start_time);

    int max_pkgs = MAX_PACKAGES;
    if (argc >= 2) max_pkgs = atoi(argv[1]);

    {
        struct tm tm_buf;
        localtime_r(&start_time, &tm_buf);
        strftime(g_log_file, sizeof(g_log_file), "raport_%Y%m%d_%H%M%S.txt", &tm_buf);
        setenv(ENV_LOG_FILE, g_log_file, 1);
    }

    printf("=== Magazyn firmy spedycyjnej ===\n");
    printf("PID dyspozytora: %d\n", getpid());
    printf("Parametry: tasma K=%d M=%.0f kg, ciezarowki N=%d W=%.0f kg V=%.2f m3\n",
           BELT_CAPACITY_K, BELT_MAX_WEIGHT_M, TRUCK_COUNT_N, TRUCK_LOAD_W, TRUCK_VOLUME_V);
    printf("Paczek na pracownika: %d%s\n", max_pkgs, max_pkgs == 0 ? " (nieskonczonosc)" : "");
    printf("Sterowanie:\n");
    printf("  kill -SIGUSR1 %d  -> ciezarowka odjedza z niepelnym\n", getpid());
    printf("  kill -SIGUSR2 %d  -> P4 dostarcza ekspres\n", getpid());
    printf("  kill -SIGINT  %d  -> koniec pracy (lub Ctrl+C)\n", getpid());
    printf("Raport: %s\n\n", g_log_file);


    //Semafory
    key_t sem_key = ftok(".", FTOK_SEM);
    if (sem_key < 0) { perror("ftok sem"); exit(EXIT_FAILURE); }

    sem_id = semget(sem_key, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0600);
    if (sem_id < 0) {
        /* Stary zbiór – usuń i utwórz nowy */
        int old = semget(sem_key, SEM_COUNT, 0600);
        if (old >= 0) semctl(old, 0, IPC_RMID);
        sem_id = semget(sem_key, SEM_COUNT, IPC_CREAT | 0600);
    }
    if (sem_id < 0) { perror("semget"); exit(EXIT_FAILURE); }
    g_sem_id = sem_id;

    // Inicjalizacja semaforów
    if (semctl(sem_id, SEM_BELT_SLOTS,    SETVAL, BELT_CAPACITY_K) < 0 ||
        semctl(sem_id, SEM_BELT_ITEMS,    SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_BELT_MUTEX,    SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_WEIGHT_MUTEX,  SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_WEIGHT_FREED,  SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_RAMP_MUTEX,    SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_LOG_MUTEX,     SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_TRUCK_MUTEX,   SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_EXPRESS_READY, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_EXPRESS_DONE,  SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_RAMP_READY,    SETVAL, 0) < 0) {
        perror("semctl SETVAL"); cleanup(); exit(EXIT_FAILURE);
    }

    // Pamięć dzielona: taśma
    key_t belt_key = ftok(".", FTOK_SHM_BELT);
    shm_belt_id = create_shm(belt_key, sizeof(belt_t));
    if (shm_belt_id < 0) { perror("shmget belt"); cleanup(); exit(EXIT_FAILURE); }
    g_shm_belt_id = shm_belt_id;

    belt = (belt_t *)shmat(shm_belt_id, NULL, 0);
    if (belt == (void *)-1) { perror("shmat belt"); cleanup(); exit(EXIT_FAILURE); }
    memset(belt, 0, sizeof(belt_t));

    // Pamięć dzielona: ciężarówki
    key_t trucks_key = ftok(".", FTOK_SHM_TRUCKS);
    size_t trucks_size = sizeof(truck_state_t) * TRUCK_COUNT_N;
    shm_trucks_id = create_shm(trucks_key, trucks_size);
    if (shm_trucks_id < 0) { perror("shmget trucks"); cleanup(); exit(EXIT_FAILURE); }
    g_shm_trucks_id = shm_trucks_id;

    trucks = (truck_state_t *)shmat(shm_trucks_id, NULL, 0);
    if (trucks == (void *)-1) { perror("shmat trucks"); cleanup(); exit(EXIT_FAILURE); }
    memset(trucks, 0, trucks_size);

    // Kolejka komunikatów
    key_t msgq_key = ftok(".", FTOK_MSGQ);
    msgq_id = msgget(msgq_key, IPC_CREAT | IPC_EXCL | 0600);
    if (msgq_id < 0) {
        int old = msgget(msgq_key, 0600);
        if (old >= 0) msgctl(old, IPC_RMID, NULL);
        msgq_id = msgget(msgq_key, IPC_CREAT | 0600);
    }
    if (msgq_id < 0) { perror("msgget"); cleanup(); exit(EXIT_FAILURE); }
    g_msgq_id = msgq_id;

    // Zmienne środowiskowe dla procesów potomnych
    set_env_int(ENV_SEM_ID, sem_id);
    set_env_int(ENV_SHM_BELT, shm_belt_id);
    set_env_int(ENV_SHM_TRUCKS, shm_trucks_id);
    set_env_int(ENV_MSGQ_ID, msgq_id);

    log_msg("DYSPOZYTOR", "IPC utworzone (sem=%d, belt=%d, trucks=%d, msgq=%d)",
            sem_id, shm_belt_id, shm_trucks_id, msgq_id);


    // Ciężarówki
    for (int i = 0; i < TRUCK_COUNT_N; i++) {
        char idx_str[8];
        snprintf(idx_str, sizeof(idx_str), "%d", i);
        pid_t pid = fork();
        if (pid < 0) { perror("fork truck"); cleanup(); exit(EXIT_FAILURE); }
        if (pid == 0) {
            execl("./truck.out", "truck.out", idx_str, NULL);
            perror("execl truck.out");
            exit(EXIT_FAILURE);
        }
        truck_pids[i] = pid;
        log_msg("DYSPOZYTOR", "Utworzono ciezarowke %d (PID %d)", i, pid);
    }

    // Pracownicy P1-P3
    for (int i = 0; i < 3; i++) {
        char type_str[4], max_str[16];
        snprintf(type_str, sizeof(type_str), "%d", i);
        snprintf(max_str, sizeof(max_str), "%d", max_pkgs);
        pid_t pid = fork();
        if (pid < 0) { perror("fork worker"); cleanup(); exit(EXIT_FAILURE); }
        if (pid == 0) {
            execl("./worker.out", "worker.out", type_str, max_str, NULL);
            perror("execl worker.out");
            exit(EXIT_FAILURE);
        }
        worker_pids[i] = pid;
        log_msg("DYSPOZYTOR", "Utworzono pracownika P%d (PID %d)", i + 1, pid);
    }

    // P4 ekspres
    {
        pid_t pid = fork();
        if (pid < 0) { perror("fork P4"); cleanup(); exit(EXIT_FAILURE); }
        if (pid == 0) {
            execl("./p4_express.out", "p4_express.out", NULL);
            perror("execl p4_express.out");
            exit(EXIT_FAILURE);
        }
        p4_pid = pid;
        log_msg("DYSPOZYTOR", "Utworzono pracownika P4 (PID %d)", pid);
    }

    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = dispatcher_sig_handler;
        sa.sa_flags = 0;
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
        sigaction(SIGINT,  &sa, NULL);
    }


    log_msg("DYSPOZYTOR", "Rozpoczynam prace");
    int all_workers_done = 0;

    while (!g_signal3) {
        // Sygnał 1 - wymuś odjazd ciężarówki z niepełnym ładunkiem
        if (g_signal1) {
            g_signal1 = 0;
            for (int i = 0; i < TRUCK_COUNT_N; i++) {
                sem_p(SEM_TRUCK_MUTEX);
                int at_ramp = (trucks[i].status == TRUCK_AT_RAMP);
                sem_v(SEM_TRUCK_MUTEX);
                if (at_ramp) {
                    kill(truck_pids[i], SIGUSR1);
                    log_msg("DYSPOZYTOR", "Sygnal 1: ciezarowka %d odjedza z niepelnym", i);
                    break;
                }
            }
        }

        // Sygnał 2 → P4 dostarcza ekspres
        if (g_signal2) {
            g_signal2 = 0;
            kill(p4_pid, SIGUSR1);
            log_msg("DYSPOZYTOR", "Sygnal 2: P4 dostarcza ekspres");
        }

        // Odbierz komunikaty z kolejki: ciężarówki wracające z kursu
        msg_t msg;
        while (msgrcv(g_msgq_id, &msg, sizeof(msg) - sizeof(long),
                       MSG_TRUCK_RETURNED, IPC_NOWAIT) >= 0) {
            int truck_idx = msg.data;
            log_msg("DYSPOZYTOR", "Ciezarowka %d jest wolna", truck_idx);
            enqueue_free_truck(truck_idx);
        }

        // Wyślij wolną ciężarówkę pod rampę
        if (free_truck_count > 0) {
            int idx = dequeue_free_truck();
            log_msg("DYSPOZYTOR", "Wysylam ciezarowke %d pod rampe", idx);
            kill(truck_pids[idx], SIGRTMIN);
        }

        // Sprawdź czy pracownicy P1-P3 zakończyli
        if (!all_workers_done) {
            int done = 1;
            for (int i = 0; i < 3; i++) {
                if (worker_pids[i] > 0) {
                    int status;
                    pid_t ret = waitpid(worker_pids[i], &status, WNOHANG);
                    if (ret == worker_pids[i]) {
                        log_msg("DYSPOZYTOR", "Pracownik P%d (PID %d) zakonczyl", i+1, worker_pids[i]);
                        worker_pids[i] = 0;
                    } else done = 0;
                }
            }
            if (done) {
                all_workers_done = 1;
                log_msg("DYSPOZYTOR", "Wszyscy pracownicy P1-P3 zakonczyli");
            }
        }

        // Jeśli wszyscy skończyli i taśma pusta - zamykamy
        if (all_workers_done) {
            sem_p(SEM_BELT_MUTEX);
            int empty = (belt->count == 0);
            sem_v(SEM_BELT_MUTEX);
            if (empty) {
                log_msg("DYSPOZYTOR", "Tasma pusta – inicjuje zamkniecie");
                g_signal3 = 1;
            }
        }

        usleep(1000);
    }

    log_msg("DYSPOZYTOR", "=== ZAMYKANIE ===");

    // Flaga shutdown w pamięci dzielonej
    sem_p(SEM_BELT_MUTEX);
    belt->shutdown = 1;
    sem_v(SEM_BELT_MUTEX);

    // Wyślij SIGUSR2 do pracowników
    for (int i = 0; i < 3; i++)
        if (worker_pids[i] > 0) kill(worker_pids[i], SIGUSR2);
    if (p4_pid > 0) kill(p4_pid, SIGUSR2);

    // Odblokuj semafory
    for (int i = 0; i < 3; i++) { sem_v(SEM_BELT_SLOTS); sem_v(SEM_WEIGHT_FREED); }
    sem_v(SEM_EXPRESS_READY); sem_v(SEM_EXPRESS_DONE); sem_v(SEM_BELT_ITEMS);

    // Czekaj na zakończenie pracownikow
    for (int i = 0; i < 3; i++)
        if (worker_pids[i] > 0) waitpid(worker_pids[i], NULL, 0);
    if (p4_pid > 0) waitpid(p4_pid, NULL, 0);
    log_msg("DYSPOZYTOR", "Pracownicy zakonczyli");

    // Wymuś odjazd ciężarówek przy rampie
    for (int i = 0; i < TRUCK_COUNT_N; i++) {
        sem_p(SEM_TRUCK_MUTEX);
        int st = trucks[i].status;
        sem_v(SEM_TRUCK_MUTEX);
        if (st == TRUCK_AT_RAMP) kill(truck_pids[i], SIGUSR1);
    }

    // Czekaj aż ciężarówki wrócą z trasy
    for (int i = 0; i < TRUCK_COUNT_N; i++) {
        sem_p(SEM_TRUCK_MUTEX);
        int st = trucks[i].status;
        sem_v(SEM_TRUCK_MUTEX);
        while (st == TRUCK_ON_ROUTE) {
            usleep(10000);
            sem_p(SEM_TRUCK_MUTEX); st = trucks[i].status; sem_v(SEM_TRUCK_MUTEX);
        }
    }

    // Zamknij ciężarówki
    for (int i = 0; i < TRUCK_COUNT_N; i++) {
        kill(truck_pids[i], SIGUSR2);
        kill(truck_pids[i], SIGRTMIN);
    }
    for (int i = 0; i < TRUCK_COUNT_N; i++)
        waitpid(truck_pids[i], NULL, 0);

    log_msg("DYSPOZYTOR", "Ciezarowki zakonczyli");
    generate_report(start_time);
    cleanup();

    printf("\n=== Symulacja zakonczona. Raport: %s ===\n", g_log_file);
    return 0;
}
