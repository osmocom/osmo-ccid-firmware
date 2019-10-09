CFLAGS=-Wall -g -I../ccid_common -I.
LIBS?=-lasan $(shell pkg-config --libs libosmocore)

all: ccid_functionfs hub_functionfs cuart_test cuart_fsm_test

ccid_functionfs: ccid_main_functionfs.o \
		 logging.o \
		 utils_ringbuffer.o \
		 cuart_driver_tty.o \
		 ../ccid_common/ccid_proto.o \
		 ../ccid_common/ccid_device.o \
		 ../ccid_common/cuart.o \
		 ../ccid_common/ccid_slot_fsm.o \
		 ../ccid_common/iso7816_fsm.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -laio

hub_functionfs: hub_main_functionfs.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -laio

cuart_test: cuart_test.o \
		 cuart_driver_tty.o \
		 utils_ringbuffer.o \
		 ../ccid_common/cuart.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

cuart_fsm_test: cuart_fsm_test.o \
		 logging.o \
		 cuart_driver_tty.o \
		 utils_ringbuffer.o \
		 ../ccid_common/cuart.o \
		 ../ccid_common/iso7816_fsm.o
	$(CC) -o $@ $^ $(LIBS) $(shell pkg-config --libs libosmosim)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid_functionfs hub_functionfs cuart_test cuart_fsm_test *.o ../ccid_common/*.o
