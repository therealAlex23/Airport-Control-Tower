#include "main.h"
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <string.h>

/* SIMULATION MANAGER */
pid_t pid;
int end_timer = 0;
int current_time = 0;
status_t status = INITIAL;
int named_pipe_id;
config_t* config_p;

sem_t* timer_sem_p = NULL;
sem_t* logger_sem_p = NULL;

flight_list_t* flight_list_p = NULL;
sem_t* flight_list_sem_p = NULL;

int shm_lanes_id;
shm_lanes_t* shm_lanes_p = NULL;
sem_t* shm_lanes_sem_p = NULL;

int shm_stats_id;
shm_stats_t* shm_stats_p = NULL;
sem_t* shm_stats_sem_p = NULL;

/* PIPEREADER */
pthread_t pipe_reader_thread;
int end_pipe_reader = 0;

/* CONTROL TOWER */
int process_killed = 0;
int tower_message_queue_id;
queue_t* departures_queue_p = NULL;
queue_t* arrivals_queue_p = NULL;
sem_t* queue_sem_p = NULL;

/* FLIGHT THREAD */
int flight_message_queue_id;

/* SCHEDULER */
int end_scheduler = 0;
sem_t* scheduler_sem_p = NULL;

int main() {
  logger(PROGRAM_START, NULL, logger_sem_p);
  init_globals();
  
  signal(SIGINT, shutdown);
  signal(SIGUSR1, print_stats);
  signal(SIGHUP, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGKILL, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);

  if ((config_p = read_config(&status)) == NULL) shutdown();  
  if (!create_named_pipe(&status)) shutdown();
  if (!open_named_pipe(&named_pipe_id, &status)) shutdown();
  if (!create_message_queue(&flight_message_queue_id, FLIGHT_MESSAGE_QUEUE_CREATED, &status)) shutdown();
  if (!create_message_queue(&tower_message_queue_id, TOWER_MESSAGE_QUEUE_CREATED, &status)) shutdown();
  if (!get_shm(&shm_lanes_id, SHM_LANES, SHM_LANES_CREATED, &status)) shutdown();
  if (!get_shm(&shm_stats_id, SHM_STATS, SHM_STATS_CREATED, &status)) shutdown();
  if (!attach_shm((void**)&shm_lanes_p, shm_lanes_id, SHM_LANES_ATTACHED, &status)) shutdown();
  if (!attach_shm((void**)&shm_stats_p, shm_stats_id, SHM_STATS_ATTACHED, &status)) shutdown();
  if (!create_semaphores()) shutdown();
  if (!create_pipe_reader_thread()) shutdown();

  shm_lanes_init(shm_lanes_p);
  shm_stats_init(shm_stats_p);

  pid = fork();

  if (pid == -1) {
    if (DEBUG_RESOURCES) perror("Failed to create Control Tower process");
    shutdown();
  } else if (pid == 0) {
    signal(SIGINT, shutdown_process);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGKILL, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    control_tower();
  } else {
    status = PROCESS_CREATED;
  }

  simulation_manager();

  return 0;
}

int possible_holding(int fuel_left) {
  if (fuel_left == 0) return 1;
  if (fuel_left < 0) return -1;

  for (int i = config_p->min; i <= config_p->max; ++i) {
    int res = possible_holding(fuel_left - i);
    if (res != -1) return i;
  }

  return -1;
}

