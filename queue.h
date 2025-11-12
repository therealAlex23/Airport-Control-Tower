#include "flight.h"

#include <semaphore.h>

typedef struct queue_node {
  int flight_id;
  int t_start;
  int t_end;
  int t_fuel;
  struct queue_node* next;
} queue_node_t;

typedef struct {
  int size;
  queue_node_t* head;
} queue_t;

queue_node_t* queue_node_init(int, int, int, int);
void queue_node_free(queue_node_t*);

queue_t* queue_init();
void queue_free(queue_t*, sem_t*);
int queue_size(queue_t*, sem_t*);
void queue_add_departure(queue_t*, int, int, int, sem_t*);
void queue_add_arrival(queue_t*, int, int, int, int, sem_t*);
queue_node_t* queue_pop(queue_t*, sem_t*);
void queue_print(queue_t*, const char*, sem_t*);