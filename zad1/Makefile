CC= gcc
CFLAGS= -Wall -std=c99
CMACRO= -D_GNU_SOURCE

#SIGTERM = 15
SIGNAL= -15

.PHONY: all
all: clean barber clients

barber: barber.c
	$(CC) -o $@ $^ $(CFLAGS) $(CMACRO)

clients: clients.c
	$(CC) -o $@ $^ $(CFLAGS) $(CMACRO)

.PHONY: clean
clean:
	rm -f barber clients

.PHONY: kill_barber
kill_barber:
	kill $(SIGNAL) `pidof barber`
