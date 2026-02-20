#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include "pow.h"

#define ROUNDS 5
#define N_THREADS 8

int globalSolution = 0;
int findSolution = 0;

typedef struct Thread_args
{
    long int start;
    long int end;
    long int target;

}Thread_args;

void *miner(Thread_args *t)
{
    for (long int i = t->start; i < t->end && !findSolution; i++)
    {
        if (pow_hash(i) == t->target)
        {
            findSolution = 1;
            globalSolution = i;
        }
    }
}

/*error = pthread_create(&h2, NULL, slow_printf, world);
  if (error != 0) {
    fprintf(stderr, "pthread_create: %s\n", strerror(error));
    exit(EXIT_FAILURE);
  }*/

int main(int argc, char *argv)
{

    pthread_t *h;
    Thread_args t[N_THREADS];
    if (argc < 4)
    {
        fprintf(stderr, "Usage ./miner < TARGET_INI > < ROUNDS > < N_THREADS >\n");
    }
    if ((h = malloc(sizeof(pthread_t) * N_THREADS)) == NULL)
    {
        fprintf(stderr, "malloc\n");
        exit(EXIT_FAILURE);
    }
    for (int j = 0; j < ROUNDS; j++)
    {
        for (int i = 0; i < N_THREADS; i++)
        {
            int error = pthread_create(&h[i], NULL, miner,&t[i]);
            if (error != 0)
            {
                fprintf(stderr, "pthread_create: %s\n", strerror(error));
                exit(EXIT_FAILURE);
            }

            return 0;
        }

    }
}