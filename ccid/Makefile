

ccid_descriptors: ccid_descriptors.o ccid_proto.o
	$(CC) -lasan -losmocore -laio -o $@ $^

%.o: %.c
	$(CC) -o $@ -c $^

clean:
	rm ccid_descriptors *.o
