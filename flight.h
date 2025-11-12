#ifndef FLIGHT_H
#define FLIGHT_H

typedef enum { ARRIVAL, DEPARTURE, NOT_FOUND } flight_type_t;

typedef struct {
  flight_type_t type;
  int id ;
  int number;
  int init;
  int takeoff;
  int eta;
  int fuel;
} flight_t;

flight_t flight_init();

#endif