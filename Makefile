CC = gcc
DESTDIR = /usr/local/bin

toggler: main.c
	$(CC) -Wall -O3 -o toggler main.c -lsystemd

toggler-dbg: main.c
	$(CC) -Wall -g -o toggler-dbg main.c -lsystemd

.PHONY: install
install: toggler
	install -m755 toggler -D $(DESTDIR)/toggler
