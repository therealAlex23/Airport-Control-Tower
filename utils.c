#include "config.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <pthread.h>
#include <string.h>
#include <regex.h>

// CONFIG
config_t* read_config(status_t* status_p) {
  config_t* config_p = (config_t*) malloc(sizeof(config_t));

  FILE* file_p = fopen(CONFIG_FILENAME, "r");

  int n_ut = fscanf(file_p, "%d\n", &(config_p->ut));
  int n_t = fscanf(file_p, "%d, %d\n", &(config_p->T), &(config_p->dt));
  int n_l = fscanf(file_p, "%d, %d\n", &(config_p->L), &(config_p->dl));
  int n_min_max = fscanf(file_p, "%d, %d\n", &(config_p->min), &(config_p->max));
  int n_d = fscanf(file_p, "%d\n", &(config_p->D));
  int n_a = fscanf(file_p, "%d\n", &(config_p->A));

  if (n_ut != 1 || n_t != 2 || n_l != 2 || n_min_max != 2 || n_d != 1 || n_a != 1) {
    if (DEBUG_RESOURCES) printf("Invalid config file\n");
    free(config_p);
    config_p = NULL;
  } else {
    *status_p = CONFIG_LOADED;
    if (DEBUG_CONFIG) print_config(*config_p);
  }

  fclose(file_p);

  return config_p;
}

void print_config(config_t config) {
  printf("--- CONFIG ---\n");
  printf("ut: %d\n", config.ut);
  printf("T: %d, dt: %d\n", config.T, config.dt);
  printf("L: %d, dl: %d\n", config.L, config.dl);
  printf("min: %d, max: %d\n", config.min, config.max);
  printf("D: %d, A: %d\n", config.D, config.A);
  printf("--- END CONFIG ---\n");
}

// NAMED PIPE
int create_named_pipe(status_t* status_p) {
  int success = !(mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0777) < 0 && errno != EEXIST);
  if (!success) {
    if (DEBUG_RESOURCES) perror("Failed to create named pipe");
  } else {
    *status_p = NAMEDPIPE_CREATED;
  }

  return success;
}

int open_named_pipe(int* named_pipe_id_p, status_t* status_p) {
  *named_pipe_id_p = open(PIPE_NAME, O_RDWR);
  
  if (*named_pipe_id_p == -1) {
    if (DEBUG_RESOURCES) perror("Failed to open named pipe");
    return 0;
  } else {
    *status_p = NAMEDPIPE_OPENED;
  }

  return 1;
}

// MESSAGE QUEUE
int create_message_queue(int* message_queue_id_p, status_t success_status, status_t* status_p) {
  *message_queue_id_p = msgget(IPC_PRIVATE, IPC_CREAT|0777);

  if (*message_queue_id_p == -1) {
    if (DEBUG_RESOURCES) perror("Failed to get message queue");
    return 0;
  } else {
    *status_p = success_status;
  }

  /* TODO: TOWER MESSAGE QUEUE */

  return 1;
}

// SEMAPHORES
sem_t* semaphore_init(const char* sem_name, status_t success_status, status_t* status, int value) {
  sem_t* new_sem_p = sem_open(sem_name, O_CREAT|O_EXCL, 0777, value);
  if (new_sem_p != SEM_FAILED) *status = success_status;
  return new_sem_p;
}

/* STRING */
char* string_init() {
  return (char*)malloc(sizeof(char) * BUFF_SIZE_INC);
}

char* string_append(char* src, char* string) {
  size_t length = strlen(src);
  int multiplier = (length / BUFF_SIZE_INC) + 1;
  size_t capacity = multiplier * BUFF_SIZE_INC;

  if (length + strlen(string) > capacity) {
    src = (char*)realloc(string, sizeof(char) * BUFF_SIZE_INC * (multiplier + 1));
  }

  return strcat(src, string);
}

void logger(log_type_t log_type, char* msg, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);

  char* log_msg = string_init();
  time_t timer;
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);
  strftime(log_msg, BUFF_SIZE_INC, "%H:%M:%S", tm_info);
  log_msg = string_append(log_msg, " ");

  switch(log_type) {
    case PROGRAM_START:
      log_msg = string_append(log_msg, "PROGRAM START");
      break;

    case PROGRAM_END:
      log_msg = string_append(log_msg, "PROGRAM END");
      break;

    case NEW_COMMAND:
      log_msg = string_append(log_msg, "NEW COMMAND => ");
      log_msg = string_append(log_msg, msg);
      break;

    case WRONG_COMMAND:
      log_msg = string_append(log_msg, "WRONG COMMAND => ");
      log_msg = string_append(log_msg, msg);
      break;

    case THREAD_START:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " FLIGHT DETECTED");
      break;

    case THREAD_END:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " FLIGHT HANDLING FINALIZED");
      break;

    case FLIGHT_HOLDING:
      log_msg = string_append(log_msg, "HOLDING ");
      log_msg = string_append(log_msg, msg);
      break;

    case FLIGHT_LANDING_START:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " STARTED LANDING");
      break;

    case FLIGHT_LANDING_END:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " FINISHED LANDING");
      break;

    case FLIGHT_TAKEOFF_START:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " STARTED TAKEOFF");
      break;

    case FLIGHT_TAKEOFF_END:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " FINISHED TAKEOFF");
      break;

    case EMERGENCY_LANDING:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " REQUESTED AN EMERGENCY LANDING");
      break;

    case FLIGHT_REDIRECT:
      log_msg = string_append(log_msg, msg);
      log_msg = string_append(log_msg, " LEAVING TO ANOTHER AIRPORT");
      break;
  }

  puts(log_msg);
  FILE* fp = fopen(LOG_FILENAME, "a");
  fprintf(fp, "%s\n", log_msg);
  free(log_msg);
  fclose(fp);

  if (sem_p != NULL) sem_post(sem_p);
}