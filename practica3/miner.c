/**
 * @brief It defines a simulation of blockchain and minering
 *
 * @file miner.c
 * @author Shaofan Xu, Javier Santamaria
 * @version 1.0
 * @date 24/04/2026
 */
#define _GNU_SOURCE

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
#include <errno.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "pow.h"
#include "shared_data.h"

#include <mqueue.h>

#define SEM_NAME_PID "/miner_pid"         // nombre del semaforo mutex para proteger escritura de pid
#define SEM_NAME_TARGET "/miner_target"   // nombre del semaforo mutex para proteger escritura de target
#define SEM_NAME_WINNER "/miner_winner"   // nombre del semaforo mutex para proteger winner
#define SEM_NAME_VOTE "/miner_vote"       // nombre del semaforo mutex para proteger el proceso de votacion
#define SEM_NAME_BARRIER "/miner_barrier" // nombre del semaforo mutex para proteger el proceso de votacion

#define TARGET_INIT 0 // Target default inicial

#define MQ_NAME "/mq_monitor" // Nombre de la cola del Monitor

/**
 * @brief RegistradorMsg
 * This structure store all information for process register
 */
typedef struct
{
    int round;    // El numero de ronda
    int target;   // El target
    int solution; // La solucion
    int coins;    // El numero de monedas
} RegistradorMsg; // Para el Pipe interno

/**
 * @brief Thread_args
 * This structure store all information of argument needed to executed the function
 */
typedef struct Thread_args
{
    long int start; // El inicio del intervalo
    long int end;   // Final del intervalo
} Thread_args;

sem_t *sem_pid = NULL;     // Semaforo para proteger pids.pid
sem_t *sem_votes = NULL;   // Semaforo para proteger vote.txt
sem_t *sem_winner = NULL;  // Semaforo para proteger winner
sem_t *sem_target = NULL;  // Semaforo para proteger target.tgt
sem_t *sem_barrier = NULL; // Semaforo para proteger el proceso de votacion

atomic_int globalSolution = 0;          // La solucion encontrada
atomic_int findSolution = 0;            // Flag: si ha encontrado la solucion o no
volatile sig_atomic_t time_to_exit = 0; // Flag: si ha alcanzado el tiempo limite o no
int globalTarget = 0;                   // El target
int n_threads = 0;                      // Numero de hilos
pthread_t *threads;                     // Array de hilos
Thread_args *args;                      // Array de argumentos de hilos

volatile sig_atomic_t start_mining = 0; // Flags: si empezar a minar o no
volatile sig_atomic_t start_voting = 0; // Flags: si empezar a votar o no

mqd_t mq_monitor; // Cola de mensajes
int fd_pipe[2];   // Pipe para el Registrador

SharedData *shm_ptr = NULL; // puntero de memoria compartida;
int shm_fd = -1;            // descriptor de shm de shared data

/**
 * @brief This function print all miner
 * @author Shaofan Xu
 *
 * @param f the file to print
 */
void print_all_miners(FILE *f)
{
    for (int i = 0; i < shm_ptr->num_procesos; i++)
    {
        pid_t temp_pid = shm_ptr->procesos_pid[i];
        fprintf(f, "  %jd\n", (intmax_t)temp_pid);
    }
}

/**
 * @brief Esta funcion borra al proceso mismo del fichero pids.pid, y si es ultimo elimina el fichero
 * @author Javier
 */
