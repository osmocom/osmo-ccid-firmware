CFLAGS=-Wall -g
LIBS?=-lasan $(shell pkg-config --libs libosmocore)

all: ccid_functionfs hub_functionfs cuart_test cuart_fsm_test

ccid_functionfs: ccid_main_functionfs.o logging.o ccid_proto.o ccid_device.o ccid_slot_sim.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -laio

hub_functionfs: hub_main_functionfs.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -laio

cuart_test: cuart_test.o cuart.o cuart_driver_tty.o utils_ringbuffer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

cuart_fsm_test: cuart_fsm_test.o logging.o cuart.o cuart_driver_tty.o utils_ringbuffer.o iso7816_fsm.o
	$(CC) -o $@ $^ $(LIBS) $(shell pkg-config --libs libosmosim)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid_functionfs hub_functionfs cuart_test cuart_fsm_test *.o
