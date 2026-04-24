/**
 * @brief It defines a simulation of motorization blockchain and minering
 *
 * @file monitor.c
 * @author Shaofan Xu, Javier Santamaria
 * @version 1.0
 * @date 24/04/2026
 */
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/types.h>

#define MQ_MINER_COMPROBADOR "/mq_monitor"  //Nombre de la cola
volatile sig_atomic_t isMessageInQueue=0;   //Si mensaje en cola o no

/**
 * Atributos para la cola
 */
struct mq_attr attributes = {.mq_flags = 0 ,
.mq_maxmsg = 10 ,
.mq_curmsgs = 0 ,
.mq_msgsize = sizeof ( int ) };

/**
 * @brief Handler functon of alarm sign for comprobador
 * @author Shaofan Xu
 */
void comprobador_alarm_handler(int sig) { isMessageInQueue = 1; }

 /**
  * @brief la entrada de programa
  */
int main(int argc, char *argv[])
{      
    /**
     * El pid de monitor
     */
    pid_t monitor_pid;
    /**
     * El descriptor de cola
     */
    mqd_t queue;


    //Argument comprobation
    if(argc!=3)
    {
        perror("Useage ./monitor < LAG_COMPROBADOR > < LAG_MONITOR >");
        exit(EXIT_FAILURE);
    }

    if((monitor_pid=fork())>0)
    {
        //en la logica de comprobador
        queue=mq_open(MQ_MINER_COMPROBADOR,O_WRONLY,S_IRUSR | S_IWUSR,&attributes);
        while(1)
        {
            if(isMessageInQueue==1)
            {
                isMessageInQueue=0;
            }
        }
        
    }else if(monitor_pid==0)
    {
        //logica de monitor
        queue=mq_open(MQ_MINER_COMPROBADOR,O_CREAT,S_IRUSR | S_IWUSR,&attributes);
        if(queue==(mqd_t)-1)
        {
            perror("Error opening the queue\n");
            exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_SUCCESS);
}