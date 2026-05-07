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

#define MQ_MINER_COMPROBADOR "/mq_monitor" // Nombre de la cola
#define SHM_NAME "/shm_monitor"            // Nombre de memoria compartida
#define MAX_BUFFER 6                       // Numero maximo de buffer
#define MAX_MSG 7                          // Numero maximo de mensaje en la cola

/**
 * Atributos para la cola
 */
struct mq_attr attributes = {.mq_flags = 0,
                             .mq_maxmsg = MAX_MSG,
                             .mq_curmsgs = 0,
                             .mq_msgsize = sizeof(MonitorMsg)};

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
    /**
     * El descriptor de fichero
     */
    int shm_fd;
    /**
     * Puntero a memoria compartida
     */
    SharedData *shm_ptr;

    // Argument comprobation
    if (argc != 3)
    {
        fprintf(stderr, "Usage ./monitor < LAG_COMPROBADOR > < LAG_MONITOR >\n");
        exit(EXIT_FAILURE);
    }

    int lag_comprobador = atoi(argv[1]); // Lag para comprobador
    int lag_monitor = atoi(argv[2]);     // Lag para monitor

    // Limpieza de recursos previos por seguridad
    // mq_unlink(MQ_MINER_COMPROBADOR);
    // shm_unlink(SHM_NAME);
    // sem_unlink(SEM_MUTEX);
    // sem_unlink(SEM_EMPTY);
    // sem_unlink(SEM_FULL);

    // Inicializar cola, memoria compartida y semáforos
    queue = mq_open(MQ_MINER_COMPROBADOR, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
    if (queue == (mqd_t)-1)
    {
        perror("Error opening the queue");
        exit(EXIT_FAILURE);
    }

    // Crear la memoria compartida
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1)
    {
        perror("Error en shm_open");
        mq_close(queue);
        mq_unlink(MQ_MINER_COMPROBADOR);
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1)
    {
        perror("Error en ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        mq_close(queue);
        mq_unlink(MQ_MINER_COMPROBADOR);
        exit(EXIT_FAILURE);
    }
    shm_ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        perror("Error en mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        mq_close(queue);
        mq_unlink(MQ_MINER_COMPROBADOR);
        exit(EXIT_FAILURE);
    }
    close(shm_fd);
    shm_ptr->in = 0;
    shm_ptr->out = 0;
    shm_ptr->num_procesos = 0;
    shm_ptr->votos_y = 0;
    shm_ptr->votos_n = 0;
    shm_ptr->target_objetivo = 0;   

    sem_init(&shm_ptr->mutex, 1, 1);
    sem_init(&shm_ptr->empty, 1, MAX_BUFFER);
    sem_init(&shm_ptr->full, 1, 0);

    // Crear proceso monitor
    monitor_pid = fork();
    if (monitor_pid < 0)
    {
        perror("Error fork");
        sem_destroy(&shm_ptr->mutex);
        sem_destroy(&shm_ptr->empty);
        sem_destroy(&shm_ptr->full);

        munmap(shm_ptr, sizeof(SharedData));
        
        shm_unlink(SHM_NAME);
        mq_close(queue);
        mq_unlink(MQ_MINER_COMPROBADOR);
        exit(EXIT_FAILURE);
    }
    if (monitor_pid > 0)
    {
        // PROCESO PADRE: Comprobador
        MonitorMsg msg;
        fprintf(stdout,"[Comprobador] Iniciado correctamente. Esperando bloques...\n");

        while (1)
        {
            if (mq_receive(queue, (char *)&msg, sizeof(MonitorMsg), NULL) != -1)
            {
                // Si recibimos el código -1, terminamos la ejecución
                if (msg.target == -1 && msg.solution == -1)
                {
                    msg.is_valid = -1;
                    sem_wait(&shm_ptr->empty);
                    sem_wait(&shm_ptr->mutex);
                    shm_ptr->buffer[shm_ptr->in] = msg;
                    shm_ptr->in = (shm_ptr->in + 1) % MAX_BUFFER;
                    sem_post(&shm_ptr->mutex);
                    sem_post(&shm_ptr->full);
                    break;
                }

                // Validar el bloque comprobando el hash (Apartado a)
                if (pow_hash(msg.solution) == msg.target)
                {
                    msg.is_valid = 1;
                }
                else
                {
                    msg.is_valid = 0;
                    fprintf(stdout,"[Comprobador] ALERTA: Bloque inválido recibido (Ronda: %d)\n", msg.round);
                }

                sem_wait(&shm_ptr->empty);
                sem_wait(&shm_ptr->mutex);
                shm_ptr->buffer[shm_ptr->in] = msg;
                shm_ptr->in = (shm_ptr->in + 1) % MAX_BUFFER;
                sem_post(&shm_ptr->mutex);
                sem_post(&shm_ptr->full);
            }
            usleep(lag_comprobador * 1000); // LAG_COMPROBADOR
        }

        // Destrucción de recursos controlada por el padre
        wait(NULL);
        mq_close(queue);
        mq_unlink(MQ_MINER_COMPROBADOR);

        sem_destroy(&shm_ptr->mutex);
        sem_destroy(&shm_ptr->empty);
        sem_destroy(&shm_ptr->full);

        munmap(shm_ptr, sizeof(SharedData));
        shm_unlink(SHM_NAME);
    }
    else if (monitor_pid == 0)
    {
        // Proceso Monitor
        while (1)
        {
            sem_wait(&shm_ptr->full);
            sem_wait(&shm_ptr->mutex);
            MonitorMsg msg = shm_ptr->buffer[shm_ptr->out];
            shm_ptr->out = (shm_ptr->out + 1) % MAX_BUFFER;
            sem_post(&shm_ptr->mutex);
            sem_post(&shm_ptr->empty);

            if (msg.target == -1 && msg.solution == -1)
            {
                break;
            }
            // Imprime los resultados
            if (msg.is_valid)
            {
                fprintf(stdout,"Solution accepted: %08ld --> %08ld\n", msg.target, msg.solution);
            }
            else
            {
                fprintf(stdout,"Solution rejected: %08ld !-> %08ld\n", msg.target, msg.solution);
            }

            usleep(lag_monitor * 1000); // LAG_MONITOR
        }
    }
    exit(EXIT_SUCCESS);
}