void print_stats() {
  sem_wait(shm_stats_sem_p);
  printf("--- STATS ---\n");
  printf("CREATED FLIGHTS: %d\n", shm_stats_p->arrival_flights + shm_stats_p->departure_flights + shm_stats_p->rejected_flights);
  printf("ARRIVAL FLIGHTS: %d\n", shm_stats_p->arrival_flights);
  float avg_arrival_holding_requests = shm_stats_p->arrival_flights == 0 ? 0 : shm_stats_p->arrival_holdings / (float)shm_stats_p->arrival_flights;
  printf("ARRIVAL FLIGHTS AVERAGE HOLDING REQUESTS: %f\n", avg_arrival_holding_requests);
  float avg_arrival_holding_time = shm_stats_p->arrival_flights == 0 ? 0 : shm_stats_p->arrival_total_waiting_time / (float)shm_stats_p->arrival_flights;
  printf("ARRIVAL FLIGHTS AVERAGE HOLDING TIME: %f\n", avg_arrival_holding_time);
  printf("ARRIVAL FLIGHTS REDIRECT REQUESTS: %d\n", shm_stats_p->arrival_redirects);
  printf("DEPARTURE FLIGHTS: %d\n", shm_stats_p->departure_flights);
  float avg_departure_waiting_time = shm_stats_p->departure_flights == 0 ? 0 : shm_stats_p->departure_total_waiting_time / (float)shm_stats_p->departure_flights;
  printf("DEPARTURE FLIGHTS AVERAGE WAITING TIME: %f\n", avg_departure_waiting_time);
  printf("REJECTED FLIGHTS: %d\n", shm_stats_p->rejected_flights);
  printf("--- END STATS ---\n");
  sem_post(shm_stats_sem_p);
}

/* CREATORS */
void init_globals() {
  flight_list_p = flight_list_init();
  departures_queue_p = queue_init();
  arrivals_queue_p = queue_init();
}

int create_pipe_reader_thread() {
  int success = pthread_create(&pipe_reader_thread, NULL, pipe_reader, NULL) == 0;
  if (success) status = PIPE_READER_CREATED;
  return success;
}

int create_semaphores() {
  flight_list_sem_p = semaphore_init(FLIGHT_LIST_SEMAPHORE, FLIGHT_LIST_SEM_CREATED, &status, 1);

  if (flight_list_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create flight list semaphore");
    return 0;
  }

  queue_sem_p = semaphore_init(QUEUE_SEMAPHORE, QUEUE_SEM_CREATED, &status, 1);

  if (queue_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create queue semaphore");
    return 0;
  }

  scheduler_sem_p = semaphore_init(SCHEDULER_SEMAPHORE, SCHEDULER_SEM_CREATED, &status, 0);

  if (scheduler_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create scheduler semaphore");
    return 0;
  }

  timer_sem_p = semaphore_init(TIMER_SEMAPHORE, TIMER_SEM_CREATED, &status, 0);

  if (timer_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create timer semaphore");
    return 0;
  }

  shm_lanes_sem_p = semaphore_init(SHM_LANES_SEMAPHORE, SHM_LANES_SEM_CREATED, &status, 1);

  if (shm_lanes_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create lanes semaphore");
    return 0;
  }

  logger_sem_p = semaphore_init(LOGGER_SEMAPHORE, LOGGER_SEM_CREATED, &status, 1);

  if (logger_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create logger semaphore");
    return 0;
  }

  shm_stats_sem_p = semaphore_init(SHM_STATS_SEMAPHORE, SHM_STATS_SEM_CREATED, &status, 1);

  if (shm_stats_sem_p == SEM_FAILED) {
    if (DEBUG_RESOURCES) perror("Failed to create stats semaphore");
    return 0;
  }

  return 1;
}

