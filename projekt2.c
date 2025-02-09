#include <stdio.h>//testowy komentarz
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#define FIFO_NAME "/home/filip/skrypty_shellowe/projekt/my_fifo"
#define ByteSize 256
#define SHM_NAME "/shared_memory"

sem_t *sem_1, *sem_2, *sem_3;
char buffer[ByteSize];

// Funkcja raportująca błędy
void report_and_exit(const char *msg, int exit_code) {
    perror(msg);
    exit(exit_code);
}

// Funkcja zliczająca znaki w tekście
int zlicz_znaki(const char *str) {
    int count = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] != '\n' && str[i] != '\r') count++;
    }
    return count;
}

// Proces P1
void P1() {
    if (mkfifo(FIFO_NAME, 0666) == -1 && errno != EEXIST) {
        report_and_exit("mkfifo error", 1);
    }

    int fd = open(FIFO_NAME, O_WRONLY);
    if (fd == -1) {
        report_and_exit("open FIFO error", 1);
    }

    while (1) {
        printf("\nMenu:\n");
        printf("1. Czytaj z klawiatury (stdin)\n");
        printf("2. Czytaj z pliku (każdy wiersz kończy się <EOL>)\n");
        printf("Wybierz opcję: ");

        int choice;
        scanf("%d", &choice);
        getchar();

        if (choice == 1) {
            printf("P1(%d): Wprowadz tekst do FIFO: ", getpid());
            fgets(buffer, ByteSize, stdin);

            if (strncmp(buffer, "exit", 4) == 0) {
                write(fd, buffer, strlen(buffer) + 1);
                break;
            }

            write(fd, buffer, strlen(buffer) + 1);

        } else if (choice == 2) {
            char filename[256];
            printf("Podaj nazwę pliku: ");
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

            FILE *file = fopen(filename, "r");
            if (!file) {
                perror("Nie udało się otworzyć pliku");
                continue;
            }

            while (fgets(buffer, ByteSize, file)) {
                printf("P1(%d): Wczytano tekst z pliku: %s", getpid(), buffer);
                write(fd, buffer, strlen(buffer) + 1);
                printf("P1(%d): Linia wysłana do FIFO\n", getpid());
                sem_wait(sem_1); // Czekaj na sygnał od P2
            }

            fclose(file);

        } else {
            printf("Nieprawidłowy wybór. Spróbuj ponownie.\n");
        }
    }

    close(fd);
}

// Proces P2
void P2(char *shared_memory) {
    int fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        report_and_exit("open FIFO error", 1);
    }

    while (1) {
        read(fd, buffer, ByteSize);
        printf("P2(%d): Dane odczytane z FIFO: %s\n", getpid(), buffer);

        if (strncmp(buffer, "exit", 4) == 0) {
            strncpy(shared_memory, "exit", ByteSize);
            sem_post(sem_3);
            break;
        }

        int ilosc = zlicz_znaki(buffer);
        sprintf(shared_memory, "%d", ilosc);

        printf("P2(%d): Ilosc znakow: %d\n", getpid(), ilosc);
        sem_post(sem_3); // Sygnał do P3
        printf("P2(%d): przekazuje do P3\n", getpid());
        sem_wait(sem_2); // Czekaj na odpowiedź od P3
        sem_post(sem_1); // Sygnał do P1
    }

    close(fd);
}

// Proces P3
void P3(char *shared_memory) {
    while (1) {
        sem_wait(sem_3); // Czekaj na dane od P2

        if (strncmp(shared_memory, "exit", 4) == 0) {
            break;
        }

        printf("P3(%d): Dane odczytane z PD: %s\n\n", getpid(), shared_memory);
        printf("-------------------------------\n");
        sem_post(sem_2); // Sygnał do P2
    }
}

int main() {
    // Usuń stare semafory
    sem_unlink("/sem_1");
    sem_unlink("/sem_2");
    sem_unlink("/sem_3");
    shm_unlink(SHM_NAME); // Usuń starą pamięć dzieloną

    // Utwórz nowe semafory
    sem_1 = sem_open("/sem_1", O_CREAT | O_EXCL, 0666, 0);
    sem_2 = sem_open("/sem_2", O_CREAT | O_EXCL, 0666, 0);
    sem_3 = sem_open("/sem_3", O_CREAT | O_EXCL, 0666, 0);

    if (sem_1 == SEM_FAILED || sem_2 == SEM_FAILED || sem_3 == SEM_FAILED) {
        report_and_exit("sem_open error", 1);
    }

    // Utwórz pamięć dzieloną
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        report_and_exit("shm_open error", 1);
    }

    if (ftruncate(shm_fd, ByteSize) == -1) {
        report_and_exit("ftruncate error", 1);
    }

    char *shared_memory = mmap(NULL, ByteSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        report_and_exit("mmap error", 1);
    }

    pid_t pid = fork();

    if (pid == 0) {
        // Proces P2
        P2(shared_memory);
    } else {
        pid_t pid2 = fork();

        if (pid2 == 0) {
            // Proces P3
            P3(shared_memory);
        } else {
            // Proces P1
            P1();

            wait(NULL);
            wait(NULL);

            // Posprzątaj semafory i pamięć dzieloną
            sem_unlink("/sem_1");
            sem_unlink("/sem_2");
            sem_unlink("/sem_3");
            shm_unlink(SHM_NAME);
        }
    }

    return 0;
}

