CC = gcc
CFLAG = -g -o

all: obs

obs.o : obs.c
	$(CC) -c obs.c
obs: obs.o 
	$(CC) $(CFLAG) $@ $?
