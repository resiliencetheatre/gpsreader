CC=gcc

EXTRA_WARNINGS=-Wall

CFLAGS=$(EXTRA_WARNINGS) -lgps -lm

BINS=gpsreader

gpsreader:	gpsreader.c
	 $(CC) $+ $(CFLAGS)  -o $@ -lgps -lm -I.

clean:
	rm -rf $(BINS)

