CFLAGS=-Wall -g -I../ccid_common -I.

all: ccid_functionfs hub_functionfs cuart_test

ccid_functionfs: ccid_main_functionfs.o \
		 ccid_slot_sim.o \
		 logging.o \
		 ../ccid_common/ccid_proto.o \
		 ../ccid_common/ccid_device.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc -laio

hub_functionfs: hub_main_functionfs.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc -laio

cuart_test:	cuart_test.o \
		cuart_driver_tty.o \
		utils_ringbuffer.o \
		../ccid_common/cuart.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -f ccid_functionfs hub_functionfs cuart_test *.o
