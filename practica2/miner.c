/**
 * @brief It defines a simulation of blockchain and minering
 *
 * @file miner.c
 * @author Shaofan Xu, Javier Santamaria
 * @version 1.0
 */
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include "pow.h"

typedef struct Thread_args
{
    long int start;
    long int end;
} Thread_args;

#define PID_FILE "pids.pid"
#define TARGET_FILE "target.tgt"
#define VOTE_FILE "vote.txt"
#define SEM_NAME_PID "/miner_pid"
#define SEM_NAME_TARGET "/miner_target"
#define SEM_NAME_WINNER "/miner_winner"
#define SEM_NAME_VOTE "/miner_vote"

#define TARGET_INIT 0

sem_t *sem_pid = NULL;
sem_t *sem_votes = NULL;
sem_t *sem_winner = NULL;
sem_t *sem_target = NULL;

atomic_int globalSolution = 0;
atomic_int findSolution = 0;
volatile sig_atomic_t time_to_exit = 0;
int globalTarget = 0;
int n_threads = 0;
pthread_t *threads;
Thread_args *args;

volatile sig_atomic_t start_mining = 0;
volatile sig_atomic_t start_voting = 0;

void print_all_miners(FILE *f)
{
    rewind(f);
    int pid;
    printf("Current miners in system: \n");
    while (fscanf(f, "%d", &pid) == 1)
    {
        fprintf(stdout, "  %d\n", pid);
    }
}

void miner_shutdown()
{
    int pid, remaining_miner = 0;
    pid_t myPid = getpid();

    while (sem_wait(sem_pid) == -1 && errno == EINTR);

    FILE *pidFile = fopen(PID_FILE, "r");
    FILE *tempFile = fopen("temp.pid", "w");

    if (pidFile != NULL && tempFile != NULL)
    {
        while (fscanf(pidFile, "%d", &pid) == 1)
        {
            if ((pid_t)pid != myPid)
            {
                remaining_miner++;
                fprintf(tempFile, "%d\n", pid);
            }
        }
        fclose(pidFile);
        fclose(tempFile);

        remove(PID_FILE);
        if (remaining_miner > 0)
        {
            rename("temp.pid", PID_FILE);
            printf("Miner %d exited system. Remaining miners: %d\n", (int)myPid, remaining_miner);
        }
        else
        {
            remove("temp.pid");
            remove(TARGET_FILE);
            remove(VOTE_FILE);

            sem_unlink(SEM_NAME_PID);
            sem_unlink(SEM_NAME_TARGET);
            sem_unlink(SEM_NAME_WINNER);
            sem_unlink(SEM_NAME_VOTE);
            printf("Miner %d was the last one. System cleaned.\n", (int)myPid);
        }
    }

    sem_post(sem_pid);
    sem_close(sem_pid);
    sem_close(sem_target);
    sem_close(sem_winner);
    sem_close(sem_votes);

    exit(EXIT_SUCCESS);
}

void alarm_handler(int sig) { time_to_exit = 1; }
void sigusr1_handler(int sig) { start_mining = 1; }
void sigusr2_handler(int sig) { start_voting = 1; }

