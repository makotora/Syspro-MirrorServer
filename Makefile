OBJS1 = ContentServer.o MirrorServer.o
OBJS2 = functions.o structs.o Protocol.o
B = Buffer.o
SOURCE = ContentServer.c MirrorServer.c functions.c Protocol.c structs.c Buffer.c
HEADER = functions.h Protocol.h structs.h Buffer.h
CC = gcc
CFLAGS = -c -Wall
LFLAGS = -Wall
SFLAGS = -lsocket -lnsl
OUT = MirrorServer, ContentServer

all: MirrorServer ContentServer


MirrorServer: MirrorServer.o $(OBJS2) $(B)
	$(CC) $(LFLAGS) $(OBJS2) $(B) MirrorServer.o -o MirrorServer -lpthread

ContentServer: ContentServer.o $(OBJS2)
	$(CC) $(LFLAGS) $(OBJS2) ContentServer.o -o ContentServer -lpthread


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