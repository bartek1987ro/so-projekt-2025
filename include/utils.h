#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "config.h"
#include "structures.h"

/* ============================================================
 *  Globalne ID-ki IPC (ładowane z env w procesach potomnych)
 * ============================================================ */
static int g_sem_id  = -1;
static int g_shm_belt_id = -1;
static int g_shm_trucks_id = -1;
static int g_msgq_id = -1;
static char g_log_file[256] = "";

/* Nazwy zmiennych środowiskowych */
#define ENV_SEM_ID       "MAG_SEM_ID"
#define ENV_SHM_BELT     "MAG_SHM_BELT"
#define ENV_SHM_TRUCKS   "MAG_SHM_TRUCKS"
#define ENV_MSGQ_ID      "MAG_MSGQ_ID"
#define ENV_LOG_FILE     "MAG_LOG_FILE"

static void set_env_int(const char *name, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    if (setenv(name, buf, 1) < 0) { perror("setenv"); exit(EXIT_FAILURE); }
}

static void load_ipc_ids(void) {
    const char *s;
    s = getenv(ENV_SEM_ID);     if (s) g_sem_id = atoi(s);
    s = getenv(ENV_SHM_BELT);   if (s) g_shm_belt_id = atoi(s);
    s = getenv(ENV_SHM_TRUCKS); if (s) g_shm_trucks_id = atoi(s);
    s = getenv(ENV_MSGQ_ID);    if (s) g_msgq_id = atoi(s);
    s = getenv(ENV_LOG_FILE);   if (s) strncpy(g_log_file, s, sizeof(g_log_file) - 1);
}

/* ============================================================
 *  Operacje semaforowe
 * ============================================================ */
static void sem_p(int sem_num) {
    struct sembuf buf = { sem_num, -1, 0 };
    while (semop(g_sem_id, &buf, 1) == -1) {
        if (errno == EINTR) continue;
        perror("sem_p"); exit(EXIT_FAILURE);
    }
}

static void sem_v(int sem_num) {
    struct sembuf buf = { sem_num, 1, 0 };
    while (semop(g_sem_id, &buf, 1) == -1) {
        if (errno == EINTR) continue;
        perror("sem_v"); exit(EXIT_FAILURE);
    }
}

static int sem_p_nowait(int sem_num) {
    struct sembuf buf = { sem_num, -1, IPC_NOWAIT };
    if (semop(g_sem_id, &buf, 1) == -1) {
        if (errno == EAGAIN || errno == EINTR) return -1;
        perror("sem_p_nowait"); exit(EXIT_FAILURE);
    }
    return 0;
}

/* sem_p z przerwaniem przez sygnał (nie restartuje po EINTR) */
static int sem_p_intr(int sem_num) {
    struct sembuf buf = { sem_num, -1, 0 };
    if (semop(g_sem_id, &buf, 1) == -1) {
        if (errno == EINTR) return -1;
        perror("sem_p_intr"); exit(EXIT_FAILURE);
    }
    return 0;
}

/* ============================================================
 *  Logowanie do pliku i terminala
 * ============================================================ */

/* Wersja async-signal-safe — do użycia TYLKO w handlerach sygnałów.
 * Nie używa printf/malloc/semop — tylko write(), open(), close(), getpid().
 * Zapisuje: "[SIGNAL] PID <pid> otrzymal sygnal <signum> (<nazwa>)\n"
 * zarówno na stdout jak i do pliku raportu.                             */
static void sig_log(int signum) {
    /* Nazwy sygnałów których używamy */
    const char *name = "?";
    if      (signum == SIGUSR1)  name = "SIGUSR1";
    else if (signum == SIGUSR2)  name = "SIGUSR2";
    else if (signum == SIGINT)   name = "SIGINT";
    else if (signum == SIGCONT)  name = "SIGCONT";
    else if (signum == SIGALRM)  name = "SIGALRM";
    else if (signum == SIGRTMIN) name = "SIGRTMIN";

    /* Budujemy wiadomość ręcznie (snprintf nie jest async-signal-safe
     * wg POSIX, ale w glibc działa – tu używamy go dla czytelności;
     * alternatywą byłoby ręczne itoa + write) */
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                       "[SIGNAL] PID %d otrzymal sygnal %d (%s)\n",
                       getpid(), signum, name);
    if (len > 0) {
        write(STDOUT_FILENO, buf, len);
        /* Dopisz do pliku raportu (bez semafora – async-signal-safe) */
        int fd = open(g_log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) { write(fd, buf, len); close(fd); }
    }
}

static void log_msg(const char *source, const char *fmt, ...) {
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

    char line[700];
    int len = snprintf(line, sizeof(line), "[%s][%s](PID %d): %s\n",
                       time_str, source, getpid(), text);
    printf("%s", line);

    sem_p(SEM_LOG_MUTEX);
    int fd = open(g_log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { write(fd, line, len); close(fd); }
    sem_v(SEM_LOG_MUTEX);
}

/* ============================================================
 *  Obsługa błędów
 * ============================================================ */
static void error_exit(const char *source, const char *msg) {
    fprintf(stderr, "[BLAD][%s](PID %d): %s – %s\n",
            source, getpid(), msg, strerror(errno));
    exit(EXIT_FAILURE);
}

/* ============================================================
 *  Losowanie
 * ============================================================ */
static int rand_int(int min, int max) {
    return min + rand() % (max - min + 1);
}

static float rand_float(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

/* ============================================================
 *  Generowanie paczki
 * ============================================================ */
static package_t generate_package(int type) {
    package_t pkg;
    pkg.type = type;
    pkg.express = 0;
    pkg.worker_pid = getpid();
    pkg.id = 0;

    switch (type) {
        case PKG_TYPE_A:
            pkg.weight = rand_float(WEIGHT_MIN_A, WEIGHT_MAX_A);
            pkg.volume = VOLUME_A;
            break;
        case PKG_TYPE_B:
            pkg.weight = rand_float(WEIGHT_MIN_B, WEIGHT_MAX_B);
            pkg.volume = VOLUME_B;
            break;
        case PKG_TYPE_C:
            pkg.weight = rand_float(WEIGHT_MIN_C, WEIGHT_MAX_C);
            pkg.volume = VOLUME_C;
            break;
        default:
            pkg.weight = 1.0;
            pkg.volume = VOLUME_A;
            break;
    }
    return pkg;
}

static const char* pkg_type_name(int type) {
    switch (type) {
        case PKG_TYPE_A: return "A";
        case PKG_TYPE_B: return "B";
        case PKG_TYPE_C: return "C";
        default:         return "?";
    }
}

#endif /* UTILS_H */