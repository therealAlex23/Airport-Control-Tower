make:
	gcc main.c utils.c flight.c flight_list.c shm.c queue.c -Wall -o main -lpthread
