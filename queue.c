#include "queue.h"

#include <stdlib.h>
#include <stdio.h>

queue_node_t* queue_node_init(int id, int t_start, int t_end, int t_fuel) {
  queue_node_t* queue_node_p = (queue_node_t*)malloc(sizeof(queue_node_t));
  queue_node_p->flight_id = id;
  queue_node_p->t_start = t_start;
  queue_node_p->t_end = t_end;
  queue_node_p->t_fuel = t_fuel;
  return queue_node_p;
}

void queue_node_free(queue_node_t* queue_node_p) {
  if (queue_node_p == NULL) return;
  queue_node_free(queue_node_p->next);
  free(queue_node_p);
}

queue_t* queue_init() {
  queue_t* queue_p = (queue_t*)malloc(sizeof(queue_t));
  queue_p->size = 0;
  queue_p->head = NULL;
  return queue_p;
}

void queue_free(queue_t* queue_p, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);
  if (queue_p != NULL) {
    queue_node_free(queue_p->head);
    free(queue_p);
  }
  if (sem_p != NULL) sem_post(sem_p);
}

int queue_size(queue_t* queue_p, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);
  int size = 0;
  if (queue_p != NULL) size = queue_p->size;
  if (sem_p != NULL) sem_post(sem_p);
  return size;
}

void queue_add_departure(queue_t* queue_p, int id, int t_start, int t_end, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);
  if (queue_p == NULL) {
    if (sem_p != NULL) sem_post(sem_p);
    return;
  }

  queue_node_t* new_node_p = queue_node_init(id, t_start, t_end, 0);
  
  if (queue_p->head == NULL) {
    queue_p->head = new_node_p;
  }
  else if (t_start < queue_p->head->t_start) {
    queue_node_t* aux_p = queue_p->head;
    queue_p->head = new_node_p;
    new_node_p->next = aux_p;
  }
  else {
    queue_node_t* prev_node_p = queue_p->head;
    queue_node_t* curr_node_p = queue_p->head->next;

    while (curr_node_p != NULL) {
      if (t_start < curr_node_p->t_start) {
        new_node_p->next = curr_node_p;
        break;
      }
      prev_node_p = curr_node_p;
      curr_node_p = curr_node_p->next;
    }

    prev_node_p->next = new_node_p;
  }

  queue_p->size += 1;

  if (sem_p != NULL) sem_post(sem_p);
}

void queue_add_arrival(queue_t* queue_p, int id, int t_start, int t_end, int t_fuel, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);
  if (queue_p == NULL) {
    if (sem_p != NULL) sem_post(sem_p);
    return;
  }

  queue_node_t* new_node_p = queue_node_init(id, t_start, t_end, t_fuel);
  
  if (queue_p->head == NULL) {
    queue_p->head = new_node_p;
  }
  else if (t_start < queue_p->head->t_start) {
    queue_node_t* aux_p = queue_p->head;
    queue_p->head = new_node_p;
    new_node_p->next = aux_p;
  }
  else {
    queue_node_t* prev_node_p = queue_p->head;
    queue_node_t* curr_node_p = queue_p->head->next;

    while (curr_node_p != NULL) {
      if (t_start < curr_node_p->t_start || (t_start == curr_node_p->t_fuel && t_fuel < curr_node_p->t_fuel)) {
        new_node_p->next = curr_node_p;
        break;
      }
      prev_node_p = curr_node_p;
      curr_node_p = curr_node_p->next;
    }

    prev_node_p->next = new_node_p;
  }

  queue_p->size += 1;

  if (sem_p != NULL) sem_post(sem_p);
}

queue_node_t* queue_pop(queue_t* queue_p, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);

  queue_node_t* queue_node_p = NULL;

  if (queue_p != NULL) {
    queue_node_p = queue_p->head;

    if (queue_node_p != NULL) {
      queue_p->size -= 1;
      queue_p->head = queue_node_p->next;
      queue_node_p->next = NULL;
    }
  }

  if (sem_p != NULL) sem_post(sem_p);

  return queue_node_p;
}

void queue_print(queue_t* queue_p, const char* label, sem_t* sem_p) {
  if (sem_p != NULL) sem_wait(sem_p);

  if (label == NULL) {
    printf("--- QUEUE ---\n");
  }
  else {
    printf("--- %s QUEUE ---\n", label);
  }
  if (queue_p != NULL) {
    queue_node_t* queue_node_p = queue_p->head;
    while (queue_node_p != NULL) {
      printf("FLIGHT_ID: %d T_START: %d T_END: %d T_FUEL: %d\n", queue_node_p->flight_id, queue_node_p->t_start, queue_node_p->t_end, queue_node_p->t_fuel);
      queue_node_p = queue_node_p->next;
    }
  }
  printf("--- END QUEUE ---\n");

  if (sem_p != NULL) sem_post(sem_p);
}