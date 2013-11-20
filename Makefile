CC=gcc
CFLAGS=-Wall

.PHONY: all
all: l2cap_proxy

.PHONY: clean
clean:
	rm -f l2cap_proxy *~ *.o

l2cap_proxy: l2cap_proxy.o l2cap_con.o bt_utils.o
	$(CC) -o $@ $^ -lbluetooth

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)
	