/* DESTROYERS */
void shutdown() {
  switch (status) {
    case PROCESS_CREATED:
      end_timer = 1;
      if (DEBUG_RESOURCES) printf("Waiting for Control Tower to finish\n");
      msgctl(flight_message_queue_id, IPC_RMID, NULL);
      wait(NULL);
      process_killed = 1;

    case PIPE_READER_CREATED:
      end_pipe_reader = 1;
      write(named_pipe_id, "", 1);
      pthread_join(pipe_reader_thread, NULL);

    case SHM_STATS_SEM_CREATED:
      sem_post(shm_stats_sem_p);
      sem_destroy(shm_stats_sem_p);
      sem_unlink(SHM_STATS_SEMAPHORE);

    case LOGGER_SEM_CREATED:
      sem_post(logger_sem_p);
      sem_destroy(logger_sem_p);
      sem_unlink(LOGGER_SEMAPHORE);

    case SHM_LANES_SEM_CREATED:
      sem_post(shm_lanes_sem_p);
      sem_destroy(shm_lanes_sem_p);
      sem_unlink(SHM_LANES_SEMAPHORE);

    case TIMER_SEM_CREATED:
      sem_post(timer_sem_p);
      sem_destroy(timer_sem_p);
      sem_unlink(TIMER_SEMAPHORE);

    case QUEUE_SEM_CREATED:
      sem_post(queue_sem_p);
      sem_destroy(queue_sem_p);
      sem_unlink(QUEUE_SEMAPHORE);

    case SCHEDULER_SEM_CREATED:
      sem_post(scheduler_sem_p);
      sem_destroy(scheduler_sem_p);
      sem_unlink(SCHEDULER_SEMAPHORE);

    case FLIGHT_LIST_SEM_CREATED:
      sem_post(flight_list_sem_p);
      sem_destroy(flight_list_sem_p);
      sem_unlink(FLIGHT_LIST_SEMAPHORE);

    case SHM_STATS_ATTACHED:
      shmdt(shm_stats_p);

    case SHM_LANES_ATTACHED:
      shmdt(shm_lanes_p);

    case SHM_STATS_CREATED:
      shmctl(shm_lanes_id, IPC_RMID, NULL);

    case SHM_LANES_CREATED:
      shmctl(shm_stats_id, IPC_RMID, NULL);

    case TOWER_MESSAGE_QUEUE_CREATED:
      msgctl(tower_message_queue_id, IPC_RMID, NULL);

    case FLIGHT_MESSAGE_QUEUE_CREATED:
      if (!process_killed) msgctl(flight_message_queue_id, IPC_RMID, NULL);

    case NAMEDPIPE_OPENED:
      close(named_pipe_id);

    case NAMEDPIPE_CREATED:
      unlink(PIPE_NAME);

    case CONFIG_LOADED:
      if (config_p != NULL) free(config_p);

    case INITIAL:
      flight_list_free(flight_list_p, NULL);
      queue_free(departures_queue_p, NULL);
      queue_free(arrivals_queue_p, NULL);
  }

  if (DEBUG_RESOURCES) printf("Closing Simulation Manager\n");

  logger(PROGRAM_END, NULL, logger_sem_p);

  if (status != INITIAL && status != PROCESS_CREATED) exit(1);
  else exit(0);
}

void shutdown_process() {
  if (DEBUG_RESOURCES) printf("Control Tower closed\n");
  end_scheduler = 1;
  sem_post(scheduler_sem_p);
}

// PROCESSES
void control_tower() {
  if (DEBUG_RESOURCES) printf("Control Tower is live\n");

  pthread_t thread;
  pthread_create(&thread, NULL, scheduler, NULL);

  flight_msg_t flight_msg;
  while (msgrcv(flight_message_queue_id, &flight_msg, sizeof(flight_t), -NORMAL_FLIGHT, 0) != -1) {
    flight_t flight = flight_msg.flight;

    if (DEBUG_CONTROL_TOWER) printf("CONTROL TOWER - DETECTED FLIGHT %d\n", flight.id);

    // Trying to accept departure
    if (flight.type == DEPARTURE && queue_size(departures_queue_p, queue_sem_p) < config_p->D) {
      sem_wait(shm_stats_sem_p);
      shm_stats_p->departure_flights += 1;
      sem_post(shm_stats_sem_p);

      int t_start =  flight.init + flight.takeoff;
      int t_end = t_start + config_p->T + config_p->dt;
      queue_add_departure(departures_queue_p, flight.id, t_start, t_end, queue_sem_p);
      if (DEBUG_CONTROL_TOWER) printf("CONTROL TOWER - ADDING DEPARTURE FLIGHT %d to QUEUE - T_START: %d T_END: %d\n", flight.id, t_start, t_end);
      if (DEBUG_CONTROL_TOWER) printf("QDS: %d / %d\n", queue_size(departures_queue_p, queue_sem_p), config_p->D);
    }
    // Trying to accept arrival
    else if (queue_size(arrivals_queue_p, queue_sem_p) < config_p->A) {
      sem_wait(shm_stats_sem_p);
      shm_stats_p->arrival_flights += 1;
      sem_post(shm_stats_sem_p);

      int t_start =  flight.init + flight.eta;
      int t_end = t_start + config_p->L + config_p->dl;
      int t_fuel = flight.init + flight.fuel;
      queue_add_arrival(arrivals_queue_p, flight.id, t_start, t_end, t_fuel, queue_sem_p);
      if (DEBUG_CONTROL_TOWER) printf("CONTROL TOWER - ADDING ARRIVAL FLIGHT %d to QUEUE - T_START: %d T_END: %d T_FUEL: %d\n", flight.id, t_start, t_end, t_fuel);
      if (DEBUG_CONTROL_TOWER) printf("QAS: %d / %d\n", queue_size(arrivals_queue_p, queue_sem_p), config_p->A);
    }
    // Rejecting flight
    else {
      sem_wait(shm_stats_sem_p);
      shm_stats_p->rejected_flights += 1;
      sem_post(shm_stats_sem_p);

      if (DEBUG_CONTROL_TOWER) printf("CONTROL TOWER - REJECTING FLIGHT %d\n", flight.id);
      tower_msg_t message;
      message.msg_type = flight.id;
      message.action.type = REJECT;
      msgsnd(tower_message_queue_id, &message, sizeof(action_t), 0);
    }
  }

  exit(0);
}