void miner_shutdown(void)
{
    if (start_voting)
    {
        sem_post(sem_barrier);
    }

    int remaining_miner = 0;
    pid_t myPid = getpid();

    while (sem_wait(sem_pid) == -1)
    {
        if (errno == EINTR)
            continue;
        break;
    }

    int found = 0;
    for (int i = 0; i < shm_ptr->num_procesos; i++)
    {
        if (shm_ptr->procesos_pid[i] == myPid)
        {
            found = 1;
        }
        if (found && i < shm_ptr->num_procesos - 1)
        {
            shm_ptr->procesos_pid[i] = shm_ptr->procesos_pid[i + 1];
            shm_ptr->carteras[i] = shm_ptr->carteras[i + 1];
        }
    }
    if (found)
    {
        shm_ptr->num_procesos--;
    }
    remaining_miner = shm_ptr->num_procesos;

    if (remaining_miner > 0)
    {
        fprintf(stdout, "Miner %d exited system. Remaining miners: %d\n", (int)myPid, remaining_miner);
        fprintf(stdout, "Current miners in system:\n");
        print_all_miners(stdout);
    }
    else
    {
        sem_unlink(SEM_NAME_PID);
        sem_unlink(SEM_NAME_TARGET);
        sem_unlink(SEM_NAME_WINNER);
        sem_unlink(SEM_NAME_VOTE);
        fprintf(stdout, "Miner %d was the last one. System cleaned.\n", (int)myPid);
    }
    if (remaining_miner == 0)
    {

        MonitorMsg end = {-1, -1, getpid(), -1, 0};
        mq_send(mq_monitor, (char *)&end, sizeof(end), 0);
    }

    mq_close(mq_monitor);

    RegistradorMsg bye = {0, 0, -1, 0}; // Ajustado para que el campo msg.solution sea -1
    write(fd_pipe[1], &bye, sizeof(bye));
    close(fd_pipe[1]);
    wait(NULL);

    sem_post(sem_pid);
    sem_close(sem_pid);
    sem_close(sem_target);
    sem_close(sem_winner);
    sem_close(sem_votes);
    sem_close(sem_barrier);

    munmap(shm_ptr, sizeof(SharedData));
    close(shm_fd);

    exit(EXIT_SUCCESS);
}

/**
 * @brief Handler functon of alarm sign
 * @author Javier Santa
 */
void alarm_handler(int sig) { time_to_exit = 1; }

/**
 * @brief Handler functon of sigusr1 to start the round
 * @author Shaofan Xu
 */
void sigusr1_handler(int sig) { start_mining = 1; }

/**
 * @brief Handler functon of sigusr2 to start voting for the winner
 * @author Shaofan Xu
 */
void sigusr2_handler(int sig) { start_voting = 1; }

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

    // El hilo se detiene si encuentra solucion, o si alguien manda a votar, o si es hora de salir
    for (long int i = t->start; i < t->end && !findSolution && !start_voting && !time_to_exit; i++)
    {

        if (pow_hash(i) == globalTarget)
        {

            globalSolution = i;
            findSolution = 1;
            return NULL;
        }
    }
    return NULL;
}
/**
 * @brief Esta funcion registra el resultado
 * @author Javier
 *
 * @param pipe_read el descriptor de pipe para leer datos
 * @return NULL
 */
void ejecutar_registrador(int pipe_read)
{
    RegistradorMsg msg;
    char log_name[32];
    sprintf(log_name, "%d.txt", getppid());

    while (1)
    {
        ssize_t bytes_leidos = 0;
        ssize_t total = sizeof(RegistradorMsg);

        while (bytes_leidos < total)
        {
            ssize_t n = read(pipe_read, ((char *)&msg) + bytes_leidos, total - bytes_leidos);

            if (n == 0)
            {
                break;
            }
            if (n == -1)
            {
                perror("read registrador");
                exit(EXIT_FAILURE);
            }
            bytes_leidos += n;
        }

        if (bytes_leidos < total)
        {
            break;
        }

        if (msg.solution == -1)
            break; // Señal de cierre

        FILE *logFile = fopen(log_name, "a");
        if (logFile)
        {
            fprintf(logFile, "Round: %d | Coins: %d | Solution: %d\n",
                    msg.round, msg.coins, msg.solution);
            fclose(logFile);
        }
    }
    exit(EXIT_SUCCESS);
}

/**
 * @brief La funcion main del programa
 */
