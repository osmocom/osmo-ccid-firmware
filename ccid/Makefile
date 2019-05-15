CFLAGS=-Wall -g

ccid_descriptors: ccid_descriptors.o ccid_proto.o ccid_device.o
	$(CC) $(CFLAGS) -lasan -losmocore -laio -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid_descriptors *.o
