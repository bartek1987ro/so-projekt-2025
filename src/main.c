#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    printf("=== Projekt SO 2025/2026 ===\n");
    printf("PID: %d, PPID: %d\n", getpid(), getppid());
    
    // Test fork()
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        // Proces potomny
        printf("Proces potomny: PID=%d\n", getpid());
        exit(EXIT_SUCCESS);
    } else {
        // Proces rodzic
        printf("Proces rodzic: PID=%d, potomek=%d\n", getpid(), pid);
        wait(NULL);
        printf("Potomek zakończył działanie\n");
    }
    
    return EXIT_SUCCESS;
}
