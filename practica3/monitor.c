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
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include "pow.h"
#include "shared_data.h"

#define MQ_MINER_COMPROBADOR "/mq_monitor"  //Nombre de la cola
#define SHM_NAME "/shm_monitor"           //Nombre de memoria compartida          
#define SEM_MUTEX "/sem_monitor_mutex"  //Semaforo de monitor
#define SEM_EMPTY "/sem_monitor_empty"  //Semaforo de monitor
#define SEM_FULL "/sem_monitor_full"    //Semaforo de mofitor
#define MAX_BUFFER 6   //Numero maximo de buffer
#define MAX_MSG 7   //Numero maximo de mensaje en la cola

/**
 * Atributos para la cola
 */
struct mq_attr attributes = {.mq_flags = 0 ,
.mq_maxmsg = MAX_MSG ,
.mq_curmsgs = 0 ,
.mq_msgsize = sizeof(MonitorMsg) };

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
    int shm_fd;
    SharedData *shm_ptr;
    sem_t *sem_mutex, *sem_empty, *sem_full;


    //Argument comprobation
    if(argc!=3)
    {
        fprintf(stderr, "Usage ./monitor < LAG_COMPROBADOR > < LAG_MONITOR >\n");
        exit(EXIT_FAILURE);
    }

    int lag_comprobador = atoi(argv[1]);
    int lag_monitor = atoi(argv[2]);

    // Limpieza de recursos previos por seguridad
    mq_unlink(MQ_MINER_COMPROBADOR);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);

    // Inicializar cola, memoria compartida y semáforos
    queue = mq_open(MQ_MINER_COMPROBADOR, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
    if(queue == (mqd_t)-1)
    {
        perror("Error opening the queue");
        exit(EXIT_FAILURE);
    }

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(shm_fd, sizeof(SharedData));
    shm_ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    shm_ptr->in = 0;
    shm_ptr->out = 0;

    sem_mutex = sem_open(SEM_MUTEX, O_CREAT, S_IRUSR | S_IWUSR, 1);
    sem_empty = sem_open(SEM_EMPTY, O_CREAT, S_IRUSR | S_IWUSR, MAX_BUFFER);
    sem_full = sem_open(SEM_FULL, O_CREAT, S_IRUSR | S_IWUSR, 0);

    monitor_pid=fork();
    if(monitor_pid<0)
    {
        perror("Error fork");
        exit(EXIT_FAILURE);    
    }
    if(monitor_pid>0)
    {
        // PROCESO PADRE: Comprobador
        MonitorMsg msg;
        printf("[Comprobador] Iniciado correctamente. Esperando bloques...\n");

        while(1)
        {
            if(mq_receive(queue, (char*)&msg, sizeof(MonitorMsg), NULL) != -1)
            {
                // Si recibimos el código -1, terminamos la ejecución
                if(msg.target == -1 && msg.solution == -1) {
                    msg.is_valid = -1;
                    sem_wait(sem_empty);
                    sem_wait(sem_mutex);
                    shm_ptr->buffer[shm_ptr->in] = msg;
                    shm_ptr->in = (shm_ptr->in + 1) % MAX_BUFFER;
                    sem_post(sem_mutex);
                    sem_post(sem_full);
                    break;
                }

                // Validar el bloque comprobando el hash (Apartado a)
                if (pow_hash(msg.solution) == msg.target) {
                    sem_wait(sem_empty);
                    sem_wait(sem_mutex);

                    shm_ptr->buffer[shm_ptr->in] = msg;
                    shm_ptr->in = (shm_ptr->in + 1) % MAX_BUFFER;

                    sem_post(sem_mutex);
                    sem_post(sem_full);
                } else {
                    printf("[Comprobador] ALERTA: Bloque inválido recibido (Ronda: %d)\n", msg.round);
                }

                sem_wait(sem_empty);
                sem_wait(sem_mutex);
                shm_ptr->buffer[shm_ptr->in] = msg;
                shm_ptr->in = (shm_ptr->in + 1) % MAX_BUFFER;
                sem_post(sem_mutex);
                sem_post(sem_full);
            }
            usleep(lag_comprobador * 1000); // LAG_COMPROBADOR
        }
        
        // Destrucción de recursos controlada por el padre
        wait(NULL);
        mq_close(queue); mq_unlink(MQ_MINER_COMPROBADOR);
        munmap(shm_ptr, sizeof(SharedData)); shm_unlink(SHM_NAME);
        sem_close(sem_mutex); sem_close(sem_empty); sem_close(sem_full);
        sem_unlink(SEM_MUTEX); sem_unlink(SEM_EMPTY); sem_unlink(SEM_FULL);
        
        kill(monitor_pid, SIGTERM); // Terminar al hijo (Monitor)
        wait(NULL);
    }else if(monitor_pid==0)
    {
        while(1)
        {
           sem_wait(sem_full);
            sem_wait(sem_mutex);
            MonitorMsg msg = shm_ptr->buffer[shm_ptr->out];
            shm_ptr->out = (shm_ptr->out + 1) % MAX_BUFFER;
            sem_post(sem_mutex);
            sem_post(sem_empty);

            if (msg.target == -1 && msg.solution == -1) {
                break; 
            }

            if (msg.is_valid) {
                printf("Solution accepted: %08d --> %08d\n", msg.target, msg.solution);
            } else {
                printf("Solution rejected: %08d !-> %08d\n", msg.target, msg.solution);
            }

            usleep(lag_monitor * 1000); // LAG_MONITOR
        }
    }
    exit(EXIT_SUCCESS);
}