void simulation_manager() {
  if (DEBUG_RESOURCES) printf("Simulation Manager is live\n");

  while (1) {
    if (DEBUG_TIME_SEMAHORES) printf("LOCKING TIMER\n");
    sem_wait(timer_sem_p);
    if (end_timer) break;

    if (DEBUG_TIME) printf("T: %d - TIMER\n", current_time);

    flight_t flight = flight_list_pop_by_init(flight_list_p, current_time, flight_list_sem_p);

    while (flight.type != NOT_FOUND) {
      long msg_type;

      if (flight.type == ARRIVAL) {
        if (flight.fuel == flight.eta + config_p->L) {
          if (DEBUG_TIME) printf("T: %d - TIMER - PRIORITY ARRIVAL FLIGHT %d\n", current_time, flight.id);
          char* flight_name = string_init();
          sprintf(flight_name, "TP%d", flight.number);
          logger(EMERGENCY_LANDING, flight_name, logger_sem_p);
          free(flight_name);
          msg_type = PRIORITY_FLIGHT;
        }
        else if (flight.fuel > flight.eta + config_p->L) {
          if (DEBUG_TIME) printf("T: %d - TIMER - NORMAL ARRIVAL FLIGHT %d\n", current_time, flight.id);
          msg_type = NORMAL_FLIGHT;
        }
        else {
          if (DEBUG_TIME) printf("T: %d - TIMER - INVALID ARRIVAL FLIGHT %d - FUEL: %d FUEL_REQUIRED: %d\n", current_time, flight.id, flight.fuel, flight.eta + config_p->L);
          flight = flight_list_pop_by_init(flight_list_p, current_time, flight_list_sem_p);
          continue;
        }
      } else {
        if (DEBUG_TIME) printf("T: %d - TIMER - NORMAL DEPARTURE FLIGHT %d\n", current_time, flight.id);
        msg_type = NORMAL_FLIGHT;
      }

      pthread_t thread;
      flight_msg_t* flight_msg_p = (flight_msg_t*)malloc(sizeof(flight_msg_t));
      flight_msg_p->msg_type = msg_type;
      flight_msg_p->flight = flight;

      if (flight.type == ARRIVAL) {
        pthread_create(&thread, NULL, arrival_flight, (void*)flight_msg_p);
      }

      if (flight.type == DEPARTURE) {
        pthread_create(&thread, NULL, departure_flight, (void*)flight_msg_p);
      }

      flight = flight_list_pop_by_init(flight_list_p, current_time, flight_list_sem_p);
    }

    usleep(config_p->ut * 1000);
    ++current_time;
    if (DEBUG_TIME_SEMAHORES) printf("UNLOCKING SCHEDULER\n");
    sem_post(scheduler_sem_p);
  }
}

