#include "config.h"
#include "utils.h"
#include "flight_list.h"
#include "status.h"
#include "shm.h"

int main();
int possible_holding(int);
void print_stats();

// CREATORS
void init_globals();
int create_pipe_reader_thread();
int create_semaphores();

// DESTROYERS
void shutdown();
void shutdown_process();

// PROCESSES
void control_tower();
void simulation_manager();

// THREADS
void* pipe_reader();
void* arrival_flight(void*) ;
void* departure_flight(void*);
void* scheduler();