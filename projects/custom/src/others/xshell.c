#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int main_exec(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage:%s {executable_file_path} [[arg]...]\n", argv[0]);
        return EXIT_FAILURE;
    }
    pid_t pid;
    int status;
    while (1) {
        pid = fork();
        if (pid < 0) {
            perror("fork() failed!");
            return EXIT_FAILURE;
        } else if (pid == 0) {
            if (execvp(argv[1], argv + 2) < 0) {
                perror("execvp() failed!");
                return EXIT_FAILURE;
            }
        } else {
            pid = wait(&status);
            printf("pid(%d) wait() returned with %d!\n", pid, status);
            sleep(1);
        }
    }
    return EXIT_SUCCESS;
}

static int main_system(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage:%s {executable_file_path} [[arg]...]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    for (int i = 1; i < argc; ++i) {
        strcat(buffer, argv[i]);
        strcat(buffer, " ");
    }
    printf("invoke command %s\n", buffer);
    int status;
    while (1) {
        status = system(buffer);
        printf("pid(%d) system() returned with %d!\n", getpid(), status);
        sleep(1);
    }
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    return main_exec(argc, argv);
}