// THREADS
void* pipe_reader() {
  int ret = 0;
  if (DEBUG_RESOURCES) printf("Pipe reader is live\n");

  char pipe_buffer[BUFF_SIZE_INC] = "";

  regex_t regex_departure;
  regcomp(&regex_departure, "DEPARTURE TP([0-9]+) init: ([0-9]+) takeoff: ([0-9]+)$", REG_EXTENDED);
  regex_t regex_arrival;
  regcomp(&regex_arrival, "ARRIVAL TP([0-9]+) init: ([0-9]+) eta: ([0-9]+) fuel: ([0-9]+)$", REG_EXTENDED);

  int current_id = 1;

  while (1) {
    read(named_pipe_id, pipe_buffer, BUFF_SIZE_INC);

    if (end_pipe_reader) break;

    if (strcmp(pipe_buffer, "")) {
      char* token = strtok(pipe_buffer, "\n");

      while (token != NULL) {
        flight_t flight;
        flight.id = current_id++;

        if (!regexec(&regex_departure, token, 0, NULL, 0)) {
          flight.type = DEPARTURE;
          sscanf(token, "DEPARTURE TP%d init: %d takeoff: %d", &(flight.number), &(flight.init), &(flight.takeoff));
          if (flight.init <= current_time) {
            logger(WRONG_COMMAND, token, logger_sem_p);
          }
          else {
            logger(NEW_COMMAND, token, logger_sem_p);
            flight_list_add(flight_list_p, flight, flight_list_sem_p);
          }
        }
        else if (!regexec(&regex_arrival, token, 0, NULL, 0)) {
          flight.type = ARRIVAL;
          sscanf(token, "ARRIVAL TP%d init: %d eta: %d fuel: %d", &(flight.number), &(flight.init), &(flight.eta), &(flight.fuel));
          if (flight.init <= current_time) {
            logger(WRONG_COMMAND, token, logger_sem_p);
          }
          else {
            logger(NEW_COMMAND, token, logger_sem_p);
            flight_list_add(flight_list_p, flight, flight_list_sem_p);
          }
        }
        else {
          logger(WRONG_COMMAND, token, logger_sem_p);
        }

        token = strtok(NULL, "\n");
      }
    }

    memset(pipe_buffer, 0, BUFF_SIZE_INC);
  }

  if (DEBUG_RESOURCES) printf("Pipe reader closed\n");

  pthread_exit(&ret);
}

void* arrival_flight(void* flight_msg_p) {
  int ret = 0;
  flight_t flight = ((flight_msg_t*)flight_msg_p)->flight;
  
  char* flight_name = string_init();
  sprintf(flight_name, "TP%d ARRIVAL", flight.number);
  logger(THREAD_START, flight_name, logger_sem_p);

  msgsnd(flight_message_queue_id, flight_msg_p, sizeof(flight_t), 0);
  free(flight_msg_p);

  tower_msg_t tower_msg;
  char loop = 1;
  while (loop && msgrcv(tower_message_queue_id, &tower_msg, sizeof(action_t), flight.id, 0) != -1) {
    switch (tower_msg.action.type) {
      case REJECT:
        if (DEBUG_FLIGHT_THREADS) printf("ARRIVAL FLIGHT %d - REJECTED\n", flight.id);
        loop = 0;
        break;

      case EXECUTE:
        if (DEBUG_FLIGHT_THREADS) printf("ARRIVAL FLIGHT %d - LANDING\n", flight.id);
        
        char* landing_data = string_init();
        sprintf(landing_data, "TP%d ON LANE %d", flight.number, tower_msg.action.lane_used);
        logger(FLIGHT_LANDING_START, landing_data, logger_sem_p);
        
        usleep(config_p->L * config_p->ut * 1000);
        
        logger(FLIGHT_LANDING_END, landing_data, logger_sem_p);
        free(landing_data);

        loop = 0;
        break;

      case REDIRECT:
        if (DEBUG_FLIGHT_THREADS) printf("ARRIVAL FLIGHT %d - REDIRECT\n", flight.id);
        
        char* redirect_data = string_init();
        sprintf(redirect_data, "TP%d WITH %d FUEL LEFT", flight.number, tower_msg.action.fuel_left);
        logger(FLIGHT_REDIRECT, redirect_data, logger_sem_p);
        free(redirect_data);

        loop = 0;
        break;

      case HOLD:
        if (DEBUG_FLIGHT_THREADS) printf("ARRIVAL FLIGHT %d - HOLDING FOR %d\n", flight.id, tower_msg.action.holding_time);
        
        char* holding_data = string_init();
        sprintf(holding_data, "TP%d FOR %d WITH %d FUEL LEFT", flight.number, tower_msg.action.holding_time, tower_msg.action.fuel_left);
        logger(FLIGHT_HOLDING, holding_data, logger_sem_p);
        free(holding_data);
        break;

      case NOP:
        if (DEBUG_FLIGHT_THREADS) printf("ARRIVAL FLIGHT %d - IGNORED MESSAGE\n", flight.id);
        break;
    }
  }

  logger(THREAD_END, flight_name, logger_sem_p);
  free(flight_name);

  pthread_exit(&ret);
}

