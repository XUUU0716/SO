#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include "pow.h"
#include <string.h>

#define ROUNDS 5
#define N_THREADS 8

int globalSolution = 0;
int findSolution = 0;
int globalTarget=0;
typedef struct Thread_args
{
    long int start;
    long int end;
    //long int target;

}Thread_args;

void *miner(void *arg)
{
    Thread_args *t=(Thread_args*)arg;
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

    if (argc < 4)
    {
        fprintf(stderr, "Usage ./miner < TARGET_INI > < ROUNDS > < N_THREADS >\n");
        exit(EXIT_FAILURE);
    }

    // Read arguments 
    targetIni=atoi(argv[1]);
    rounds=atoi(argv[2]);
    nThreads=atoi(argv[3]);

    // Create n_threads threads_args
    if((args=malloc(sizeof(Thread_args)*nThreads))==NULL)
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
    
    //Asignation of target init
    globalTarget=targetIni;

    for (long long j = 0; j < rounds; ++j)
    {
        for (long long i = 0; i < nThreads; ++i)
        {   
            args[i].start=i*(POW_LIMIT)/nThreads;
            args[i].end=(i+1)*(POW_LIMIT)/nThreads;

            int error = pthread_create(&threads[i], NULL, miner, &args[i]);
            if (error != 0)
            {
                fprintf(stderr, "pthread_create: %s\n", strerror(error));
                exit(EXIT_FAILURE);
            }

        }
        for(long long i=0;i<nThreads;++i)
        {
            pthread_join(threads[i],NULL);
        }
        fprintf(stdout,"Solution ronda %lld: %d\n",j,globalSolution);

        //set new target 
        findSolution=0;
        globalTarget=globalSolution;
    }
    free(args);
    free(threads);
    return 0;
}