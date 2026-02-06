
//  Ciężarówka – ładuje paczki z taśmy i rozwozi


#include "../include/utils.h"

static volatile sig_atomic_t g_force_leave = 0;  // sygnał 1: odjazd z niepełnym
static volatile sig_atomic_t g_shutdown = 0;     // SIGUSR2: koniec pracy
static volatile sig_atomic_t g_alarm_fired = 0;  // SIGALRM: kurs zakończony
static volatile sig_atomic_t g_go_to_ramp = 0;   // SIGRTMIN: podjedź pod rampę

static int g_truck_index = -1;

static void sig_handler(int signum) {
    if (signum == SIGUSR1)       g_force_leave = 1;
    else if (signum == SIGUSR2)  g_shutdown = 1;
    else if (signum == SIGALRM)  g_alarm_fired = 1;
    else if (signum == SIGRTMIN) g_go_to_ramp = 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "Uzycie: truck.out <index>\n"); exit(EXIT_FAILURE); }

    g_truck_index = atoi(argv[1]);
    srand(time(NULL) ^ getpid());
    load_ipc_ids();

    // Podłącz pamięć dzieloną
    belt_t *belt = (belt_t *)shmat(g_shm_belt_id, NULL, 0);
    if (belt == (void *)-1) error_exit("TRUCK", "shmat belt");

    truck_state_t *trucks = (truck_state_t *)shmat(g_shm_trucks_id, NULL, 0);
    if (trucks == (void *)-1) error_exit("TRUCK", "shmat trucks");

    truck_state_t *my = &trucks[g_truck_index];
    my->pid = getpid();
    my->status = TRUCK_FREE;
    my->current_weight = 0; my->current_volume = 0;
    my->package_count = 0;  my->courses = 0; my->total_packages = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);   // odjazd z niepełnym
    sigaction(SIGUSR2, &sa, NULL);   //shutdown
    sigaction(SIGALRM, &sa, NULL);   // koniec kursu
    sigaction(SIGRTMIN, &sa, NULL);  //podjedź pod rampę

    char src[16];
    snprintf(src, sizeof(src), "TRUCK%d", g_truck_index);
    log_msg(src, "Ciezarowka gotowa (ladownosc %.0f kg, %.2f m3)",
            TRUCK_LOAD_W, TRUCK_VOLUME_V);

    // Maska do sigsuspend – czekaj na konkretne sygnały
    sigset_t wait_mask;
    sigfillset(&wait_mask);
    sigdelset(&wait_mask, SIGRTMIN);
    sigdelset(&wait_mask, SIGUSR1);
    sigdelset(&wait_mask, SIGUSR2);
    sigdelset(&wait_mask, SIGALRM);

    // Poinformuj dyspozytora: jestem wolna
    msg_t msg;
    msg.mtype = MSG_TRUCK_RETURNED;
    msg.sender_pid = getpid();
    msg.data = g_truck_index;
    if (msgsnd(g_msgq_id, &msg, sizeof(msg) - sizeof(long), 0) < 0)
        if (errno != EINTR) error_exit(src, "msgsnd init");

    //GŁÓWNA PĘTLA: czekaj → ładuj → jedź → wróć
    while (!g_shutdown) {
        // Czekaj na sygnał SIGRTMIN od dyspozytora (podjedź pod rampę)
        g_go_to_ramp = 0;
        while (!g_go_to_ramp && !g_shutdown)
            sigsuspend(&wait_mask);
        if (g_shutdown) break;

        // Zajmij rampę (mutex – tylko 1 ciężarówka naraz)
        sem_p(SEM_RAMP_MUTEX);

        sem_p(SEM_TRUCK_MUTEX);
        my->status = TRUCK_AT_RAMP;
        my->current_weight = 0; my->current_volume = 0;
        my->package_count = 0;
        g_force_leave = 0;
        sem_v(SEM_TRUCK_MUTEX);

        log_msg(src, "Podjezdzam pod rampe");

        int loading = 1;
        while (loading && !g_shutdown) {

            // 1) Priorytet: przesyłki ekspresowe
            if (sem_p_nowait(SEM_EXPRESS_READY) == 0) {
                int type = rand_int(PKG_TYPE_A, PKG_TYPE_C);
                package_t epkg = generate_package(type);
                epkg.express = 1;

                sem_p(SEM_TRUCK_MUTEX);
                if (my->current_weight + epkg.weight <= TRUCK_LOAD_W &&
                    my->current_volume + epkg.volume <= TRUCK_VOLUME_V) {
                    my->current_weight += epkg.weight;
                    my->current_volume += epkg.volume;
                    my->package_count++;
                    sem_v(SEM_TRUCK_MUTEX);
                    sem_v(SEM_EXPRESS_DONE);
                    log_msg(src, "Zaladowano EKSPRES typ %s, waga %.1f kg "
                            "[ciezarowka: %.1f/%.0f kg, %.3f/%.2f m3]",
                            pkg_type_name(type), epkg.weight,
                            my->current_weight, TRUCK_LOAD_W,
                            my->current_volume, TRUCK_VOLUME_V);
                } else {
                    sem_v(SEM_TRUCK_MUTEX);
                    sem_v(SEM_EXPRESS_DONE);
                    log_msg(src, "Brak miejsca na ekspres");
                }
                continue;
            }

            // 2) Sygnał 1: dyspozytor kazał odjechać z niepełnym
            if (g_force_leave) {
                log_msg(src, "Dyspozytor nakazal odjazd z niepelnym");
                break;
            }

            /* 3) Sprawdź czy następna paczka się zmieści */
            sem_p(SEM_TRUCK_MUTEX);
            float cw = my->current_weight;
            float cv = my->current_volume;
            sem_v(SEM_TRUCK_MUTEX);

            sem_p(SEM_BELT_MUTEX);
            int belt_empty = (belt->count == 0);
            int can_fit = 0;
            if (!belt_empty) {
                package_t *next = &belt->packages[belt->head];
                can_fit = (cw + next->weight <= TRUCK_LOAD_W) &&
                          (cv + next->volume <= TRUCK_VOLUME_V);
            }
            int belt_shut = belt->shutdown;
            sem_v(SEM_BELT_MUTEX);

            if (!can_fit && !belt_empty) {
                log_msg(src, "Ciezarowka pelna (nastepna paczka sie nie miesci)");
                break;
            }
            if (belt_empty && belt_shut) {
                log_msg(src, "Tasma pusta i koniec pracy");
                break;
            }

            // 4) Pobierz paczkę z taśmy
            if (sem_p_nowait(SEM_BELT_ITEMS) != 0) {
                if (g_shutdown || g_force_leave) break;
                sem_p(SEM_BELT_ITEMS);
                if (g_shutdown || g_force_leave) { sem_v(SEM_BELT_ITEMS); break; }
            }

            sem_p(SEM_BELT_MUTEX);
            if (belt->count == 0) {
                sem_v(SEM_BELT_MUTEX);
                sem_v(SEM_BELT_ITEMS);
                continue;
            }
            package_t pkg = belt->packages[belt->head];
            belt->head = (belt->head + 1) % BELT_CAPACITY_K;
            belt->count--;
            belt->current_weight -= pkg.weight;
            sem_v(SEM_BELT_MUTEX);

            // Oddaj wolne miejsce na taśmie + obudź pracownika czekającego na wagę
            sem_v(SEM_BELT_SLOTS);
            sem_v(SEM_WEIGHT_FREED);

            sem_p(SEM_TRUCK_MUTEX);
            my->current_weight += pkg.weight;
            my->current_volume += pkg.volume;
            my->package_count++;
            sem_v(SEM_TRUCK_MUTEX);

            log_msg(src, "Zaladowano paczke #%d typ %s, waga %.1f kg "
                    "[ciezarowka: %.1f/%.0f kg, %.3f/%.2f m3, %d szt]",
                    pkg.id, pkg_type_name(pkg.type), pkg.weight,
                    my->current_weight, TRUCK_LOAD_W,
                    my->current_volume, TRUCK_VOLUME_V,
                    my->package_count);
        }

        // odjazd w trase
        sem_p(SEM_TRUCK_MUTEX);
        my->status = TRUCK_ON_ROUTE;
        int pkgs_this_run = my->package_count;
        my->total_packages += pkgs_this_run;
        my->courses++;
        int course_num = my->courses;
        sem_v(SEM_TRUCK_MUTEX);

        // Zwolnij rampę
        sem_v(SEM_RAMP_MUTEX);

        if (pkgs_this_run == 0)
            log_msg(src, "Odjazd pusty (kurs #%d)", course_num);
        else
            log_msg(src, "Odjazd: %d paczek, %.1f kg, %.3f m3 (kurs #%d)",
                    pkgs_this_run, my->current_weight, my->current_volume, course_num);

        if (g_shutdown && pkgs_this_run == 0) break;

        int course_time = rand_int(TRUCK_MIN_COURSE, TRUCK_MAX_COURSE);
        log_msg(src, "W trasie na %d sekund", course_time);

        g_alarm_fired = 0;
        alarm(course_time);

        sigset_t alarm_mask;
        sigfillset(&alarm_mask);
        sigdelset(&alarm_mask, SIGALRM);
        sigdelset(&alarm_mask, SIGUSR2);

        while (!g_alarm_fired && !g_shutdown)
            sigsuspend(&alarm_mask);
        alarm(0);

        // powrot-reset
        sem_p(SEM_TRUCK_MUTEX);
        my->status = TRUCK_FREE;
        my->current_weight = 0; my->current_volume = 0; my->package_count = 0;
        sem_v(SEM_TRUCK_MUTEX);

        log_msg(src, "Wrocil z kursu #%d", course_num);

        // Poinformuj dyspozytora przez kolejkę komunikatów
        msg.mtype = MSG_TRUCK_RETURNED;
        msg.sender_pid = getpid();
        msg.data = g_truck_index;
        if (msgsnd(g_msgq_id, &msg, sizeof(msg) - sizeof(long), 0) < 0)
            if (errno != EINTR) error_exit(src, "msgsnd TRUCK_RETURNED");
    }

    log_msg(src, "Ciezarowka konczy prace (kursow: %d, paczek: %d)",
            my->courses, my->total_packages);

    shmdt(belt);
    shmdt(trucks);
    return 0;
}