void* departure_flight(void* flight_msg_p) {
  int ret = 0;
  flight_t flight = ((flight_msg_t*)flight_msg_p)->flight;
  
  if (DEBUG_FLIGHT_THREADS) printf("T: %d - DEPARTURE FLIGHT %d - INIT\n", flight.init, flight.id);

  char* flight_name = string_init();
  sprintf(flight_name, "TP%d DEPARTURE", flight.number);
  logger(THREAD_START, flight_name, logger_sem_p);

  msgsnd(flight_message_queue_id, flight_msg_p, sizeof(flight_t), 0);

  tower_msg_t tower_msg;
  char loop = 1;  
  while (loop && msgrcv(tower_message_queue_id, &tower_msg, sizeof(action_t), flight.id, 0) != -1) {
    switch (tower_msg.action.type) {
      case REJECT:
        if (DEBUG_FLIGHT_THREADS) printf("DEPARTURE FLIGHT %d - REJECTED\n", flight.id);
        loop = 0;
        break;

      case EXECUTE:
        if (DEBUG_FLIGHT_THREADS) printf("DEPARTURE FLIGHT %d - TAKING OFF\n", flight.id);

        char* takeoff_data = string_init();
        sprintf(takeoff_data, "TP%d ON LANE %d", flight.number, tower_msg.action.lane_used);
        logger(FLIGHT_TAKEOFF_START, takeoff_data, logger_sem_p);

        usleep(config_p->T * config_p->ut * 1000);

        logger(FLIGHT_TAKEOFF_END, takeoff_data, logger_sem_p);
        free(takeoff_data);

        loop = 0;
        break;

      default:
        if (DEBUG_FLIGHT_THREADS) printf("DEPARTURE FLIGHT %d - IGNORED MESSAGE\n", flight.id);
    }
  }

  logger(THREAD_END, flight_name, logger_sem_p);
  free(flight_name);

  pthread_exit(&ret);
}

