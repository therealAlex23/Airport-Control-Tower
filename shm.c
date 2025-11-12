#include "shm.h"
#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdio.h>

int get_shm(int* shmid_p, shm_type_t shm_type, status_t success_status, status_t* status_p) {
  switch (shm_type) {
    case SHM_LANES:
      *shmid_p = shmget(IPC_PRIVATE, sizeof(shm_lanes_t), IPC_CREAT|0777);
      break;
    case SHM_STATS:
      *shmid_p = shmget(IPC_PRIVATE, sizeof(shm_stats_t), IPC_CREAT|0777);
      break;
  }

  if (*shmid_p == -1){
    if (DEBUG_RESOURCES) perror("Failed to create shared memory");
    return 0;
  } else {
    *status_p = success_status;
  }

  return 1;
}

int attach_shm(void** shm_p_p, int shmid, status_t success_status, status_t* status_p) {
  *shm_p_p = shmat(shmid, NULL, 0);

  if (*shm_p_p == (void*)-1){
    return 0;
  } else {
    *status_p = success_status;
  }

  return 1;
}

void shm_lanes_init(shm_lanes_t* shm_lanes_p) {
  shm_lanes_p->type = NOT_FOUND;
  shm_lanes_p->t_end_1 = -1;
  shm_lanes_p->t_end_2 = -1;
}

void shm_stats_init(shm_stats_t* shm_stats_p) {
  shm_stats_p->arrival_flights = 0;
  shm_stats_p->arrival_total_waiting_time = 0;
  shm_stats_p->arrival_holdings = 0;
  shm_stats_p->arrival_redirects = 0;
  shm_stats_p->departure_flights = 0;
  shm_stats_p->departure_total_waiting_time = 0;
  shm_stats_p->rejected_flights = 0;
}