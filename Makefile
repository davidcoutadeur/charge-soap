src=$(wildcard *.c)
obj=$(src:.c=.o)

CC=gcc
CFLAGS=-Wall -D_REENTRANT -I. -I/usr/local/lib
LDFLAGS=-lcurl -lpthread -lrt


charge-soap: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) charge-soap
