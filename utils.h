#ifndef UTILS_H
#define UTILS_H

#include "config.h"
#include "flight.h"
#include "status.h"

#include <semaphore.h>

// CONFIG
config_t* read_config(status_t*);
void print_config(config_t);

// NAMED PIPE
int create_named_pipe(status_t*);
int open_named_pipe(int*, status_t*);

// MESSAGE QUEUE
int create_message_queue(int*, status_t, status_t*);

// SEMAPHORES
sem_t* semaphore_init(const char*, status_t, status_t*, int);

// LOGGER
typedef enum {
  PROGRAM_START, PROGRAM_END, NEW_COMMAND, WRONG_COMMAND, THREAD_START, THREAD_END,
  FLIGHT_HOLDING, FLIGHT_LANDING_START, FLIGHT_LANDING_END, FLIGHT_TAKEOFF_START,
  FLIGHT_TAKEOFF_END, EMERGENCY_LANDING, FLIGHT_REDIRECT
} log_type_t;

void logger(log_type_t, char*, sem_t*);

// STRING
char* string_init();
char* string_append(char*, char*);

#endif
