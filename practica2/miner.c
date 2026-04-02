/**
 * @brief It defines a simulation of blockchain and minering
 *
 * @file miner.c
 * @author Shaofan Xu, Javier Santa Maria
 * @version 0
 * @date 13-03-2026
 * @copyright GNU Public License
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
#include "pow.h"

#define PID_FILE "pids.pid"
#define TARGET_FILE "target.tgt"
#define VOTE_FILE "vote.txt"
#define SEM_NAME_PID "/miner_pid"
#define SEM_NAME_TARGET "/miner_target"
#define SEM_NAME_WINNER "/miner_winner"
#define SEM_NAME_VOTE "/miner_vote"

#define MAX_INTENTO 100

#define TARGET_INIT 0

sem_t *sem_pid = NULL;
sem_t *sem_votes = NULL;
sem_t *sem_winner = NULL;
sem_t *sem_target = NULL;

/**
 * @brief Thread_args
 * This structure store all information of argument needed to executed the function
 */
typedef struct Thread_args
{
    long int start; // The start of the intervals to search
    long int end;   // The end of the intervals to search

} Thread_args;

atomic_int globalSolution = 0;
atomic_int findSolution = 0;
int globalTarget = 0;
int n_threads = 0;
pthread_t *threads;
Thread_args *args;

volatile sig_atomic_t start_mining = 0;
volatile sig_atomic_t start_voting = 0;

/**
 * @brief MinerMsg
 * This structure store all information that process Miner have to pass to process Registrador
 */
typedef struct MinerMsg
{
    int round;    // The round of the game
    int target;   // The target that are searching
    int solution; // The solution founded
} MinerMsg;

/**
 * @brief This function print all miner 
 * @author Shaofan Xu
 *
 * @param f the file to print
 */
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

/**
 * @brief This function remove pid from the pid file
 * @author Shaofan Xu
 *
 * @param sig the signal that receive
 */
void alarm_handler(int sig)
{
    int pid;
    pid_t myPid;
    int remaining_miner = 0;
    sem_wait(sem_pid);
    FILE *pidFile = fopen(PID_FILE, "r+");
    FILE *tempFile = fopen("temp.pid", "w+");

    if (pidFile == NULL || tempFile == NULL)
    {
        if (pidFile != NULL)
            fclose(pidFile);
        if (tempFile != NULL)
            fclose(tempFile);
        perror("Error fichero");
        sem_post(sem_pid);
        exit(EXIT_FAILURE);
    }

    myPid = getpid();
    while (fscanf(pidFile, "%d", &pid) == 1)
    {
        if ((pid_t)pid != myPid)
        {
            remaining_miner++;
            fprintf(tempFile, "%d\n", pid);
        }
    }

    if (pidFile != NULL)
        fclose(pidFile);
    if (tempFile != NULL)
        fclose(tempFile);

    fprintf(stdout, "Miner %jd exited system\n", (intmax_t)myPid);

    if (remaining_miner == 0)
    {
        remove(PID_FILE);
        remove("temp.pid");
        sem_post(sem_pid);
        sem_unlink(SEM_NAME_PID);
        sem_unlink(SEM_NAME_TARGET);
        sem_unlink(SEM_NAME_VOTE);
        sem_unlink(SEM_NAME_WINNER);
    }
    else
    {
        remove(PID_FILE);
        rename("temp.pid", PID_FILE);
        FILE *file = fopen(PID_FILE, "r");
        if (file != NULL)
        {
            print_all_miners(file);
            fclose(file);
        }

        sem_post(sem_pid);
    }

    exit(EXIT_SUCCESS);
}

/**
 * @brief This function set a flag to true, start minering
 * @author Shaofan Xu
 *
 * @param sig the signal that receive
 */
void sigusr1_handler(int sig)
{
    start_mining = 1;
}

/**
 * @brief This function set a flag to true, start voting
 * @author Shaofan Xu
 *
 * @param sig the signal that receive
 */
void sigusr2_handler(int sig)
{
    start_voting = 1;
}

/**
 * @brief This function try to solve a hash problem by brute force
 * @author Shaofan Xu
 *
 * @param arg pointer to parameter of function
 * @return NULL
 */
