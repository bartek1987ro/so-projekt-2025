// Pracownik P4 - przesylki ekspresowe
// Czeka na SIGUSR1 od dyspozytora, generuje pakiet, wpisuje do pamieci dzielonej.

#include "../include/utils.h"

static volatile sig_atomic_t g_express_request = 0;
static volatile sig_atomic_t g_shutdown = 0;

static void sig_handler(int signum) {
    sig_log(signum);
    if (signum == SIGUSR1)      g_express_request = 1;
    else if (signum == SIGUSR2) g_shutdown = 1;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    srand(time(NULL) ^ getpid());
    load_ipc_ids();

    belt_t *belt = (belt_t *)shmat(g_shm_belt_id, NULL, 0);
    if (belt == (void *)-1) error_exit("P4", "shmat belt");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGCONT, &sa, NULL);
    signal(SIGINT, SIG_IGN);

    log_msg("P4", "Pracownik ekspresowy rozpoczal prace");

    int express_count = 0;

    sigset_t wait_mask;
    sigfillset(&wait_mask);
    sigdelset(&wait_mask, SIGUSR1);
    sigdelset(&wait_mask, SIGUSR2);
    sigdelset(&wait_mask, SIGCONT);

    while (!g_shutdown) {
        while (!g_express_request && !g_shutdown)
            sigsuspend(&wait_mask);
        if (g_shutdown) break;
        g_express_request = 0;

        int count = rand_int(EXPRESS_MIN_COUNT, EXPRESS_MAX_COUNT);
        log_msg("P4", "Pakiet ekspresowy (%d paczek)", count);

        /* Powiadom ciężarówkę ile paczek będzie w pakiecie */
        sem_p(SEM_BELT_MUTEX);
        belt->express_remaining = count;
        sem_v(SEM_BELT_MUTEX);

        for (int i = 0; i < count && !g_shutdown; i++) {
            int type = rand_int(PKG_TYPE_A, PKG_TYPE_C);
            package_t pkg = generate_package(type);
            pkg.express = 1;

            /* Wpisz paczke do pamieci dzielonej */
            sem_p(SEM_BELT_MUTEX);
            belt->express_pkg = pkg;
            belt->express_waiting = 1;
            sem_v(SEM_BELT_MUTEX);

            /* Obudz ciezarowke (moze byc zablokowana na SEM_BELT_ITEMS) */
            sem_v(SEM_BELT_ITEMS);

            /* Czekaj na potwierdzenie zaladunku */
            while (1) {
                if (sem_p_intr(SEM_EXPRESS_DONE) == 0) break;
                if (g_shutdown) break;
            }
            if (g_shutdown) break;

            express_count++;
            log_msg("P4", "Dostarczono ekspres #%d typ %s, waga %.1f kg",
                    express_count, pkg_type_name(type), pkg.weight);
        }

        /* Wyczyść remaining na wypadek przerwania */
        sem_p(SEM_BELT_MUTEX);
        belt->express_remaining = 0;
        belt->express_waiting = 0;
        sem_v(SEM_BELT_MUTEX);
    }

    log_msg("P4", "P4 konczy prace (dostarczono %d ekspresow)", express_count);
    shmdt((void *)belt);
    return 0;
}