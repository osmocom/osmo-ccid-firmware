CFLAGS=-Wall -g `pkg-config --cflags libusb-1.0`

LDFLAGS=-lasan `pkg-config --libs libusb-1.0`

ccid-control: ccid-control.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm ccid-control *.o
