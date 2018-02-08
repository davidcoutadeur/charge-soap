src=$(wildcard *.c)
obj=$(src:.c=.o)

CC=gcc
CFLAGS=-I.
LDFLAGS=-lcurl -lpthread -lrt


charge-soap: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) charge-soap
