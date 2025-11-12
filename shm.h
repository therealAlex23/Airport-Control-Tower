#include "status.h"
#include "flight.h"
#include "config.h"

#include <semaphore.h>

typedef enum { SHM_LANES, SHM_STATS } shm_type_t;

typedef struct {
  flight_type_t type;
  int t_end_1;
  int t_end_2;
} shm_lanes_t;

typedef struct {
  int arrival_flights;
  int arrival_holdings;
  int arrival_total_waiting_time;
  int arrival_redirects;
  int departure_flights;
  int departure_total_waiting_time;
  int rejected_flights;
} shm_stats_t;

int get_shm(int*, shm_type_t, status_t, status_t*);
int attach_shm(void**, int, status_t, status_t*);

void shm_lanes_init(shm_lanes_t*);
void shm_stats_init(shm_stats_t*);