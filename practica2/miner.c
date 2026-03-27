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

#define PID_FILE "pids.pid"
#define TARGET_FILE "target.tgt"
#define SEM_NAME_PID "/miner_pid"

sem_t *sem_pid=NULL;
sem_t* sem_votes=NULL;
sem_t* sem_winner=NULL;

void print_all_miners(FILE *f) {
    rewind(f);
    int pid;
    printf("Current miners in system: \n");
    while (fscanf(f, "%d", &pid) == 1) {
        fprintf(stdout,"  %d\n", pid);
    }
}

void alarm_handler(int sig)
{   
    int  pid;
    pid_t myPid;
    int remaining_miner=0;
    sem_wait(sem_pid);
    FILE *pidFile=fopen(PID_FILE,"r+");
    FILE *tempFile=fopen("temp.pid","w+");
    if(pidFile==NULL || tempFile==NULL)
    {   
        if(pidFile!=NULL)fclose(pidFile);
        if(tempFile!=NULL)fclose(tempFile);
        perror("Error fichero");
        sem_post(sem_pid);
        exit(EXIT_FAILURE);
    }

    myPid=getpid();
    while(fscanf(pidFile,"%d",&pid)==1)
    {
        if((pid_t)pid!=myPid)
        {
            remaining_miner++;
            fprintf(tempFile,"%d\n",pid);
        }
    }

    if(pidFile!=NULL)fclose(pidFile);
    if(tempFile!=NULL)fclose(tempFile);

    fprintf(stdout,"Miner %jd exited system\n", (intmax_t)myPid);

    if(remaining_miner==0)
    {
        remove(PID_FILE);
        remove("temp.pid");
        sem_post(sem_pid);
        sem_unlink(SEM_NAME_PID);
        
    }else{
        remove(PID_FILE);
        rename("temp.pid",PID_FILE);
        FILE *file = fopen(PID_FILE, "r");
        if (file != NULL) {
            print_all_miners(file);
            fclose(file);
        }

        sem_post(sem_pid);
    }

    exit(EXIT_SUCCESS);

}

int main(int argc, char *argv[])
{   
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

    if((sem_pid=sem_open(SEM_NAME_PID,O_CREAT,S_IRUSR|S_IWUSR,1))==SEM_FAILED)
    {
        perror("sem open");
        exit(EXIT_FAILURE);
    }

    n_seconds=atoi(argv[1]);
    n_threads=atoi(argv[2]);

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


    if((pidFile=fopen(PID_FILE,"a+"))==NULL)
    {
        sem_post(sem_pid);
        perror("Error fichero");
        exit(EXIT_FAILURE);
    }
    fprintf(pidFile,"%jd\n",(intmax_t)getpid());
    fprintf(stdout,"Miner %jd added to system\n", (intmax_t)getpid());
    print_all_miners(pidFile);
    if(pidFile!=NULL)fclose(pidFile);
    sem_post(sem_pid);

    // write at the file
    alarm(n_seconds);

    while(1)
    {
        pause();
    }
    return 0;
}