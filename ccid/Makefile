CFLAGS=-Wall -g

ccid_functionfs: ccid_main_functionfs.o ccid_proto.o ccid_device.o ccid_slot_sim.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc -laio

hub_functionfs: hub_main_functionfs.o
	$(CC) $(CFLAGS) -o $@ $^ -lasan -losmocore -ltalloc -laio

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid_functionfs *.o
