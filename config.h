#ifndef CONFIG_H
#define CONFIG_H

#include "flight.h"

#define CONFIG_FILENAME "config.txt"
#define PIPE_NAME "named_pipe"
#define FLIGHT_LIST_SEMAPHORE "FLIGHT_LIST_SEMAPHORE_1"
#define QUEUE_SEMAPHORE "QUEUE_SEMAPHORE"
#define SCHEDULER_SEMAPHORE "SCHEDULER_SEMAPHORE"
#define TIMER_SEMAPHORE "TIMER_SEMAPHORE"
#define SHM_LANES_SEMAPHORE "SHM_LANES_SEMAPHORE"
#define LOGGER_SEMAPHORE "LOGGER_SEMAPHORE"
#define SHM_STATS_SEMAPHORE "SHM_STATS_SEMAPHORE"
#define LOG_FILENAME "log.txt"
#define BUFF_SIZE_INC 1024

#define DEBUG_CONFIG 0
#define DEBUG_RESOURCES 0
#define DEBUG_TIME_SEMAHORES 0
#define DEBUG_TIME 0
#define DEBUG_FLIGHT_THREADS 0
#define DEBUG_SCHEDULER 0
#define DEBUG_CONTROL_TOWER 0

typedef enum { INVALID_FLIGHT, PRIORITY_FLIGHT, NORMAL_FLIGHT } flight_priority_t;
typedef enum { NOP, REJECT, REDIRECT, EXECUTE, HOLD } action_type_t;

// CONFIG
typedef struct {
  int ut;
  int T, dt;
  int L, dl;
  int min, max;
  int D;
  int A;
} config_t;

typedef struct {
  long msg_type;
  flight_t flight;
} flight_msg_t;

typedef struct {
  action_type_t type;
  int holding_time;
  int lane_used;
  int fuel_left;
} action_t;

typedef struct {
  long msg_type;
  action_t action;
} tower_msg_t;

#endif
