CFLAGS=-Wall -g

all: ccid_functionfs hub_functionfs cuart_test

ccid_functionfs: ccid_main_functionfs.o logging.o ccid_proto.o ccid_device.o ccid_slot_sim.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc -laio

hub_functionfs: hub_main_functionfs.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc -laio

cuart_test: cuart_test.o cuart.o cuart_driver_tty.o utils_ringbuffer.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid_functionfs hub_functionfs cuart_test *.o