void *miner(void *arg)
{
    Thread_args *t = (Thread_args *)arg;
    for (long int i = t->start; i < t->end && !findSolution && !start_voting; i++)
    {
        if (pow_hash(i) == globalTarget)
        {
            findSolution = 1;
            globalSolution = i;
            return NULL;
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int n_seconds;
    int ronda = 0;
    int is_first_miner = 0;
    struct sigaction pid_act, sigusr1_act, sigusr2_act;
    int temp;
    int total_miners = 0;
    int solution_escrita = 0;
    int is_winner = 0;
    int my_coins = 0;

    char reg_filename[64];

    FILE *checkFile = NULL;
    FILE *targetFile = NULL;
    FILE *pidFile = NULL;
    FILE *voteFile = NULL;
    FILE *regFile = NULL;

    sem_unlink(SEM_NAME_PID);
    sem_unlink(SEM_NAME_TARGET);
    sem_unlink(SEM_NAME_VOTE);
    sem_unlink(SEM_NAME_WINNER);

    // Argument comprobation
    if (argc < 3)
    {
        fprintf(stderr, "Usage ./miner < N_SECS > < N_THREADS >\n");
        fprintf(stdout, "Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
    }
    // crear semaforos
    if ((sem_pid = sem_open(SEM_NAME_PID, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem open pid");
        exit(EXIT_FAILURE);
    }

    if ((sem_target = sem_open(SEM_NAME_TARGET, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem open target");
        exit(EXIT_FAILURE);
    }

    if ((sem_winner = sem_open(SEM_NAME_WINNER, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open winner");
        exit(EXIT_FAILURE);
    }

    if ((sem_votes = sem_open(SEM_NAME_VOTE, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open vote");
        exit(EXIT_FAILURE);
    }
    n_seconds = atoi(argv[1]);
    n_threads = atoi(argv[2]);

    // set the sigal action of alrm
    sigemptyset(&(pid_act.sa_mask));
    sigemptyset(&(sigusr1_act.sa_mask));
    sigemptyset(&(sigusr2_act.sa_mask));

    pid_act.sa_flags = 0;
    sigusr1_act.sa_flags = 0;
    sigusr2_act.sa_flags = 0;

    pid_act.sa_handler = alarm_handler;
    sigusr1_act.sa_handler = sigusr1_handler;
    sigusr2_act.sa_handler = sigusr2_handler;

    if (sigaction(SIGALRM, &pid_act, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR1, &sigusr1_act, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGUSR2, &sigusr2_act, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // store pid in the file
    sem_wait(sem_pid);

    checkFile = fopen(PID_FILE, "r");
    if (checkFile == NULL)
        is_first_miner = 1;
    else
    {
        if (fscanf(checkFile, "%d", &temp) != 1)
        {
            is_first_miner = 1;
        }
        fclose(checkFile);
    }

    if ((pidFile = fopen(PID_FILE, "a+")) == NULL)
    {
        sem_post(sem_pid);
        perror("Error fichero");
        exit(EXIT_FAILURE);
    }
    fprintf(pidFile, "%jd\n", (intmax_t)getpid());
    fprintf(stdout, "Miner %jd added to system\n", (intmax_t)getpid());
    print_all_miners(pidFile);
    if (pidFile != NULL)
        fclose(pidFile);
    sem_post(sem_pid);

    // write at the file
    alarm(n_seconds);

    if (is_first_miner)
    {   
        remove(VOTE_FILE);
        remove(TARGET_FILE);
        sem_wait(sem_target);
        if ((targetFile = fopen(TARGET_FILE, "w+")) == NULL)
        {
            perror("Error fichero");
            sem_post(sem_target);
            exit(EXIT_FAILURE);
        }
        fprintf(targetFile, "%d", TARGET_INIT);
        fclose(targetFile);
        sem_post(sem_target);

        while (total_miners < 2)
        {
            sem_wait(sem_pid);
            if ((pidFile = fopen(PID_FILE, "r")) == NULL)
            {
                perror("Error fichero");
                sem_post(sem_pid);
                exit(EXIT_FAILURE);
            }
            total_miners = 0;
            while (fscanf(pidFile, "%d", &temp) == 1)
            {
                total_miners++;
            }
            fclose(pidFile);
            sem_post(sem_pid);
            // Espera si hay mas procesos
            if (total_miners < 2)
                sleep(1);
        }

        sem_wait(sem_pid);
        pidFile = fopen(PID_FILE, "r");
        if (pidFile == NULL)
        {
            perror("Error fichero");
            sem_post(sem_pid);
            exit(EXIT_FAILURE);
        }
        while (fscanf(pidFile, "%d", &temp) == 1)
        {
            if (kill(temp, SIGUSR1) == -1)
            {
                perror("Error al lanzar señal");
            }
        }
        fclose(pidFile);
        sem_post(sem_pid);
    }
    while (1)
    {
        if (start_mining == 1)
        {
            ronda++;
            start_mining = 0;
            findSolution = 0;
            start_voting = 0;
            sem_wait(sem_target);
            targetFile = fopen(TARGET_FILE, "r");
            if (targetFile != NULL)
            {
                fscanf(targetFile, "%d", &globalTarget);
                fclose(targetFile);
            }
            sem_post(sem_target);
            // Create n_threads threads_args
            if ((args = malloc(sizeof(Thread_args) * n_threads)) == NULL)
            {
                fprintf(stderr, "Memory allocation error\n");
                fprintf(stdout, "Miner exited unexpectedly\n");
                exit(EXIT_FAILURE);
            }

            // Create an array of threads
            if ((threads = malloc(sizeof(pthread_t) * n_threads)) == NULL)
            {
                free(args);
                fprintf(stderr, "Memory allocation error\n");
                fprintf(stdout, "Miner exited unexpectedly\n");
                exit(EXIT_FAILURE);
            }

            // Initialize the threads arguments
            for (int i = 0; i < n_threads; ++i)
            {
                args[i].start = i * (POW_LIMIT) / n_threads;
                args[i].end = (i + 1) * (POW_LIMIT) / n_threads;
            }

            for (long long i = 0; i < n_threads; ++i)
            {
                // Create threads
                int error = pthread_create(&threads[i], NULL, miner, &args[i]);
                // Error control
                if (error != 0)
                {
                    free(args);
                    free(threads);
                    fprintf(stderr, "pthread_create: %s\n", strerror(error));
                    fprintf(stdout, "Miner exited unexpectedly\n");
                    exit(EXIT_FAILURE);
                }
            }

            for (long long i = 0; i < n_threads; ++i)
            {
                pthread_join(threads[i], NULL);
            }

            // Free all memory
            free(args);
            free(threads);

            if (findSolution == 1)
            {
                // Intenta ser el winner
                if (sem_trywait(sem_winner) == 0)
                {
                    is_winner = 1;
                    sem_wait(sem_target);
                    targetFile = fopen(TARGET_FILE, "w");
                    if (targetFile == NULL)
                    {
                        perror("Error Fichero");
                        sem_post(sem_target);
                        sem_post(sem_winner);
                        exit(EXIT_FAILURE);
                    }
                    fprintf(targetFile, "%d", globalSolution);
                    fclose(targetFile);
                    sem_post(sem_target);

                    sem_wait(sem_pid);
                    pidFile = fopen(PID_FILE, "r");
                    if (pidFile != NULL)
                    {
                        while (fscanf(pidFile, "%d", &temp) == 1)
                        {
                            if (temp != getpid())
                            {
                                kill(temp, SIGUSR2);
                            }
                        }
                        fclose(pidFile);
                    }
                    else
                    {
                        sem_post(sem_pid);
                        sem_post(sem_winner);
                        exit(EXIT_FAILURE);
                    }
                    sem_post(sem_pid);

                    start_voting = 1;
                }
            }
            if (is_winner == 1)
            {

                int expected_votes = -1; // Debe ser 0, pero quitando a si mismo da -1;
                sem_wait(sem_pid);
                pidFile = fopen(PID_FILE, "r");
                if (pidFile == NULL)
                {
                    perror("Error fichero");
                    sem_post(sem_pid);
                    exit(EXIT_FAILURE);
                }
                while (fscanf(pidFile, "%d", &temp) == 1)
                {
                    expected_votes++;
                }
                fclose(pidFile);
                sem_post(sem_pid);

                // Contar los votos
                int total_votes = 0;
                int intentos = 0;
                int y_votes = 0;
                int n_votes = 0;
                char vote;
                while (total_votes < expected_votes && intentos < 50)
                {
                    usleep(100000);
                    total_votes = 0;
                    y_votes = 0;
                    n_votes = 0;
                    intentos++;
                    sem_wait(sem_votes);
                    voteFile = fopen(VOTE_FILE, "r");
                    if (voteFile == NULL)
                    {
                        sem_post(sem_votes);
                        exit(EXIT_FAILURE);
                    }
                    while (fscanf(voteFile, " %c", &vote) == 1)
                    {
                        total_votes++;
                        if (vote == 'Y')
                            y_votes++;
                        else if (vote == 'N')
                            n_votes++;
                    }
                    fclose(voteFile);
                    sem_post(sem_votes);
                }
                fprintf(stdout, "Winner %jd => [ ", (intmax_t)getpid());
                for (int i = 0; i < y_votes; i++)
                    fprintf(stdout, "Y ");
                for (int i = 0; i < n_votes; i++)
                    fprintf(stdout, "N ");

                if (y_votes >= n_votes)
                {
                    fprintf(stdout, "] => Accepted\n");
                    my_coins++;

                    sprintf(reg_filename, "%jd.txt", (intmax_t)getpid());

                    regFile = fopen(reg_filename, "a");
                    if (regFile == NULL)
                    {
                        exit(EXIT_FAILURE);
                    }
                    // Imprime el resultado
                    fprintf(regFile, "Id:         %d\n", ronda);
                    fprintf(regFile, "Winner:     %jd\n", (intmax_t)getpid());
                    fprintf(regFile, "Target:     %d\n", globalTarget);
                    fprintf(regFile, "Solution:   %d (validated)\n", globalSolution);
                    fprintf(regFile, "Votes:      %d/%d\n", y_votes, expected_votes);
                    fprintf(regFile, "Wallets:    %jd:%d\n", (intmax_t)getpid(), my_coins);
                    fprintf(regFile, "\n");
                    fclose(regFile);
                }
                else
                {
                    fprintf(stdout, "] => Rejected\n");
                }

                // vaciar el archivo
                sem_wait(sem_votes);
                voteFile = fopen(VOTE_FILE, "w");
                if (voteFile != NULL)
                    fclose(voteFile);
                sem_post(sem_votes);

                // Actualizar target
                if (y_votes >= n_votes)
                {
                    sem_wait(sem_target);
                    targetFile = fopen(TARGET_FILE, "w");
                    if (targetFile != NULL)
                    {
                        fprintf(targetFile, "%d", globalSolution);
                        fclose(targetFile);
                    }
                    sem_post(sem_target);
                }

                sem_post(sem_winner);

                // Empezar nueva ronda
                sem_wait(sem_pid);
                pidFile = fopen(PID_FILE, "r");
                if (pidFile != NULL)
                {

                    while (fscanf(pidFile, "%d", &temp) == 1)
                    {
                        kill(temp, SIGUSR1);
                    }
                    fclose(pidFile);
                }
                sem_post(sem_pid);
                is_winner = 0;
            }
            else
            {
                // Votacion
                while (start_voting == 0)
                {
                    pause();
                }
                sem_wait(sem_target);
                targetFile = fopen(TARGET_FILE, "r");
                if (targetFile == NULL)
                {
                    sem_post(sem_target);
                    perror("Error fichero");
                    exit(EXIT_FAILURE);
                }

                // Lee la solucion escrita
                fscanf(targetFile, "%d", &solution_escrita);
                fclose(targetFile);
                sem_post(sem_target);

                sem_wait(sem_votes);
                voteFile = fopen(VOTE_FILE, "a+");
                if (voteFile == NULL)
                {
                    sem_post(sem_votes);
                    exit(EXIT_FAILURE);
                }
                // Vota yes o no, comprobando la solucion
                if (pow_hash(solution_escrita) == globalTarget)
                {
                    fprintf(voteFile, "Y\n");
                }
                else
                {
                    fprintf(voteFile, "N\n");
                }
                fclose(voteFile);
                sem_post(sem_votes);
                start_voting = 0;
            }
        }

        // Si no hay ningun señal, se espera
        if (start_mining == 0 && start_voting == 0)
        {
            pause();
        }
    }
    return 0;
}