void *miner(void *arg)
{
    Thread_args *t = (Thread_args *)arg;
    
    for (long int i = t->start; i < t->end; i++)
    {
        if (start_voting || time_to_exit) return NULL;

        if (pow_hash(i) == globalTarget)
        {
            if (sem_trywait(sem_winner) == 0)
            {
                globalSolution = i;
                findSolution = 1; 
            }
            return NULL;
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int n_seconds, temp;
    int is_first_miner = 0;
    int total_miners = 0;
    int solution_escrita = 0;
    int is_winner = 0;
    int my_coins = 0;
    int round_id = 0; 
    struct sigaction pid_act, sigusr1_act, sigusr2_act;
    sigset_t mask_block, mask_empty;

    FILE *targetFile = NULL;
    FILE *pidFile = NULL;
    FILE *voteFile = NULL;

    sem_unlink(SEM_NAME_PID);
    sem_unlink(SEM_NAME_TARGET);
    sem_unlink(SEM_NAME_VOTE);
    sem_unlink(SEM_NAME_WINNER);

    sigemptyset(&mask_block);
    sigaddset(&mask_block, SIGUSR1);
    sigaddset(&mask_block, SIGUSR2);
    sigaddset(&mask_block, SIGALRM);
    sigemptyset(&mask_empty);

    if (sigprocmask(SIG_BLOCK, &mask_block, NULL) < 0)
    {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    if (argc < 3)
    {
        fprintf(stderr, "Usage ./miner < N_SECS > < N_THREADS >\n");
        exit(EXIT_FAILURE);
    }
    n_seconds = atoi(argv[1]);
    n_threads = atoi(argv[2]);

    if ((sem_pid = sem_open(SEM_NAME_PID, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_target = sem_open(SEM_NAME_TARGET, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_winner = sem_open(SEM_NAME_WINNER, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_votes = sem_open(SEM_NAME_VOTE, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    pid_act.sa_handler = alarm_handler;
    sigemptyset(&pid_act.sa_mask);
    pid_act.sa_flags = 0;
    sigaction(SIGALRM, &pid_act, NULL);

    sigusr1_act.sa_handler = sigusr1_handler;
    sigemptyset(&sigusr1_act.sa_mask);
    sigusr1_act.sa_flags = 0;
    sigaction(SIGUSR1, &sigusr1_act, NULL);

    sigusr2_act.sa_handler = sigusr2_handler;
    sigemptyset(&sigusr2_act.sa_mask);
    sigusr2_act.sa_flags = 0;
    sigaction(SIGUSR2, &sigusr2_act, NULL);

    while (sem_wait(sem_pid) == -1 && errno == EINTR);
    FILE *checkFile = fopen(PID_FILE, "r");
    if (checkFile == NULL) is_first_miner = 1;
    else
    {
        if (fscanf(checkFile, "%d", &temp) != 1) is_first_miner = 1;
        fclose(checkFile);
    }

    if ((pidFile = fopen(PID_FILE, "a+")) == NULL)
    {
        sem_post(sem_pid);
        exit(EXIT_FAILURE);
    }
    fprintf(pidFile, "%d\n", getpid());
    printf("Miner %d added to system\n", getpid());
    fclose(pidFile);
    sem_post(sem_pid);

    alarm(n_seconds);

    if (is_first_miner)
    {
        while (sem_wait(sem_target) == -1 && errno == EINTR);
        if ((targetFile = fopen(TARGET_FILE, "w+")) != NULL)
        {
            fprintf(targetFile, "%d", TARGET_INIT);
            fclose(targetFile);
        }
        sem_post(sem_target);

        while (total_miners < 2)
        {
            if (time_to_exit) miner_shutdown();
            while (sem_wait(sem_pid) == -1 && errno == EINTR);
            if ((pidFile = fopen(PID_FILE, "r")) != NULL)
            {
                total_miners = 0;
                while (fscanf(pidFile, "%d", &temp) == 1) total_miners++;
                fclose(pidFile);
            }
            sem_post(sem_pid);
            if (total_miners < 2) sleep(1);
        }

        while (sem_wait(sem_pid) == -1 && errno == EINTR);
        pidFile = fopen(PID_FILE, "r");
        if (pidFile != NULL)
        {
            while (fscanf(pidFile, "%d", &temp) == 1)
                kill(temp, SIGUSR1);
            fclose(pidFile);
        }
        sem_post(sem_pid);
    }

    while (1)
    {
        if (time_to_exit) miner_shutdown();

        if (start_mining == 1)
        {
            start_mining = 0;
            findSolution = 0;

            while (sem_wait(sem_target) == -1 && errno == EINTR);
            targetFile = fopen(TARGET_FILE, "r");
            if (targetFile != NULL)
            {
                fscanf(targetFile, "%d", &globalTarget);
                fclose(targetFile);
            }
            sem_post(sem_target);

            args = malloc(sizeof(Thread_args) * n_threads);
            threads = malloc(sizeof(pthread_t) * n_threads);

            for (int i = 0; i < n_threads; i++)
            {
                args[i].start = i * (POW_LIMIT / n_threads);
                args[i].end = (i + 1) * (POW_LIMIT / n_threads);
                pthread_create(&threads[i], NULL, miner, &args[i]);
            }
            
            sigprocmask(SIG_UNBLOCK, &mask_block, NULL);
            for (int i = 0; i < n_threads; i++)
                pthread_join(threads[i], NULL);
            sigprocmask(SIG_BLOCK, &mask_block, NULL);

            free(args);
            free(threads);

            if (time_to_exit) miner_shutdown();

            if (findSolution == 1 && !start_voting)
            {
                is_winner = 1;
            }
        }

        if (is_winner == 1)
        {
            is_winner = 0;
            round_id++;
            int saved_target = globalTarget;

            while (sem_wait(sem_target) == -1 && errno == EINTR);
            targetFile = fopen(TARGET_FILE, "w");
            if (targetFile != NULL)
            {
                fprintf(targetFile, "%d", globalSolution);
                fclose(targetFile);
            }
            sem_post(sem_target);

            int expected_votes = 0;
            while (sem_wait(sem_pid) == -1 && errno == EINTR);
            pidFile = fopen(PID_FILE, "r");
            if (pidFile != NULL)
            {
                while (fscanf(pidFile, "%d", &temp) == 1)
                {
                    if (temp != getpid())
                    {
                        if (kill(temp, SIGUSR2) == 0) expected_votes++;
                    }
                }
                fclose(pidFile);
            }
            sem_post(sem_pid);

            if (expected_votes == 0) usleep(500000);

            int total_v = 0, y_v = 0, n_v = 0, tries = 0;
            char v_char;

            while (total_v < expected_votes && tries < 50)
            {
                usleep(100000);
                tries++;
                total_v = 0; y_v = 0; n_v = 0;

                while (sem_wait(sem_votes) == -1 && errno == EINTR);
                voteFile = fopen(VOTE_FILE, "r");
                if (voteFile != NULL)
                {
                    while (fscanf(voteFile, " %c", &v_char) == 1)
                    {
                        if (v_char == 'Y') { y_v++; total_v++; }
                        else if (v_char == 'N') { n_v++; total_v++; }
                    }
                    fclose(voteFile);
                }
                sem_post(sem_votes);
            }

            printf("Winner %d -> [ ", getpid());
            for (int i = 0; i < y_v; i++) printf("Y ");
            for (int i = 0; i < n_v; i++) printf("N ");

            if (y_v >= n_v && (expected_votes == 0 || total_v > 0))
            {
                printf("] -> Accepted\n");
                my_coins++;

                char log_name[32];
                sprintf(log_name, "%d.txt", getpid());
                FILE *logFile = fopen(log_name, "a");
                if (logFile)
                {
                    fprintf(logFile, "Id:      %d\n", round_id);
                    fprintf(logFile, "Winner:  %d\n", getpid());
                    fprintf(logFile, "Target:  %d\n", saved_target);
                    fprintf(logFile, "Solution: %d (validated)\n", globalSolution);
                    fprintf(logFile, "Votes:   %d/%d\n", y_v, total_v);
                    fprintf(logFile, "Wallets: %d:%d\n\n", getpid(), my_coins);
                    fclose(logFile);
                }
            }
            else
            {
                printf("] -> Rejected\n");
                while (sem_wait(sem_target) == -1 && errno == EINTR);
                targetFile = fopen(TARGET_FILE, "w");
                if (targetFile) {
                    fprintf(targetFile, "%d", saved_target);
                    fclose(targetFile);
                }
                sem_post(sem_target);
            }
            fflush(stdout);

            while (sem_wait(sem_votes) == -1 && errno == EINTR);
            voteFile = fopen(VOTE_FILE, "w");
            if (voteFile != NULL) fclose(voteFile);
            sem_post(sem_votes);

            sem_post(sem_winner);

            while (sem_wait(sem_pid) == -1 && errno == EINTR);
            pidFile = fopen(PID_FILE, "r");
            if (pidFile != NULL)
            {
                while (fscanf(pidFile, "%d", &temp) == 1)
                    kill(temp, SIGUSR1);
                fclose(pidFile);
            }
            sem_post(sem_pid);
        }

        if (start_voting == 1)
        {
            start_voting = 0;
            while (sem_wait(sem_target) == -1 && errno == EINTR);
            targetFile = fopen(TARGET_FILE, "r");
            if (targetFile != NULL)
            {
                fscanf(targetFile, "%d", &solution_escrita);
                fclose(targetFile);
            }
            sem_post(sem_target);

            while (sem_wait(sem_votes) == -1 && errno == EINTR);
            voteFile = fopen(VOTE_FILE, "a+");
            if (voteFile != NULL)
            {
                if (pow_hash(solution_escrita) == globalTarget)
                    fprintf(voteFile, "Y\n");
                else
                    fprintf(voteFile, "N\n");
                fclose(voteFile);
            }
            sem_post(sem_votes);
        }

        if (start_mining == 0 && start_voting == 0 && is_winner == 0 && !time_to_exit)
        {
            sigsuspend(&mask_empty);
        }
    }
    return 0;
}