CFLAGS=-Wall -g -I../util

all: libutil broker

libutil:
	$(MAKE) -C ../util

broker.o: comun.h
comun.o: comun.h

broker: broker.o comun.o libutil.so
	$(CC) -o $@ $< comun.o -lpthread ./libutil.so -Wall

clean:
	rm -f *.o broker
