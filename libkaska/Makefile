CFLAGS=-Wall -g -fPIC -I../util

all: libutil libkaska.so

libutil:
	$(MAKE) -C ../util

libkaska.so: kaska_client_lib.o comun.o libutil.so
	$(CC) $(CFLAGS) -shared -o $@ $< comun.o ./libutil.so 

kaska_client_lib.o: comun.h  kaska.h

clean:
	rm -f *.o libkaska.so


