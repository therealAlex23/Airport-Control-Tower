#include "flight.h"
#include "utils.h"

#include <stdio.h>

flight_t flight_init() {
  flight_t flight;
  flight.type = NOT_FOUND;
  flight.id = 0;
  flight.number = 0;
  flight.init = 0;
  flight.takeoff = 0;
  flight.eta = 0;
  flight.fuel = 0;
  return flight;
}