int main(int argc, char *argv[])
{
    int n_seconds;
    int is_first_miner = 0;
    int total_miners = 0;
    int solution_escrita = 0;
    int is_winner = 0;
    int my_coins = 0;
    int round_id = 0;

    struct sigaction pid_act, sigusr1_act, sigusr2_act;
    sigset_t mask_block, mask_empty;

    sigemptyset(&mask_block);
    sigaddset(&mask_block, SIGUSR1);
    sigaddset(&mask_block, SIGUSR2);

    sigfillset(&mask_empty);
    sigdelset(&mask_empty, SIGUSR1);
    sigdelset(&mask_empty, SIGUSR2);
    sigdelset(&mask_empty, SIGALRM);

    if (pthread_sigmask(SIG_BLOCK, &mask_block, NULL) != 0)
    {
        perror("pthread_sigprocmask");
        exit(EXIT_FAILURE);
    }

    // Comprobacion de errores
    if (argc < 3)
    {
        fprintf(stderr, "Usage ./miner < N_SECS > < N_THREADS >\n");
        exit(EXIT_FAILURE);
    }
    n_seconds = atoi(argv[1]);
    n_threads = atoi(argv[2]);

    mq_monitor = mq_open(MQ_NAME, O_WRONLY);
    if (mq_monitor == (mqd_t)-1)
    {
        perror("Error el Monitor no está activo\n");
        exit(EXIT_FAILURE);
    }

    // abrir a memoria compartida
    shm_fd = shm_open("/shm_monitor", O_RDWR, 0);
    if (shm_fd == -1)
    {
        perror("Error,el Monitor no ha creado la memoria compartida.\n");
        exit(EXIT_FAILURE);
    }

    // Memoria compartida
    shm_ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED)
    {
        perror("Error, mapeo errorneo");
        exit(EXIT_FAILURE);
    }

    // Pipe para registrador
    pipe(fd_pipe);
    pid_t reg_pid = fork();

    if (reg_pid < 0)
    {
        perror("Error fork");
        exit(EXIT_FAILURE);
    }
    else if (reg_pid == 0)
    {
        close(fd_pipe[1]);
        ejecutar_registrador(fd_pipe[0]);
    }
    close(fd_pipe[0]);

    // sem_unlink(SEM_NAME_PID);
    // sem_unlink(SEM_NAME_TARGET);
    // sem_unlink(SEM_NAME_WINNER);
    // sem_unlink(SEM_NAME_VOTE);
    // sem_unlink(SEM_NAME_BARRIER);

    // crear semaforos
    if ((sem_pid = sem_open(SEM_NAME_PID, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_target = sem_open(SEM_NAME_TARGET, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_winner = sem_open(SEM_NAME_WINNER, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_votes = sem_open(SEM_NAME_VOTE, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED ||
        (sem_barrier = sem_open(SEM_NAME_BARRIER, O_CREAT, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    // set the sigal action
    pid_act.sa_handler = alarm_handler;
    sigemptyset(&pid_act.sa_mask);
    pid_act.sa_flags = 0;
    sigaction(SIGALRM, &pid_act, NULL);

    sigusr1_act.sa_handler = sigusr1_handler;
    sigemptyset(&sigusr1_act.sa_mask);
    sigusr1_act.sa_flags = 0;
    sigaction(SIGUSR1, &sigusr1_act, NULL);

    sigusr2_act.sa_handler = sigusr2_handler;
    sigemptyset(&sigusr2_act.sa_mask);
    sigusr2_act.sa_flags = 0;
    sigaction(SIGUSR2, &sigusr2_act, NULL);

    // Incorporar al sistema, intentando escribir su pid
    while (sem_wait(sem_pid) == -1 && errno == EINTR)
    {
        if (time_to_exit)
            miner_shutdown();
    }
    if (shm_ptr->num_procesos == 0)
        is_first_miner = 1;
    else
    {
        is_first_miner = 0;
    }

    if (shm_ptr->num_procesos < MAX_MINERS)
    {
        int index = shm_ptr->num_procesos;
        shm_ptr->procesos_pid[index] = getpid();
        shm_ptr->carteras[index] = 0;
        shm_ptr->num_procesos++;
        fprintf(stdout, "Miner %d added to system\n", getpid());
    }
    else
    {
        perror("Error: Número máximo de mineros alcanzado.\n");
        sem_post(sem_pid);

        munmap(shm_ptr, sizeof(SharedData));
        close(shm_fd);
        mq_close(mq_monitor);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < shm_ptr->num_procesos; i++)
    {
        pid_t temp_pid = shm_ptr->procesos_pid[i];
        if (temp_pid != getpid())
        {
            kill(temp_pid, SIGUSR1);
        }
    }
    sem_post(sem_pid);

    // Temporizacion
    alarm(n_seconds);

    // Si es primero establece el primer target
    if (is_first_miner)
    {
        while (sem_wait(sem_target) == -1 && errno == EINTR)
        {
            if (time_to_exit)
                miner_shutdown();
        }
        shm_ptr->target_objetivo = TARGET_INIT;
        sem_post(sem_target);

        // Esperando que incorpora mas minero
        while (total_miners < 2)
        {
            if (time_to_exit)
                miner_shutdown();
            while (sem_wait(sem_pid) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            total_miners = shm_ptr->num_procesos;
            sem_post(sem_pid);
            if (total_miners < 2)
                sigsuspend(&mask_empty);
        }

        // Lanzar señal para iniciar minering
        while (sem_wait(sem_pid) == -1 && errno == EINTR)
        {
            if (time_to_exit)
                miner_shutdown();
        }
        for (int i = 0; i < shm_ptr->num_procesos; i++)
        {
            pid_t temp_pid = shm_ptr->procesos_pid[i];
            if (temp_pid != getpid())
            {
                kill(temp_pid, SIGUSR1);
            }
        }
        sem_post(sem_pid);
    }

    // Minando
    while (1)
    {
        // Si llega el tiempo limite deberia terminar
        if (time_to_exit)
            miner_shutdown();

        // Minering
        if (start_mining == 1)
        {
            round_id++;
            start_mining = 0;
            findSolution = 0;
            start_voting = 0;

            // Intenta leer el target
            while (sem_wait(sem_target) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            globalTarget = shm_ptr->target_objetivo;
            sem_post(sem_target);

            pthread_sigmask(SIG_UNBLOCK, &mask_block, NULL);

            // Inicializa  hilos
            args = malloc(sizeof(Thread_args) * n_threads);
            threads = malloc(sizeof(pthread_t) * n_threads);

            for (int i = 0; i < n_threads; i++)
            {
                args[i].start = i * (POW_LIMIT / n_threads);
                args[i].end = (i + 1) * (POW_LIMIT / n_threads);
                pthread_create(&threads[i], NULL, miner, &args[i]);
            }

            // Espera a sus hilos
            for (int i = 0; i < n_threads; i++)
                pthread_join(threads[i], NULL);

            pthread_sigmask(SIG_BLOCK, &mask_block, NULL);

            free(args);
            free(threads);

            if (time_to_exit)
                miner_shutdown();

            // Intenta ser el winner desde el hilo principal
            if (findSolution == 1)
            {
                if (sem_trywait(sem_winner) == 0)
                {
                    is_winner = 1;

                    while (sem_wait(sem_target) == -1 && errno == EINTR)
                    {
                        if (time_to_exit)
                            miner_shutdown();
                    }
                    shm_ptr->solucion_actual = globalSolution;
                    sem_post(sem_target);

                    while (sem_wait(sem_pid) == -1 && errno == EINTR)
                    {
                        if (time_to_exit)
                            miner_shutdown();
                    }
                    for (int i = 0; i < shm_ptr->num_procesos; i++)
                    {
                        pid_t temp_pid = shm_ptr->procesos_pid[i];
                        if (temp_pid != getpid())
                        {
                            kill(temp_pid, SIGUSR2);
                        }
                    }
                    sem_post(sem_pid);

                    start_voting = 1;
                }
            }
        }

        if (is_winner == 1)
        {
            is_winner = 0;
            int saved_target = globalTarget;
            int expected_votes = -1; // Debe ser 0, pero quitando a si mismo da -1;

            start_voting = 0; // Reseteamos flag

            while (sem_wait(sem_target) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            shm_ptr->target_objetivo = globalSolution;
            sem_post(sem_target);

            while (sem_wait(sem_pid) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            expected_votes += shm_ptr->num_procesos;
            sem_post(sem_pid);

            if (expected_votes > 0)
            {
                for (int i = 0; i < expected_votes; i++)
                {
                    while (sem_wait(sem_barrier) == -1 && errno == EINTR)
                    {
                        if (time_to_exit)
                            miner_shutdown();
                    }
                }
            }

            // Contar votos
            int total_v = 0, y_v = 0, n_v = 0;

            while (sem_wait(sem_votes) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            y_v = shm_ptr->votos_y;
            n_v = shm_ptr->votos_n;
            total_v = y_v + n_v;
            sem_post(sem_votes);

            // Imprimir resultados
            fprintf(stdout, "Winner %d => [ ", getpid());
            for (int i = 0; i < y_v; i++)
                fprintf(stdout, "Y ");
            for (int i = 0; i < n_v; i++)
                fprintf(stdout, "N ");

            if (expected_votes <= 0)
            {

                sem_post(sem_winner);

                continue;
            }
            if (y_v > n_v)
            {

                fprintf(stdout, "] => Accepted\n");
                my_coins++;

                RegistradorMsg r_info = {round_id, saved_target, globalSolution, my_coins};
                write(fd_pipe[1], &r_info, sizeof(r_info));

                MonitorMsg m_info = {saved_target, globalSolution, getpid(), round_id, 0};
                mq_send(mq_monitor, (char *)&m_info, sizeof(m_info), 1);

                char log_name[32];
                sprintf(log_name, "%d.txt", getpid());
                FILE *logFile = fopen(log_name, "a");
                if (logFile)
                {
                    fprintf(logFile, "Id:      %d\n", round_id);
                    fprintf(logFile, "Winner:  %d\n", getpid());
                    fprintf(logFile, "Target:  %d\n", saved_target);
                    fprintf(logFile, "Solution: %d (validated)\n", globalSolution);
                    fprintf(logFile, "Votes:   %d/%d\n", y_v, total_v);
                    fprintf(logFile, "Wallets: %d:%d\n\n", getpid(), my_coins);
                    fclose(logFile);
                }

                while (sem_wait(sem_target) == -1 && errno == EINTR)
                {
                    if (time_to_exit)
                        miner_shutdown();
                }
                shm_ptr->target_objetivo = globalSolution;
                sem_post(sem_target);
            }
            else
            {
                printf("] => Rejected\n");
                while (sem_wait(sem_target) == -1 && errno == EINTR)
                {
                    if (time_to_exit)
                        miner_shutdown();
                }
                shm_ptr->target_objetivo = saved_target;
                sem_post(sem_target);
            }
            fflush(stdout);

            while (sem_wait(sem_votes) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            shm_ptr->votos_y = 0;
            shm_ptr->votos_n = 0;

            sem_post(sem_votes);

            sem_post(sem_winner);

            // Iniciar nueva ronda
            int current_miners = 0;

            while (sem_wait(sem_pid) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            current_miners = shm_ptr->num_procesos;
            sem_post(sem_pid);

            while (current_miners < 2)
            {
                if (time_to_exit)
                    miner_shutdown();

                current_miners = 0;
                while (sem_wait(sem_pid) == -1 && errno == EINTR)
                {
                    if (time_to_exit)
                        miner_shutdown();
                }
                current_miners = shm_ptr->num_procesos;
                sem_post(sem_pid);

                sigprocmask(SIG_BLOCK, &mask_block, NULL);
                if (current_miners < 2)
                {
                    sigsuspend(&mask_empty);
                }
                sigprocmask(SIG_UNBLOCK, &mask_block, NULL);
            }

            while (sem_wait(sem_pid) == -1 && errno == EINTR)
            {
                if (time_to_exit)
                    miner_shutdown();
            }
            for (int i = 0; i < shm_ptr->num_procesos; i++)
            {
                pid_t temp_pid = shm_ptr->procesos_pid[i];
                if (temp_pid != getpid())
                {
                    kill(temp_pid, SIGUSR1);
                }
            }
            sem_post(sem_pid);
            is_winner = 0;
        }
        else
        {
            // Proceso de votacion
            if (start_voting == 1)
            {
                start_voting = 0;

                // Intenta leer target
                while (sem_wait(sem_target) == -1 && errno == EINTR)
                {
                    if (time_to_exit)
                        miner_shutdown();
                }
                solution_escrita = shm_ptr->solucion_actual;
                sem_post(sem_target);

                while (sem_wait(sem_votes) == -1 && errno == EINTR)
                {
                    if (time_to_exit)
                        miner_shutdown();
                }

                if (pow_hash(solution_escrita) == globalTarget)
                    shm_ptr->votos_y++;
                else
                    shm_ptr->votos_n++;

                sem_post(sem_votes);
                sem_post(sem_barrier);
            }

            if (start_mining == 0 && start_voting == 0 && is_winner == 0 && !time_to_exit)
            {
                sigsuspend(&mask_empty);
            }
        }
    }
    return 0;
}
