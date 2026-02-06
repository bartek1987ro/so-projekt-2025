
// Pracownik P4 – przesyłki ekspresowe
// Czeka na SIGUSR1 od dyspozytora, generuje pakiet, dostarcza.

#include "../include/utils.h"

static volatile sig_atomic_t g_express_request = 0;
static volatile sig_atomic_t g_shutdown = 0;

static void sig_handler(int signum) {
    if (signum == SIGUSR1)      g_express_request = 1;
    else if (signum == SIGUSR2) g_shutdown = 1;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    srand(time(NULL) ^ getpid());
    load_ipc_ids();

    belt_t *belt = (belt_t *)shmat(g_shm_belt_id, NULL, SHM_RDONLY);
    if (belt == (void *)-1) error_exit("P4", "shmat belt");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    log_msg("P4", "Pracownik ekspresowy rozpoczal prace");

    int express_count = 0;

    // Maska – czekaj tylko na SIGUSR1 i SIGUSR2
    sigset_t wait_mask;
    sigfillset(&wait_mask);
    sigdelset(&wait_mask, SIGUSR1);
    sigdelset(&wait_mask, SIGUSR2);

    while (!g_shutdown) {
        // Czekaj na polecenie od dyspozytora (sygnał 2 → SIGUSR1 do P4)
        while (!g_express_request && !g_shutdown)
            sigsuspend(&wait_mask);
        if (g_shutdown) break;
        g_express_request = 0;

        // Generuj losowy pakiet ekspresowy
        int count = rand_int(EXPRESS_MIN_COUNT, EXPRESS_MAX_COUNT);
        log_msg("P4", "Pakiet ekspresowy (%d paczek)", count);

        for (int i = 0; i < count && !g_shutdown; i++) {
            int type = rand_int(PKG_TYPE_A, PKG_TYPE_C);
            package_t pkg = generate_package(type);
            pkg.express = 1;

            /* Sygnalizuj ciężarówce: mam ekspres */
            sem_v(SEM_EXPRESS_READY);
            /* Czekaj na potwierdzenie załadunku */
            sem_p(SEM_EXPRESS_DONE);
            if (g_shutdown) break;

            express_count++;
            log_msg("P4", "Dostarczono ekspres #%d typ %s, waga %.1f kg",
                    express_count, pkg_type_name(type), pkg.weight);
        }
    }

    log_msg("P4", "P4 konczy prace (dostarczono %d ekspresow)", express_count);
    shmdt((void *)belt);
    return 0;
}
