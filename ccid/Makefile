CFLAGS=-Wall -g

ccid_functionfs: ccid_main_functionfs.o ccid_proto.o ccid_device.o
	$(CC) $(CFLAGS) -lasan -losmocore -ltalloc -laio -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid_functionfs *.o
