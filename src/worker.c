// Pracownik P1/P2/P3 – układa przesyłki na taśmę
// typ: 0=A, 1=B, 2=C


#include "../include/utils.h"

static volatile sig_atomic_t g_shutdown = 0;

static void sig_handler(int signum) {
    sig_log(signum);
    if (signum == SIGUSR2) g_shutdown = 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uzycie: worker.out <typ 0-2> [max_paczek]\n");
        exit(EXIT_FAILURE);
    }

    int pkg_type = atoi(argv[1]);
    int max_pkgs = MAX_PACKAGES;
    if (argc >= 3) max_pkgs = atoi(argv[2]);

    srand(time(NULL) ^ getpid());
    load_ipc_ids();

    belt_t *belt = (belt_t *)shmat(g_shm_belt_id, NULL, 0);
    if (belt == (void *)-1) error_exit("WORKER", "shmat belt");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGCONT, &sa, NULL);
    signal(SIGINT, SIG_IGN);         // ignoruj Ctrl+C (zamykanie przez SIGUSR2)

    const char *type_name = pkg_type_name(pkg_type);
    char src[16];
    snprintf(src, sizeof(src), "P%d", pkg_type + 1);

    log_msg(src, "Pracownik rozpoczal prace (paczki typ %s)", type_name);

    int packages_created = 0;

    while (!g_shutdown) {
        // Sprawdź limit paczek
        if (max_pkgs > 0 && packages_created >= max_pkgs) {
            log_msg(src, "Osiagnieto limit %d paczek, koncze", max_pkgs);
            break;
        }

        package_t pkg = generate_package(pkg_type);

        // Czekaj na wolne miejsce na taśmie
        if (sem_p_intr(SEM_BELT_SLOTS) == -1) {
            if (g_shutdown) break;
            continue;
        }
        if (g_shutdown) { sem_v(SEM_BELT_SLOTS); break; }

        int placed = 0;
        while (!placed && !g_shutdown) {
            sem_p(SEM_BELT_MUTEX);

            if (belt->shutdown) {
                sem_v(SEM_BELT_MUTEX);
                g_shutdown = 1;
                break;
            }

            // Sprawdź czy waga się zmieści
            if (belt->current_weight + pkg.weight <= BELT_MAX_WEIGHT_M) {
                pkg.id = belt->next_id++;
                belt->packages[belt->tail] = pkg;
                belt->tail = (belt->tail + 1) % BELT_CAPACITY_K;
                belt->count++;
                belt->current_weight += pkg.weight;
                placed = 1;

                sem_v(SEM_BELT_MUTEX);

                // jest nowa paczka
                sem_v(SEM_BELT_ITEMS);

                log_msg(src, "Polozono paczke #%d typ %s, waga %.1f kg "
                        "[tasma: %d/%d szt, %.1f/%.0f kg]",
                        pkg.id, type_name, pkg.weight,
                        belt->count, BELT_CAPACITY_K,
                        belt->current_weight, BELT_MAX_WEIGHT_M);

                packages_created++;
                //usleep(500000); //Test sygnalu
            } else {
                //czekaj aż ciężarówka coś zabierze
                sem_v(SEM_BELT_MUTEX);
                if (sem_p_intr(SEM_WEIGHT_FREED) == -1) {
                    if (g_shutdown) break;
                    continue;
                }
                if (g_shutdown) break;
            }
        }

        if (!placed && !g_shutdown) {
            sem_v(SEM_BELT_SLOTS);
        }
    }

    log_msg(src, "Pracownik konczy prace (utworzono %d paczek)", packages_created);
    shmdt(belt);
    return 0;
}