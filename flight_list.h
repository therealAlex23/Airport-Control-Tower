#ifndef FLIGHT_LIST_H
#define FLIGHT_LIST_H

#include <semaphore.h>
#include "flight.h"

typedef struct flight_node {
  flight_t flight;
  struct flight_node* next;
} flight_node_t;

typedef struct {
  int size;
  flight_node_t* head;
} flight_list_t;

#include "flight_list.h"
#include <stdlib.h>

/* FLIGHT NODE */
flight_node_t* flight_node_init(flight_t );
void flight_node_free(flight_node_t*) ;

/* FLIGHT LIST */
flight_list_t* flight_list_init();
void flight_list_add(flight_list_t*, flight_t, sem_t*);
void flight_list_free(flight_list_t*, sem_t*);
flight_t flight_list_pop_by_init(flight_list_t*, int, sem_t*);
#endif