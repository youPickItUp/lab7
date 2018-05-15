CC = gcc
CFLAGS = -Wall -std=c99 
CMACRO = -D_GNU_SOURCE 
LIB_LINK = -lpthread -lrt 

#SIGTERM = 15
SIGNAL= -15

.PHONY: all
all: clean barber clients

barber: barber.c
	$(CC) -o $@ $^ $(CFLAGS) $(CMACRO) $(LIB_LINK)

clients: clients.c
	$(CC) -o $@ $^ $(CFLAGS) $(CMACRO) $(LIB_LINK)

.PHONY: clean
clean:
	rm -f barber clients

.PHONY: kill_barber
kill_barber:
	kill $(SIGNAL) `pidof barber`
