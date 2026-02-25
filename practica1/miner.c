#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "pow.h"
#include <string.h>
#include <stdint.h>

// Global variable
int globalSolution = 0;
int findSolution = 0;
int globalTarget = 0;

// miner function argument
typedef struct Thread_args
{
    long int start;
    long int end;
    // long int target;

} Thread_args;

typedef struct MinerMsg
{
    int round;
    int target;
    int solution;
} MinerMsg;
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
    for (long int i = t->start; i < t->end && !findSolution; i++)
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

/*error = pthread_create(&h2, NULL, slow_printf, world);
  if (error != 0) {
    fprintf(stderr, "pthread_create: %s\n", strerror(error));
    exit(EXIT_FAILURE);
  }*/

int main(int argc, char *argv[])
{

    pthread_t *threads;
    Thread_args *args;
    int rounds;
    int nThreads;
    int targetIni;

    int pipe_miner_reg[2]; // Miner to pipe
    int pipe_reg_miner[2]; // Reg to miner

    pid_t registradorPid;
    ssize_t nbytes;

    if (argc < 4)
    {
        fprintf(stderr, "Usage ./miner < TARGET_INI > < ROUNDS > < N_THREADS >\n");
        exit(EXIT_FAILURE);
    }

    // Read arguments
    targetIni = atoi(argv[1]);
    rounds = atoi(argv[2]);
    nThreads = atoi(argv[3]);

    // Create n_threads threads_args
    if ((args = malloc(sizeof(Thread_args) * nThreads)) == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    if ((threads = malloc(sizeof(pthread_t) * nThreads)) == NULL)
    {
        free(args);
        fprintf(stderr, "Memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Asignation of target init
    globalTarget = targetIni;

    for (int i = 0; i < nThreads; ++i)
    {
        args[i].start = i * (POW_LIMIT) / nThreads;
        args[i].end = (i + 1) * (POW_LIMIT) / nThreads;
    }

    // Create process for Registration
    if (pipe(pipe_miner_reg) == -1 || pipe(pipe_reg_miner) == -1)
    {
        free(args);
        free(threads);
        fprintf(stderr, "Wrong at pipe\n");
        exit(EXIT_FAILURE);
    }

    registradorPid = fork();
    if (registradorPid > 0)
    {
        close(pipe_miner_reg[0]);
        close(pipe_reg_miner[1]);

        for (int j = 0; j < rounds; ++j)
        {
            for (long long i = 0; i < nThreads; ++i)
            {
                // Create threads
                int error = pthread_create(&threads[i], NULL, miner, &args[i]);
                if (error != 0)
                {
                    free(args);
                    free(threads);
                    fprintf(stderr, "pthread_create: %s\n", strerror(error));
                    exit(EXIT_FAILURE);
                }
            }
            // Wait until all threads finished
            for (long long i = 0; i < nThreads; ++i)
            {
                pthread_join(threads[i], NULL);
            }
            fprintf(stdout, "Solution ronda %d: %d\n", j + 1, globalSolution);
            fprintf(stdout, "comprobation: %ld\n", pow_hash(globalSolution));

            MinerMsg msg;
            msg.round = j + 1;
            msg.target = globalTarget;
            msg.solution = globalSolution;

            write(pipe_miner_reg[1], &msg, sizeof(msg));

            int answer;
            read(pipe_reg_miner[0], &answer, sizeof(answer));
            // set new target
            globalTarget = globalSolution;
            findSolution = 0;
        }
        MinerMsg end_msg = {0, 0, -1};
        write(pipe_miner_reg[1], &end_msg, sizeof(end_msg));

        close(pipe_miner_reg[1]);
        close(pipe_reg_miner[0]);

        // Wait for the registration process
        wait(NULL);
    }
    else if (registradorPid == 0)
    {
        close(pipe_miner_reg[1]);
        close(pipe_reg_miner[0]);

        char registerFile[256];
        /* File to write*/
        sprintf(registerFile, "%jd.log", (intmax_t)getppid());
        int fd_register = open(registerFile, O_CREAT | O_WRONLY, 0644);
        /* read message from Miner process */
        nbytes = 0;
        MinerMsg msg;
        do
        {
            nbytes = read(pipe_miner_reg[0], &msg, sizeof(msg));
            if (nbytes == -1)
            {
                fprintf(stderr, "Error readBuffer\n");
                exit(EXIT_FAILURE);
            }
            if (nbytes > 0)
            {
                // Process Registration need to be finished
                if (msg.solution == -1)
                {
                    break;
                }

                dprintf(fd_register, "Id:         [%d]\n", msg.round);
                dprintf(fd_register, "Winner:     [%jd]\n", (intmax_t)getppid());
                dprintf(fd_register, "Target:     [%d]\n", msg.target);
                dprintf(fd_register, "Solution:   [%d] [(%s)]\n", msg.solution, "Accepted");
                dprintf(fd_register, "Votes:      [%d]/[%d]\n", msg.round, msg.round);
                dprintf(fd_register, "Wallets:    [%jd]:[%d]\n", (intmax_t)getppid(), msg.round);
                dprintf(fd_register, "\n");

                //confirmation to miner process
                int answer=1;
                write(pipe_reg_miner[1],&answer,sizeof(answer));
            }
        } while (nbytes != 0);
        close(fd_register);
        close(pipe_reg_miner[1]);
        close(pipe_miner_reg[0]);
    }
    free(args);
    free(threads);

}