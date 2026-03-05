/**
 * @brief It defines a simulation of blockchain and minering
 *
 * @file miner.c
 * @author Shaofan Xu, Javier Santa Maria
 * @version 0
 * @date 20-02-2026
 * @copyright GNU Public License
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>

#include "pow.h"

// Global variable
atomic_int globalSolution = 0;  //the solution founded by thread
atomic_int findSolution = 0;    //if there is thread that already founded the solution
int globalTarget = 0;           //the target 


/**
 * @brief Thread_args
 * This structure store all information of argument needed to executed the function
 */
typedef struct Thread_args
{
    long int start; // The start of the intervals to search
    long int end;   // The end of the intervals to search

} Thread_args;

/**
 * @brief MinerMsg
 * This structure store all information that process Miner have to pass to process Registrador
 */
typedef struct MinerMsg
{
    int round;  // The round of the game
    int target; // The target that are searching
    int solution;   // The solution founded
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


    //Argument comprobation
    if (argc < 4)
    {
        fprintf(stderr, "Usage ./miner < TARGET_INI > < ROUNDS > < N_THREADS >\n");
        fprintf(stdout,"Miner exited unexpectedly\n");
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
        fprintf(stdout,"Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
    }

    // Create an array of threads
    if ((threads = malloc(sizeof(pthread_t) * nThreads)) == NULL)
    {
        free(args);
        fprintf(stderr, "Memory allocation error\n");
        fprintf(stdout,"Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
    }

    // Asignation of target init
    globalTarget = targetIni;

    // Initialize the threads arguments
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
        fprintf(stdout,"Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
    }

    //Creat a child process Registrador
    registradorPid = fork();
    if (registradorPid > 0)
    {
        int registradorStatus;
        MinerMsg msg;
        MinerMsg end_msg = {0, 0, -1};
        int answer;

        // close pipe that don´t need
        close(pipe_miner_reg[0]);
        close(pipe_reg_miner[1]);

        for (int j = 0; j < rounds; ++j)
        {
            for (long long i = 0; i < nThreads; ++i)
            {
                // Create threads
                int error = pthread_create(&threads[i], NULL, miner, &args[i]);
                // Error control
                if (error != 0)
                {
                    free(args);
                    free(threads);
                    fprintf(stderr, "pthread_create: %s\n", strerror(error));
                    fprintf(stdout,"Miner exited unexpectedly\n");
                    exit(EXIT_FAILURE);
                }
            }
            // Wait until all threads finished
            for (long long i = 0; i < nThreads; ++i)
            {
                pthread_join(threads[i], NULL);
            }

            msg.round = j + 1;
            msg.target = globalTarget;
            msg.solution = globalSolution;

            // Send to process Registrador
            write(pipe_miner_reg[1], &msg, sizeof(msg));

            // Recive confirmation from process Regitrador
            read(pipe_reg_miner[0], &answer, sizeof(answer));

            // set new target
            globalTarget = globalSolution;
            findSolution = 0;
        }
        //Send an end message to process Registrador
        write(pipe_miner_reg[1], &end_msg, sizeof(end_msg));

        //Close all pipe
        close(pipe_miner_reg[1]);
        close(pipe_reg_miner[0]);

        //Wait for the process Registrador
        wait(&registradorStatus);
        if(WIFEXITED(registradorStatus))
        {
            fprintf(stdout,"Logger exited with status %d\n",registradorStatus);
        }else{
            fprintf(stdout,"Logger exited unexpectedly\n");
        }
    }
    else if (registradorPid == 0)
    {
        char registerFile[256];
        int fd_register;
        MinerMsg msg;
        ssize_t nbytes;

        // Close pipe that not going to use
        close(pipe_miner_reg[1]);
        close(pipe_reg_miner[0]);

        /* File to write*/
        sprintf(registerFile, "%jd.log", (intmax_t)getppid());

        fd_register = open(registerFile, O_CREAT | O_WRONLY | O_APPEND, 0644);

        /* read message from Miner process */
        nbytes = 0;
        do
        {
            nbytes = read(pipe_miner_reg[0], &msg, sizeof(msg));
            if (nbytes == -1)
            {
                fprintf(stderr, "Error reading message from Miner\n");
                exit(EXIT_FAILURE);
            }
            if (nbytes > 0)
            {
                char result_status[256];
                int answer=1;

                // Process Registration need to be finished
                if (msg.solution == -1)
                {
                    break;
                }
                if(msg.target%2==0)
                {
                    strcpy(result_status,"validated");
                }else{
                    strcpy(result_status,"rejected");
                }

                //Print at the file
                dprintf(fd_register, "Id:         %d\n", msg.round);
                dprintf(fd_register, "Winner:     %jd\n", (intmax_t)getppid());
                dprintf(fd_register, "Target:     %d\n", msg.target);
                dprintf(fd_register, "Solution:   %d (%s)\n", msg.solution, result_status);
                dprintf(fd_register, "Votes:      %d/%d\n", msg.round, msg.round);
                dprintf(fd_register, "Wallets:    %jd:%d\n", (intmax_t)getppid(), msg.round);
                dprintf(fd_register, "\n");

                //Print at the stdout
                fprintf(stdout, "Solution %s: %d --> %d\n", result_status,msg.target,msg.solution);

                //confirmation to Miner process
                write(pipe_reg_miner[1],&answer,sizeof(answer));
            }
        } while (nbytes != 0);

        //Close all file
        close(fd_register);
        close(pipe_reg_miner[1]);
        close(pipe_miner_reg[0]);
    }
    //Free all memory
    free(args);
    free(threads);

    if(registradorPid>0)
    {
        fprintf(stdout, "Miner exited with status 0\n");
    }
    return 0;
}
