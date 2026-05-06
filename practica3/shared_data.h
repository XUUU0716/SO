/**
 * @file shared_data.h
 * @author Shaofan Xu
 * @brief Estructura para memoria compartida
 * @version 2.0
 * @date 2024-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _SHARED_DATA_H
#define _SHARED_DATA_H

#include <semaphore.h>

#define MAX_MINERS 100 // Minero maximo

/**
 * @brief MonitorMsg
 * Estructura para mensaje a monitor
 */
typedef struct
{
    int target;       // EL objetivo
    int solution;     // La solucion encontrada
    pid_t winner_pid; // el pid de ganador
    int round;        // La ronda
    int is_valid;     // si es valido o no
} MonitorMsg;

/**
 * @brief SharedData
 * Estructura para memeoria compartida
 */
typedef struct
{
    // Campos para monitor
    MonitorMsg buffer[6]; // BUffer circular
    int in;               // El indice de entrada
    int out;              // el indice de salida

    // Para minero
    int target_objetivo; // el target
    int solucion_actual; // la solucion actual

    pid_t procesos_pid[MAX_MINERS]; // array de pids
    int num_procesos;               // numero de proceso

    int carteras[MAX_MINERS]; // cartera de cada proceso

    int votos_y; // votos yes
    int votos_n; // votos no

    sem_t mutex; // Mutex
    sem_t empty; // sem empty
    sem_t full;  // sem full

    int validacion_correcta; // validacion
} SharedData;

#endif