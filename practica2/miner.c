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
#include<unistd.h>

#define PID_FILE "pids.pid"
#define TARGET_FILE "target.tgt"
#define SEM_NAME_PID "/miner_pid"

void alarm_handler(int sig)
{
    FILE *pidFile=
}

int main(int argc, char *argv[])
{
    sem_t *sem_pid=NULL;
    sem_t* sem_votes=NULL;
    sem_t* sem_winner=NULL;
    int n_seconds;
    int n_threads;
    struct sigaction act;

    FILE *pidFile=NULL;
    //Argument comprobation
    if (argc < 3)
    {
        fprintf(stderr, "Usage ./miner < N_SECS > < N_THREADS >\n");
        fprintf(stdout,"Miner exited unexpectedly\n");
        exit(EXIT_FAILURE);
    }

    if((sem_pid=sem_open(SEM_NAME_PID,O_CREAT|O_EXCL,S_IRUSR|S_IWUSR,1))==SEM_FAILED)
    {
        perror("sem open");
        exit(EXIT_FAILURE);
    }

    n_seconds=atoi(argv[1]);
    n_threads=atoi(argv[2]);
    // write at the file
    alarm(n_seconds);

    // set the sigal action of alrm 
    sigemptyset(&(act.sa_mask));
    act.sa_flags=0;

    act.sa_handler=alarm_handler;
    if(sigaction(SIGALRM,&act,NULL)<0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    //store pid in the file
    sem_wait(sem_pid);
    if(pidFile=fopen(PID_FILE,"a")==NULL)
    {
        sem_post(sem_pid)
        perror("")
    }
    fprintf(pidFile,"%jd\n",(intmax_t)getpid());
    sem_post(sem_pid);
    
}