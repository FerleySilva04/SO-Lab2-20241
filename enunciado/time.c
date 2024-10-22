#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <comando>\n", argv[0]);
        return 1;
    }

    struct timeval start, end;
    gettimeofday(&start, NULL); // Tiempo inicial

    pid_t pid = fork();

    if (pid == -1) {
        perror("Error al hacer fork");
        return 1;
    } else if (pid == 0) { // Proceso hijo
        execvp(argv[1], &argv[1]);
        perror("Error al ejecutar el comando");
        exit(1);
    } else { // Proceso padre
        int status;
        waitpid(pid, &status, 0); // Espera a que el proceso hijo termine

        if (WIFEXITED(status)) { // Verifica si el hijo terminó correctamente
            gettimeofday(&end, NULL); // Tiempo final

            double elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

            printf("Elapsed time: %f seconds\n", elapsedTime);
        } else {
            printf("El proceso hijo no terminó correctamente.\n");
        }
    }

    return 0;
}
