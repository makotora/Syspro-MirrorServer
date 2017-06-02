OBJS1 = ContentServer.o MirrorServer.o MirrorInitiator.o
OBJS2 = functions.o structs.o Protocol.o
B = Buffer.o
SOURCE = MirrorInitiator.c ContentServer.c MirrorServer.c functions.c Protocol.c structs.c Buffer.c
HEADER = functions.h Protocol.h structs.h Buffer.h
CC = gcc
CFLAGS = -c -Wall
LFLAGS = -Wall
SFLAGS = -lsocket -lnsl
OUT = MirrorServer ContentServer MirrorInitiator

all: MirrorInitiator MirrorServer ContentServer

MirrorInitiator: MirrorInitiator.o functions.o Protocol.o
	$(CC) $(LFLAGS) functions.o Protocol.o MirrorInitiator.o -o MirrorInitiator

MirrorServer: MirrorServer.o $(OBJS2) $(B)
	$(CC) $(LFLAGS) $(OBJS2) $(B) MirrorServer.o -o MirrorServer -lpthread

ContentServer: ContentServer.o $(OBJS2)
	$(CC) $(LFLAGS) $(OBJS2) ContentServer.o -o ContentServer -lpthread


MirrorInitiator.o: MirrorInitiator.c
	$(CC) $(CFLAGS) $(SFLAGS) MirrorInitiator.c

MirrorServer.o: MirrorServer.c
	$(CC) $(CFLAGS) $(SFLAGS) MirrorServer.c

ContentServer.o: ContentServer.c
	$(CC) $(CFLAGS) $(SFLAGS) ContentServer.c

Buffer.o: Buffer.c
	$(CC) $(CFLAGS) $(SFLAGS) Buffer.c

Protocol.o: Protocol.c
	$(CC) $(CFLAGS) $(SFLAGS) Protocol.c

functions.o: functions.c
	$(CC) $(CFLAGS) functions.c

structs.o: structs.c
	$(CC) $(CFLAGS) structs.c	

clean:
	rm -rf $(OBJS1) $(OBJS2) $(B) $(OUT)

count:
	wc $(SOURCE) $(HEADER)