void* scheduler() {
  int ret = 0;
  int current_time = 0;

  while (1) {
    if (DEBUG_TIME_SEMAHORES) printf("UNLOCKING TIMER\n");
    sem_post(timer_sem_p); // START TIMER
    if (DEBUG_TIME_SEMAHORES) printf("LOCKING SCHEDULER\n");
    sem_wait(scheduler_sem_p); // PAUSE SCHEDULER -> TIMER RESUMES SCHEDULER
    if (end_scheduler) break;

    if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER\n", current_time);

    sem_wait(queue_sem_p);
    sem_wait(shm_lanes_sem_p);

    int new_lane1 = 0;
    int new_lane2 = 0;
    
    int scheduling_complete = 0;
    while (!scheduling_complete) {
      queue_node_t* departures_queue_node_p = departures_queue_p->head;
      queue_node_t* arrivals_queue_node_p = arrivals_queue_p->head;

      if (DEBUG_SCHEDULER) queue_print(departures_queue_p, "DEPARTURES", NULL);
      if (DEBUG_SCHEDULER) queue_print(arrivals_queue_p, "ARRIVALS", NULL);
      if (DEBUG_SCHEDULER) printf("T: %d T_END_LANE_1: %d T_END_LANE_2: %d\n", current_time, shm_lanes_p->t_end_1, shm_lanes_p->t_end_2);

      char ACCEPT_ARR = 0;

      if (arrivals_queue_node_p != NULL) {
        char ARR_COND1 = arrivals_queue_node_p->t_start == current_time;
        if (DEBUG_SCHEDULER) printf("ARR_COND1: %d\n", ARR_COND1);
        char ARR_COND2_1 = shm_lanes_p->type == ARRIVAL && (shm_lanes_p->t_end_1 < current_time || shm_lanes_p->t_end_2 < current_time);
        if (DEBUG_SCHEDULER) printf("ARR_COND2_1: %d\n", ARR_COND2_1);
        char ARR_COND2_2 = shm_lanes_p->t_end_1 < current_time && shm_lanes_p->t_end_2 < current_time;
        if (DEBUG_SCHEDULER) printf("ARR_COND2_2: %d\n", ARR_COND2_2);
        char ARR_COND2 = ARR_COND2_1 || ARR_COND2_2;
        if (DEBUG_SCHEDULER) printf("ARR_COND2: %d\n", ARR_COND2);
        ACCEPT_ARR = ARR_COND1 && ARR_COND2;
      }

      if (DEBUG_SCHEDULER) printf("ACCEPT_ARR: %d\n", ACCEPT_ARR);
      if (ACCEPT_ARR) {
        queue_pop(arrivals_queue_p, NULL);
        int flight_id = arrivals_queue_node_p->flight_id;

        if (shm_lanes_p->t_end_1 < current_time) {
          shm_lanes_p->type = ARRIVAL;
          shm_lanes_p->t_end_1 = arrivals_queue_node_p->t_end;
          new_lane1 = flight_id;
          if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - FILLING LANE 1\n", current_time);
        }
        else {
          shm_lanes_p->type = ARRIVAL;
          shm_lanes_p->t_end_2 = arrivals_queue_node_p->t_end;
          new_lane2 = flight_id;
          scheduling_complete = 1;
          if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - FILLING LANE 2\n", current_time);
        }

        free(arrivals_queue_node_p);
      }
      else {
        char ACCPET_DEP = 0;
        int time_to_takeoff = 0;

        if (departures_queue_node_p != NULL) {
          time_to_takeoff = departures_queue_node_p->t_end - departures_queue_node_p->t_start;
          char DEP_COND1 = departures_queue_node_p->t_start <= current_time;
          if (DEBUG_SCHEDULER) printf("DEP_COND1: %d %d\n", DEP_COND1, departures_queue_node_p->t_start <= current_time);
          char DEP_COND2_1 = shm_lanes_p->type == DEPARTURE && (shm_lanes_p->t_end_1 < current_time || shm_lanes_p->t_end_2 < current_time);
          if (DEBUG_SCHEDULER) printf("DEP_COND2_1: %d\n", DEP_COND2_1);
          char DEP_COND2_2 = shm_lanes_p->t_end_1 < current_time && shm_lanes_p->t_end_2  < current_time;
          if (DEBUG_SCHEDULER) printf("DEP_COND2_2: %d\n", DEP_COND2_2);
          char DEP_COND2 = DEP_COND2_1 || DEP_COND2_2;
          if (DEBUG_SCHEDULER) printf("DEP_COND2: %d\n", DEP_COND2);
          char DEP_COND3 = 1;
          if (arrivals_queue_node_p != NULL) DEP_COND3 = current_time + time_to_takeoff < arrivals_queue_node_p->t_start;
          if (DEBUG_SCHEDULER) printf("DEP_COND3: %d\n", DEP_COND3);
          ACCPET_DEP = DEP_COND1 && DEP_COND2 && DEP_COND3;
        }

        if (DEBUG_SCHEDULER) printf("ACCPET_DEP: %d\n", ACCPET_DEP);
        if (ACCPET_DEP) {
          sem_wait(shm_stats_sem_p);
          shm_stats_p->departure_total_waiting_time += current_time - departures_queue_node_p->t_start;
          sem_post(shm_stats_sem_p);
        
          queue_pop(departures_queue_p, NULL);
          int flight_id = departures_queue_node_p->flight_id;

          if (shm_lanes_p->t_end_1 < current_time) {
            shm_lanes_p->type = DEPARTURE;
            shm_lanes_p->t_end_1 = current_time + time_to_takeoff;
            new_lane1 = flight_id;
            if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - FILLING LANE 1\n", current_time);
          }
          else {
            shm_lanes_p->type = DEPARTURE;
            shm_lanes_p->t_end_2 = current_time + time_to_takeoff;
            new_lane2 = flight_id;
            scheduling_complete = 1;
            if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - FILLING LANE 2\n", current_time);
          }

          free(departures_queue_node_p);
        }
        else {
          scheduling_complete = 1;
        }
      }
    }

    // CHECK IF HOLDING POSSIBLE
    queue_node_t* arrivals_queue_node_p = arrivals_queue_p->head;
    while (arrivals_queue_node_p != NULL && arrivals_queue_node_p->t_start == current_time) {
      int fuel_to_land = arrivals_queue_node_p->t_end - config_p->dt;
      int fuel_left = arrivals_queue_node_p->t_fuel - fuel_to_land;
      int holding = possible_holding(fuel_left);
      if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - POSSIBLE HOLDING: %d\n", current_time, holding);
      queue_pop(arrivals_queue_p, NULL);

      if (holding == -1) {
        sem_wait(shm_stats_sem_p);
        shm_stats_p->arrival_redirects += 1;
        sem_post(shm_stats_sem_p);

        tower_msg_t message;
        message.msg_type = arrivals_queue_node_p->flight_id;
        message.action.type = REDIRECT;
        message.action.fuel_left = fuel_left;
        msgsnd(tower_message_queue_id, &message, sizeof(action_t), 0);
      }
      else {
        sem_wait(shm_stats_sem_p);
        shm_stats_p->arrival_holdings += 1;
        shm_stats_p->arrival_total_waiting_time += holding;
        sem_post(shm_stats_sem_p);

        tower_msg_t message;
        int flight_id = arrivals_queue_node_p->flight_id;
        int t_start = arrivals_queue_node_p->t_start + holding;
        int t_end = arrivals_queue_node_p->t_end + holding;
        int t_fuel = arrivals_queue_node_p->t_fuel;

        message.msg_type = arrivals_queue_node_p->flight_id;
        message.action.type = HOLD;
        message.action.holding_time = holding;
        message.action.fuel_left = fuel_left;
        msgsnd(tower_message_queue_id, &message, sizeof(action_t), 0);
        arrivals_queue_node_p->t_start += holding;
        arrivals_queue_node_p->t_end += holding;
        queue_add_arrival(arrivals_queue_p, flight_id, t_start, t_end, t_fuel, NULL);
      }

      free(arrivals_queue_node_p);
      arrivals_queue_node_p = arrivals_queue_p->head;
    }

    if (new_lane1) {
      if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - EXECUTING FLIGHT %d\n", current_time, new_lane1);
      tower_msg_t message;
      message.msg_type = new_lane1;
      message.action.type = EXECUTE;
      message.action.lane_used = 1;
      msgsnd(tower_message_queue_id, &message, sizeof(action_t), 0);
    }

    if (new_lane2) {
      if (DEBUG_SCHEDULER) printf("T: %d - SCHEDULER - EXECUTING FLIGHT %d\n", current_time, new_lane2);
      tower_msg_t message;
      message.msg_type = new_lane2;
      message.action.type = EXECUTE;
      message.action.lane_used = 2;
      msgsnd(tower_message_queue_id, &message, sizeof(action_t), 0);
    }

    sem_post(shm_lanes_sem_p);
    sem_post(queue_sem_p);
    ++current_time;
  }

  pthread_exit(&ret);
}