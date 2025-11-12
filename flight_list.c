#include "flight_list.h"
#include <stdlib.h>

/* FLIGHT NODE */
flight_node_t* flight_node_init(flight_t flight) {
  flight_node_t* new_node = (flight_node_t*)malloc(sizeof(flight_node_t));
  new_node->flight = flight;
  new_node->next = NULL;
  return new_node;
}

void flight_node_free(flight_node_t* flight_node_p) {
  if (flight_node_p != NULL) {
    flight_node_free(flight_node_p->next);
    free(flight_node_p);
  }
}

/* FLIGHT LIST */
flight_list_t* flight_list_init() {
  flight_list_t* new_list = (flight_list_t*) malloc(sizeof(flight_list_t));
  new_list->size = 0;
  new_list->head = NULL;
  return new_list;
}

void flight_list_add(flight_list_t* flight_list_p, flight_t flight, sem_t* flight_list_sem_p) {
  if (flight_list_sem_p != NULL) sem_wait(flight_list_sem_p);

  if (flight_list_p != NULL) {
    flight_node_t* new_node = flight_node_init(flight);
    flight_list_p->size += 1;

    if (flight_list_p->head == NULL) {
      flight_list_p->head = new_node;
    } else {
      flight_node_t* aux = flight_list_p->head;
      while (aux->next != NULL) aux = aux->next;
      aux->next = new_node;
    }
  }
  
  if (flight_list_sem_p != NULL)  sem_post(flight_list_sem_p);
}

void flight_list_free(flight_list_t* flight_list_p, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);

  if (flight_list_p != NULL) {
    flight_node_free(flight_list_p->head);
    free(flight_list_p);
  }

  if (sem_p != NULL)  sem_post(sem_p);
}

flight_t flight_list_pop_by_init(flight_list_t* flight_list_p, int init, sem_t* flight_list_sem_p) {
  if (flight_list_sem_p != NULL) sem_wait(flight_list_sem_p);

  flight_t flight = flight_init();

  if (flight_list_p != NULL) {
    if (flight_list_p->head != NULL) {
      if (flight_list_p->head->flight.init == init) {
        flight_node_t* node_to_delete = flight_list_p->head;
        flight = flight_list_p->head->flight;
        flight_list_p->head = node_to_delete->next;
        free(node_to_delete);
      }
      else {
        flight_node_t* aux = flight_list_p->head;
        while (aux->next != NULL && aux->next->flight.init != init) aux = aux->next;
        if (aux->next != NULL) {
          flight_node_t* node_to_delete = aux->next;
          flight = aux->next->flight;
          aux->next = node_to_delete->next;
          free(node_to_delete);
        }
      }
    }
  }

  if (flight_list_sem_p != NULL) sem_post(flight_list_sem_p);

  return flight;
}