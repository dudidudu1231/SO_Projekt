#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
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
#include <signal.h>
#include <sys/msg.h>

#define FIFO_NAME   "/tmp/my_fifo"
#define ByteSize    256
#define SHM_NAME    "/shared_memory"

struct sig_msg {
    long mtype;
    struct {
        pid_t sender_pid;
        int sig_num;
    } content;
};

// ID kolejek komunikatów
int msgq_p1, msgq_p2, msgq_p3;

// Semafory i zmienne globalne
sem_t *sem_2, *sem_3;
volatile sig_atomic_t g_paused = 0;
volatile sig_atomic_t g_terminate = 0;

// PID procesów
pid_t pid1, pid2, pid3, parent_pid;

void cleanup() {
    sem_unlink("/sem2");
    sem_unlink("/sem3");
    shm_unlink(SHM_NAME);
    unlink(FIFO_NAME);
    msgctl(msgq_p1, IPC_RMID, NULL);
    msgctl(msgq_p2, IPC_RMID, NULL);
    msgctl(msgq_p3, IPC_RMID, NULL);
}

void send_signal(int msgq, pid_t sender, int sig) {
    struct sig_msg msg;
    msg.mtype = 1;
    msg.content.sender_pid = sender;
    msg.content.sig_num = sig;
    
    if (msgsnd(msgq, &msg, sizeof(msg.content), 0) == -1) {
        perror("msgsnd failed");
    }
}

// Funkcje handlerów sygnałów dla poszczególnych procesów
void p1_handler(int sig, siginfo_t *info, void *ucontext) {
    if (info->si_pid != parent_pid) return;

    struct sig_msg msg;
    if (msgrcv(msgq_p1, &msg, sizeof(msg.content), 1, 0) == -1) {
        perror("msgrcv failed");
        return;
    }

    switch(msg.content.sig_num) {
        case SIGTSTP:
            g_paused = 1;
            kill(pid2, SIGUSR1); // Przekaż do P2
            break;
        case SIGCONT:
            g_paused = 0;
            kill(pid2, SIGUSR2); // Przekaż do P2
            break;
        case SIGTERM:
            g_terminate = 1;
            kill(pid2, SIGTERM); // Przekaż do P2
            break;
    }
}

void p2_handler(int sig, siginfo_t *info, void *ucontext) {
    if (info->si_pid != pid1) return;

    struct sig_msg msg;
    if (msgrcv(msgq_p2, &msg, sizeof(msg.content), 1, 0) == -1) {
        perror("msgrcv failed");
        return;
    }

    switch(msg.content.sig_num) {
        case SIGUSR1: // SIGTSTP
            g_paused = 1;
            kill(pid3, SIGUSR1);
            break;
        case SIGUSR2: // SIGCONT
            g_paused = 0;
            kill(pid3, SIGUSR2);
            break;
        case SIGTERM:
            g_terminate = 1;
            kill(pid3, SIGTERM);
            break;
    }
}

void p3_signal_handler(int sig, siginfo_t *info, void *ucontext) {
    struct sig_msg msg;
    msg.mtype = 1;
    msg.content.sender_pid = getpid();
    
    switch(sig) {
        case SIGTSTP:
        case SIGCONT:
        case SIGTERM:
            msg.content.sig_num = sig;
            send_signal(msgq_p3, getpid(), sig);
            kill(parent_pid, SIGUSR1); // Powiadom rodzica
            break;
    }
}

void parent_handler(int sig) {
    struct sig_msg msg;
    if (msgrcv(msgq_p3, &msg, sizeof(msg.content), 1, 0) == -1) return;

    // Przekaż sygnał do P1
    send_signal(msgq_p1, parent_pid, msg.content.sig_num);
    kill(pid1, SIGUSR1);
}

void setup_signals(pid_t pid) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    if (pid == pid1) {
        sa.sa_sigaction = p1_handler;
        sigaction(SIGUSR1, &sa, NULL);
    } 
    else if (pid == pid2) {
        sa.sa_sigaction = p2_handler;
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
    }
    else if (pid == pid3) {
        sa.sa_sigaction = p3_signal_handler;
        sigaction(SIGTSTP, &sa, NULL);
        sigaction(SIGCONT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
    }
}

// Pozostałe funkcje procesów (P1, P2, P3) z odpowiednimi
// sprawdzeniami flag g_paused i g_terminate w pętlach głównych

int main() {
    parent_pid = getpid();
    atexit(cleanup);

    // Utwórz kolejki komunikatów
    msgq_p1 = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    msgq_p2 = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    msgq_p3 = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

    // Uruchom procesy potomne
    if ((pid1 = fork()) == 0) {
        setup_signals(pid1);
        // Kod procesu P1...
        exit(0);
    }
    if ((pid2 = fork()) == 0) {
        setup_signals(pid2);
        // Kod procesu P2...
        exit(0);
    }
    if ((pid3 = fork()) == 0) {
        setup_signals(pid3);
        // Kod procesu P3...
        exit(0);
    }

    // Konfiguracja handlera dla rodzica
    signal(SIGUSR1, parent_handler);

    // Czekaj na zakończenie procesów
    while(wait(NULL) > 0);
    return 0;
}
